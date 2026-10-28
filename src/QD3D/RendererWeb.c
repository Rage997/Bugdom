// WebGL2 backend adapted from the Nanosaur web port.
#ifdef __EMSCRIPTEN__
#include "game.h"
#include "rendererweb.h"

extern TQ3Matrix4x4 gCameraWorldToViewMatrix, gCameraViewToFrustumMatrix;
extern TQ3Param2D gEnvMapUVs[];

#define GPU_MESH_CAPACITY 8192
#define TOMBSTONE ((const TQ3TriMeshData*)(uintptr_t)1)
typedef struct
{
    const TQ3TriMeshData* mesh;
    GLuint vao, buffers[6];
    size_t capacities[6];
    bool wireDirty;
    unsigned dirty;
} GPUMesh;
static GPUMesh meshes[GPU_MESH_CAPACITY];
static GLuint program, lineVAO, lineBuffer, environmentBuffer;
static TQ3Matrix4x4 projections[8];
static unsigned projectionDepth;
static float fogStart, fogEnd;
static TQ3ColorRGBA fogColor;
static TQ3Point3D* linePoints;
static size_t lineCount, lineCapacity, lineGPUCapacity;
static GLenum lineMode;
static uint32_t* wireIndices;
static size_t wireCapacity;
static size_t environmentCapacity;
static uint8_t* convertedPixels;
static size_t convertedCapacity;
static bool sceneDirty;
static struct
{
    GLint modelView, projection, normal, color, textured, lit, cutout, fog;
    GLint ambient, lightDirection, lightColor, lightCount, fogRange, fogColor;
} uniforms;

static const char* vertexShader =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location=0) in vec3 position; layout(location=1) in vec3 normal;\n"
    "layout(location=2) in vec2 uv; layout(location=3) in vec4 vertexColor;\n"
    "uniform mat4 modelView, projection, normalMatrix;\n"
    "uniform vec4 color; uniform bool lit; uniform vec3 ambient;\n"
    "uniform vec3 lightDirection[4], lightColor[4]; uniform int lightCount;\n"
    "out vec2 texCoord; out vec4 shade; out float distanceToEye;\n"
    "void main(){ vec4 eye=modelView*vec4(position,1.0);\n"
    "gl_Position=projection*eye; distanceToEye=abs(eye.z); texCoord=uv;\n"
    "shade=color*vertexColor; if(lit){ vec3 n=normalize(mat3(normalMatrix)*normal);\n"
    "vec3 lighting=ambient; for(int i=0;i<4;i++){if(i<lightCount)\n"
    "lighting+=lightColor[i]*max(dot(n,lightDirection[i]),0.0);}\n"
    "shade.rgb=clamp(shade.rgb*lighting,0.0,1.0); }}\n";
static const char* fragmentShader =
    "#version 300 es\nprecision highp float;\n"
    "in vec2 texCoord; in vec4 shade; in float distanceToEye;\n"
    "uniform sampler2D image; uniform bool textured, cutout, fog;\n"
    "uniform vec2 fogRange; uniform vec3 fogColor; out vec4 outputColor;\n"
    "void main(){ vec4 c=shade; if(textured)c*=texture(image,texCoord);\n"
    "if(cutout && c.a<=0.4999)discard; if(fog){float f=clamp(\n"
    "(fogRange.y-distanceToEye)/(fogRange.y-fogRange.x),0.0,1.0);\n"
    "c.rgb=mix(fogColor,c.rgb,f);} outputColor=c;}\n";

static GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        DoFatalAlert("WebGL shader compilation failed: %s", log);
    }
    return shader;
}

void RenderWeb_Init(void)
{
    if (program) return;
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexShader);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        DoFatalAlert("WebGL shader linking failed: %s", log);
    }
#define UNIFORM(field, name) uniforms.field = glGetUniformLocation(program, name)
    UNIFORM(modelView, "modelView"); UNIFORM(projection, "projection");
    UNIFORM(normal, "normalMatrix"); UNIFORM(color, "color");
    UNIFORM(textured, "textured"); UNIFORM(lit, "lit");
    UNIFORM(cutout, "cutout"); UNIFORM(fog, "fog");
    UNIFORM(ambient, "ambient"); UNIFORM(lightDirection, "lightDirection");
    UNIFORM(lightColor, "lightColor"); UNIFORM(lightCount, "lightCount");
    UNIFORM(fogRange, "fogRange"); UNIFORM(fogColor, "fogColor");
#undef UNIFORM
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineBuffer);
    glGenBuffers(1, &environmentBuffer);
}

