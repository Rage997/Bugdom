# How to build Bugdom

## The easy way: build.py (automated build script)

`build.py` can produce a game executable from a fresh clone of the repo in a single command. It will work on macOS, Windows and Linux, provided that your system has Python 3, CMake, and an adequate C++ compiler.

```
git clone --recurse-submodules https://github.com/jorio/Bugdom
cd Bugdom
python3 build.py
```

For native builds, `build.py` remains the recommended entry point. Browser builds use the Emscripten instructions below.

## How to build for the web

Requires CMake 3.24+, Git, Python 3, and [Emscripten 6.0.13](https://emscripten.org/docs/getting_started/downloads.html). Run from the repository root:

```sh
git submodule sync
git submodule update --init --recursive
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
python3 -m http.server 8000 --directory build-web
```

Open **http://localhost:8000/Bugdom.html**. Do not open the HTML with a `file://` URL.

The game starts after loading. CMake fetches SDL 3.2.28; the pinned
[Pomme fork](https://github.com/Rage997/Pomme) reuses that SDL target.
To use a local SDL checkout, add `-DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL`.
Keep web and native builds separate.

The launcher is `packaging/web.html`; artwork is in `packaging/web-assets/`.
Blender is not needed to build.

### Static deployment

Upload these files together to a static HTTPS host:

- `Bugdom.html`, `Bugdom.js`, `Bugdom.wasm`, `Bugdom.data`, `Bugdom.png`
- The complete `web-assets/` directory
- Original credits and [license](LICENSE.md)

You may rename `Bugdom.html` to `index.html`. Serve `.wasm` as `application/wasm`
and `.data` as `application/octet-stream`. No backend or cross-origin isolation is needed.

The asset bundle is about **65 MiB** uncompressed. Check the host's file-size limit
and enable compression.

### GitHub Pages

In the repository's **Settings → Pages**, set **Source** to **GitHub Actions**.
The **Web Build and GitHub Pages** workflow builds the game and deploys pushes to
`master`; pull requests and other branches build without publishing.
You can also run the workflow manually on `master`.

Keep the credits and CC BY-NC-SA 4.0 license. Commercial use needs separate permission.

### Browser behavior and verification

- Requires WebAssembly, WebGL2, and a keyboard/mouse. No touch controls.
- **Arrows/WASD** move, **X** jumps, **C** kicks/boosts, **Space** rolls, **Tab** releases the buddy bug, and **Escape** pauses.
- Click to capture the mouse and enable sound. Losing capture or hiding the tab pauses the game.
- Saves, settings, and scores stay in this browser. Clearing site data deletes them; unavailable storage allows session-only play.
- Graphics context loss requires a reload.

Before publishing, check gameplay, pause/resume, fullscreen, and save/load after a reload.


## How to build the game manually on macOS

1. Install the prerequisites:
    - Xcode (preferably the latest version)
    - [CMake](https://formulae.brew.sh/formula/cmake) 3.21+ (installing via Homebrew is recommended)
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/Bugdom
    cd Bugdom
    ```
1. Download [SDL3-3.2.8.dmg](https://libsdl.org/release/SDL3-3.2.8.dmg), open it, then browse to SDL3.xcframework/macos-arm64_x86_64. In that folder, copy **SDL3.framework** to the game's **extern** folder.
1. Prep the Xcode project:
    ```
    cmake -G Xcode -S . -B build
    ```
1. Now you can open `build/Bugdom.xcodeproj` in Xcode, or you can just go ahead and build the game:
    ```
    cmake --build build --config RelWithDebInfo
    ```
1. The game gets built in `build/RelWithDebInfo/Bugdom.app`. Enjoy!

## How to build the game manually on Windows

1. Install the prerequisites:
    - Visual Studio 2022 with the C++ toolchain
    - [CMake](https://cmake.org/download/) 3.21+
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/Bugdom
    cd Bugdom
    ```
1. Download [SDL3-devel-3.2.8-VC.zip](https://libsdl.org/release/SDL3-devel-3.2.8-VC.zip), extract it, and copy **SDL3-3.2.8** to the **extern** folder. Rename **SDL3-3.2.8** to just **SDL3**.
1. Prep the Visual Studio solution:
    ```
    cmake -G "Visual Studio 17 2022" -A x64 -S . -B build
    ```
1. Now you can open `build/Bugdom.sln` in Visual Studio, or you can just go ahead and build the game:
    ```
    cmake --build build --config Release
    ```
1. The game gets built in `build/Release/Bugdom.exe`. Enjoy!

## How to build the game manually on Linux et al.

1. Install the prerequisites from your package manager:
    - Any C++20 compiler
    - CMake 3.21+
    - SDL3 development library (e.g. "libsdl3-dev" on Ubuntu, "sdl3" on Arch, "SDL3-devel" on Fedora)
    - OpenGL development libraries (e.g. "libgl1-mesa-dev" on Ubuntu)
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/Bugdom
    cd Bugdom
    ```
1. Build the game:
    ```
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ```
    If you'd like to enable runtime sanitizers, append `-DSANITIZE=1` to the **first** `cmake` call above.
1. The game gets built in `build/Bugdom`. Enjoy!