static GPUMesh* FindMesh(const TQ3TriMeshData* mesh, bool create)
{
    unsigned index = ((uintptr_t)mesh >> 4) % GPU_MESH_CAPACITY;
    GPUMesh* vacant = NULL;
    for (unsigned i = 0; i < GPU_MESH_CAPACITY; i++, index = (index+1)%GPU_MESH_CAPACITY)
    {
        GPUMesh* slot = &meshes[index];
        if (slot->mesh == mesh) return slot;
        if (slot->mesh == TOMBSTONE && !vacant) vacant = slot;
        if (!slot->mesh)
        {
            if (!vacant) vacant = slot;
            break;
        }
    }
    if (!create) return NULL;
    GAME_ASSERT_MESSAGE(vacant, "WebGL mesh cache exhausted");
    vacant->mesh = mesh;
    vacant->dirty = kWebMeshAll;
    glGenVertexArrays(1, &vacant->vao);
    glGenBuffers(6, vacant->buffers);
    vacant->wireDirty = true;
    return vacant;
}

void RenderWeb_InvalidateMesh(const TQ3TriMeshData* mesh, unsigned mask)
{
    GPUMesh* gpu = FindMesh(mesh, false);
    if (gpu) gpu->dirty |= mask;
}
void RenderWeb_ReleaseMesh(const TQ3TriMeshData* mesh)
{
    GPUMesh* gpu = FindMesh(mesh, false);
    if (!gpu) return;
    glDeleteVertexArrays(1, &gpu->vao);
    glDeleteBuffers(6, gpu->buffers);
    *gpu = (GPUMesh){.mesh = TOMBSTONE};
}
void RenderWeb_ClearMeshes(void)
{
    for (unsigned i = 0; i < GPU_MESH_CAPACITY; i++)
        if (meshes[i].mesh && meshes[i].mesh != TOMBSTONE)
            RenderWeb_ReleaseMesh(meshes[i].mesh);
    SDL_memset(meshes, 0, sizeof(meshes));
}

void RenderWeb_BeginFrame(void)
{
    sceneDirty = true;
}

void RenderWeb_Shutdown(void)
{
    RenderWeb_ClearMeshes();
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineBuffer);
    SDL_free(linePoints);
    SDL_free(wireIndices);
    linePoints = NULL;
    wireIndices = NULL;
    lineCapacity = lineGPUCapacity = wireCapacity = 0;
    glDeleteBuffers(1, &environmentBuffer);
    SDL_free(convertedPixels);
    convertedPixels = NULL;
    convertedCapacity = environmentCapacity = 0;
    program = lineVAO = lineBuffer = environmentBuffer = 0;
}

static void UploadBuffer(GLenum target, size_t* capacity, size_t size, const void* data)
{
    if (size > *capacity)
    {
        glBufferData(target, size, NULL, GL_DYNAMIC_DRAW);
        *capacity = size;
    }
    glBufferSubData(target, 0, size, data);
}

static void Attribute(GLuint buffer, size_t* capacity, GLuint attribute, int components,
                      int count, const void* data, bool upload)
{
    if (!data)
    {
        glDisableVertexAttribArray(attribute);
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    size_t size = (size_t)count*components*sizeof(float);
    if (upload || size > *capacity) UploadBuffer(GL_ARRAY_BUFFER, capacity, size, data);
    glVertexAttribPointer(attribute, components, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attribute);
}

void RenderWeb_SetFog(float start, float end, TQ3ColorRGBA color)
{
    fogStart = start;
    fogEnd = end;
    fogColor = color;
    sceneDirty = true;
}

void RenderWeb_Enter2D(float left, float right, float bottom, float top)
{
    GAME_ASSERT(projectionDepth < 8);
    TQ3Matrix4x4* p = &projections[projectionDepth++];
    Q3Matrix4x4_SetIdentity(p);
    p->value[0][0] = 2 / (right-left);
    p->value[1][1] = 2 / (top-bottom);
    p->value[2][2] = -2.0f / 1000;
    p->value[3][0] = -(right+left) / (right-left);
    p->value[3][1] = -(top+bottom) / (top-bottom);
    p->value[3][2] = -1;
    sceneDirty = true;
}

void RenderWeb_Exit2D(void)
{
    GAME_ASSERT(projectionDepth > 0);
    projectionDepth--;
    sceneDirty = true;
}

static void SetMatrices(const TQ3Matrix4x4* transform, bool lit)
{
    TQ3Matrix4x4 view, modelView;
    if (projectionDepth) Q3Matrix4x4_SetIdentity(&view);
    else view = gCameraWorldToViewMatrix;
    if (transform) Q3Matrix4x4_Multiply(transform, &view, &modelView);
    else modelView = view;
    glUniformMatrix4fv(uniforms.modelView, 1, GL_FALSE, &modelView.value[0][0]);
    if (lit)
    {
        TQ3Matrix4x4 normal;
        Q3Matrix4x4_Invert(&modelView, &normal);
        Q3Matrix4x4_Transpose(&normal, &normal);
        glUniformMatrix4fv(uniforms.normal, 1, GL_FALSE, &normal.value[0][0]);
    }
    const TQ3Matrix4x4* projection = projectionDepth
        ? &projections[projectionDepth-1] : &gCameraViewToFrustumMatrix;
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, &projection->value[0][0]);
}

void RenderWeb_DrawMesh(const TQ3TriMeshData* mesh, const TQ3Matrix4x4* transform,
    const TQ3ColorRGBA* color, unsigned flags)
{
    GPUMesh* gpu = FindMesh(mesh, true);
    glUseProgram(program);
    glBindVertexArray(gpu->vao);
    Attribute(gpu->buffers[0], &gpu->capacities[0], 0, 3, mesh->numPoints, mesh->points, gpu->dirty & kWebMeshGeometry);
    Attribute(gpu->buffers[1], &gpu->capacities[1], 1, 3, mesh->numPoints, mesh->hasVertexNormals ? mesh->vertexNormals : NULL, gpu->dirty & kWebMeshGeometry);
    Attribute(gpu->buffers[2], &gpu->capacities[2], 2, 2, mesh->numPoints, mesh->vertexUVs, gpu->dirty & kWebMeshAttributes);
    Attribute(gpu->buffers[3], &gpu->capacities[3], 3, 4, mesh->numPoints, mesh->hasVertexColors ? mesh->vertexColors : NULL, gpu->dirty & kWebMeshAttributes);
    if (!(flags & kWebDrawNormals)) glDisableVertexAttribArray(1);
    if (!(flags & kWebDrawColors)) glDisableVertexAttribArray(3);
    glVertexAttrib3f(1, 0, 0, 1);
    glVertexAttrib4f(3, 1, 1, 1, 1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->buffers[4]);
    if ((gpu->dirty & kWebMeshTopology) || mesh->numTriangles*sizeof(*mesh->triangles) > gpu->capacities[4])
    {
        UploadBuffer(GL_ELEMENT_ARRAY_BUFFER, &gpu->capacities[4], mesh->numTriangles*sizeof(*mesh->triangles), mesh->triangles);
        gpu->wireDirty = true;
    }
    gpu->dirty = 0;
    if (flags & kWebDrawReflection)
    {
        glBindBuffer(GL_ARRAY_BUFFER, environmentBuffer);
        UploadBuffer(GL_ARRAY_BUFFER, &environmentCapacity, mesh->numPoints*sizeof(TQ3Param2D), gEnvMapUVs);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);
    }
    SetMatrices(transform, flags & kWebDrawLit);
    glUniform4fv(uniforms.color, 1, &color->r);
    glUniform1i(uniforms.lit, !!(flags & kWebDrawLit));
    glUniform1i(uniforms.textured, !!(flags & kWebDrawTextured));
    glUniform1i(uniforms.cutout, !!(flags & kWebDrawCutout));
    glUniform1i(uniforms.fog, !!(flags & kWebDrawFog));
    if (sceneDirty)
    {
        const QD3DLightDefType* lights = &gGameViewInfoPtr->lightList;
        glUniform3f(uniforms.ambient, lights->ambientBrightness*lights->ambientColor.r,
            lights->ambientBrightness*lights->ambientColor.g, lights->ambientBrightness*lights->ambientColor.b);
        GLfloat directions[12] = {0}, colors[12] = {0};
        GAME_ASSERT(lights->numFillLights <= 4);
        for (int i = 0; i < lights->numFillLights; i++)
        {
            TQ3Vector3D direction;
            Q3Vector3D_Transform(&lights->fillDirection[i], &gCameraWorldToViewMatrix, &direction);
            Q3Vector3D_Normalize(&direction, &direction);
            directions[i*3] = -direction.x; directions[i*3+1] = -direction.y; directions[i*3+2] = -direction.z;
            colors[i*3] = lights->fillColor[i].r*lights->fillBrightness[i];
            colors[i*3+1] = lights->fillColor[i].g*lights->fillBrightness[i];
            colors[i*3+2] = lights->fillColor[i].b*lights->fillBrightness[i];
        }
        glUniform1i(uniforms.lightCount, lights->numFillLights);
        glUniform3fv(uniforms.lightDirection, 4, directions);
        glUniform3fv(uniforms.lightColor, 4, colors);
        glUniform2f(uniforms.fogRange, fogStart, fogEnd);
        glUniform3fv(uniforms.fogColor, 1, &fogColor.r);
        sceneDirty = false;
    }
    if (gDebugMode == DEBUG_MODE_WIREFRAME)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->buffers[5]);
        if (gpu->wireDirty)
        {
            size_t size = mesh->numTriangles * 6 * sizeof(uint32_t);
            if (size > wireCapacity)
            {
                void* next = SDL_realloc(wireIndices, size);
                GAME_ASSERT(next);
                wireIndices = next;
                wireCapacity = size;
            }
            for (int i = 0; i < mesh->numTriangles; i++)
            {
                const uint32_t* t = mesh->triangles[i].pointIndices;
                for (int e = 0; e < 3; e++)
                {
                    wireIndices[i*6+e*2] = t[e];
                    wireIndices[i*6+e*2+1] = t[(e+1)%3];
                }
            }
            UploadBuffer(GL_ELEMENT_ARRAY_BUFFER, &gpu->capacities[5], size, wireIndices);
            gpu->wireDirty = false;
        }
        glDrawElements(GL_LINES, mesh->numTriangles*6, GL_UNSIGNED_INT, 0);
    }
    else glDrawElements(GL_TRIANGLES, mesh->numTriangles*3, GL_UNSIGNED_INT, 0);
    CHECK_GL_ERROR();
}

void Render_BeginLines(GLenum mode)
{
    GAME_ASSERT(mode == GL_LINES || mode == GL_LINE_STRIP);
    lineMode = mode;
    lineCount = 0;
}

void Render_LineVertex(float x, float y, float z)
{
    if (lineCount == lineCapacity)
    {
        size_t nextCapacity = lineCapacity ? lineCapacity*2 : 256;
        void* next = SDL_realloc(linePoints, nextCapacity*sizeof(*linePoints));
        GAME_ASSERT(next);
        linePoints = next;
        lineCapacity = nextCapacity;
    }
    linePoints[lineCount++] = (TQ3Point3D){x, y, z};
}

void Render_EndLines(void)
{
    glUseProgram(program);
    glBindVertexArray(lineVAO);
    Attribute(lineBuffer, &lineGPUCapacity, 0, 3, lineCount, linePoints, true);
    glVertexAttrib4f(3, 1, 1, 1, 1);
    SetMatrices(NULL, false);
    glUniform4f(uniforms.color, 1, 1, 1, 1);
    glUniform1i(uniforms.textured, false);
    glUniform1i(uniforms.lit, false);
    glUniform1i(uniforms.fog, false);
    glUniform1i(uniforms.cutout, false);
    glDrawArrays(lineMode, 0, lineCount);
    sceneDirty = true;
    CHECK_GL_ERROR();
}

void RenderWeb_UploadTexture(bool allocate, GLenum internalFormat, int x, int y,
    int width, int height, GLenum format, GLenum type, const void* pixels, int rowLength)
{
    if (!rowLength) rowLength = width;
    if ((format == GL_BGRA || format == GL_BGR) && pixels)
    {
        size_t size = (size_t)width*height*4;
        if (size > convertedCapacity)
        {
            void* next = SDL_realloc(convertedPixels, size);
            GAME_ASSERT(next);
            convertedPixels = next;
            convertedCapacity = size;
        }
        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
            {
                size_t index = row*rowLength+col;
                uint8_t* out = convertedPixels+4*(row*width+col);
                if (format == GL_BGR)
                {
                    size_t stride = (rowLength*3 + 3) & ~(size_t)3;
                    const uint8_t* in = (const uint8_t*)pixels + row*stride + col*3;
                    out[0] = in[2]; out[1] = in[1]; out[2] = in[0]; out[3] = 255;
                }
                else if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV)
                {
                    uint16_t pixel = ((const uint16_t*)pixels)[index];
                    out[0] = ((pixel>>10)&31)*255/31;
                    out[1] = ((pixel>>5)&31)*255/31;
                    out[2] = (pixel&31)*255/31;
                    out[3] = pixel&0x8000 ? 255 : 0;
                }
                else
                {
                    const uint8_t* in = (const uint8_t*)pixels+index*4;
                    if (type == GL_UNSIGNED_INT_8_8_8_8)
                    { out[0]=in[1]; out[1]=in[2]; out[2]=in[3]; out[3]=in[0]; }
                    else
                    { out[0]=in[2]; out[1]=in[1]; out[2]=in[0]; out[3]=in[3]; }
                }
                if (internalFormat == GL_RGB) out[3] = 255;
            }
        pixels = convertedPixels;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        rowLength = width;
    }
    if (format == GL_BGRA || format == GL_BGR) { format = GL_RGBA; type = GL_UNSIGNED_BYTE; }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    if (allocate) glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, pixels);
    else glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    CHECK_GL_ERROR();
}
#endif
