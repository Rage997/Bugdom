/****************************/
/*   	MAIN MENU.C		    */
/* (c)1999 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void MoveMenuCamera(void);
static void SetupMainMenu(void);
static bool WalkSpiderToIcon(void);


/****************************/
/*    CONSTANTS             */
/****************************/


/******************* MENU *************************/

enum
{
	MENU_MObjType_AboutIcon,
	MENU_MObjType_Background,
	MENU_MObjType_ScoresIcon,
	MENU_MObjType_PlayIcon,
	MENU_MObjType_QuitIcon,
	MENU_MObjType_RestoreIcon,
	MENU_MObjType_SettingsIcon,
	MENU_MObjType_Cyc,
	MENU_MObjType_MainMenu
};

enum
{
	kMenuChoice_About = 0,
	kMenuChoice_Scores,
	kMenuChoice_Play,
	kMenuChoice_Quit,
	kMenuChoice_Restore,
	kMenuChoice_Settings,
	NUM_MENU_ICONS,

	kMenuChoice_TimedOut = 1000,
};


/*********************/
/*    VARIABLES      */
/*********************/

static double		gCamDX,gCamDY,gCamDZ;
static TQ3Point3D	gCamCenter = { -10, 10, 250 };		// Source port change from {-40,40,250} (looks better in widescreen)
static ObjNode		*gMenuIcons[NUM_MENU_ICONS];
static ObjNode		*gSpider;
static int32_t		gMenuSelection;

/********************** DO MAIN MENU *************************/
//
// OUTPUT: true == loop to title
//

int DoMainMenu(void)
{
	static int phase;
	static float timer;
	if (phase == 0)
	{
		timer = 0;
		PlaySong(SONG_MENU, true);
		gRestoringSavedGame = false;
		SetupMainMenu();
		gDisableAnimSounds = true;
		InitAnalogCursor();
		QD3D_ResetFrameTimer();
		phase = 1;
		return -1;
	}
	if (phase == 1)
	{
		int mouseX, mouseY;
		UpdateInput();
		MoveObjects();
		MoveMenuCamera();
		QD3D_DrawScene(gGameViewInfoPtr, DrawObjects);
		bool didMove = MoveAnalogCursor(&mouseX, &mouseY);
		DoSDLMaintenance();
		if (IsAnalogCursorClicked() && PickObject(mouseX, mouseY, &gMenuSelection))
		{
			ShutdownAnalogCursor();
			phase = 2;
		}
		else
		{
			timer = didMove ? 0 : timer + gFramesPerSecondFrac;
			if (timer > 20)
			{
				gMenuSelection = kMenuChoice_TimedOut;
				phase = 3;
			}
		}
		return -1;
	}
	if (phase == 2)
	{
		if (!WalkSpiderToIcon()) return -1;
		if (gMenuSelection == kMenuChoice_Quit) CleanQuit();
		phase = 3;
		return -1;
	}
	if (phase == 3)
	{
		bool fadeAudio = gMenuSelection == kMenuChoice_Play || gMenuSelection == kMenuChoice_Scores;
		if (!GammaFadeOut(fadeAudio)) return -1;
		gCurrentSaveSlot = -1;
		ShutdownAnalogCursor();
		DeleteAllObjects();
		FreeAllSkeletonFiles(-1);
		DeleteAll3DMFGroups();
		QD3D_DisposeWindowSetup(&gGameViewInfoPtr);
		GameScreenToBlack();
		Pomme_FlushPtrTracking(true);
		gDisableAnimSounds = false;
		phase = 4;
		return -1;
	}
	switch (gMenuSelection)
	{
		case kMenuChoice_About:
			if (DoAboutScreens()) phase = 0;
			return -1;
		case kMenuChoice_Scores:
			if (ShowHighScoresScreen(0)) phase = 0;
			return -1;
		case kMenuChoice_Settings:
			if (DoSettingsScreen()) phase = 0;
			return -1;
		case kMenuChoice_Restore:
		{
			int picked = DoFileSelectScreen(FILE_SELECT_SCREEN_TYPE_LOAD);
			if (picked == -2) return -1;
			phase = 0;
			if (picked < 0) return -1;
			OSErr err = LoadSavedGame(picked);
			if (err != noErr)
			{
				DoAlert("Couldn't restore game: error %d", err);
				return -1;
			}
			return 0;
		}
		default:
			phase = 0;
			return gMenuSelection == kMenuChoice_TimedOut ? 1 : 0;
	}
}


/****************** SETUP MAIN MENU **************************/

static void SetupMainMenu(void)
{
FSSpec					spec;
QD3DSetupInputType		viewDef;
TQ3Point3D				cameraTo = {0, 0, 0 };
TQ3ColorRGB				lightColor = { 1.0, 1.0, .9 };
TQ3Vector3D				fillDirection1 = { 1, -.4, -.8 };			// key
TQ3Vector3D				fillDirection2 = { -.7, -.2, -.9 };			// fill
ObjNode					*newObj;

	gCamDX = 10; gCamDY = -5, gCamDZ = 1;
	GameScreenToBlack();

			/*************/
			/* MAKE VIEW */
			/*************/

	QD3D_NewViewDef(&viewDef);
	
	viewDef.camera.hither 			= 20;
	viewDef.camera.yon 				= 1000;
	viewDef.camera.fov 				= .9;
	viewDef.styles.usePhong 		= false;
	viewDef.camera.from.x 			= gCamCenter.x + 10;
	viewDef.camera.from.y 			= gCamCenter.y - 35;
	viewDef.camera.from.z 			= gCamCenter.z;
	viewDef.camera.to	 			= cameraTo;
	
	viewDef.lights.numFillLights 	= 2;
	viewDef.lights.ambientBrightness = 0.3;
	viewDef.lights.fillDirection[0] = fillDirection1;
	viewDef.lights.fillDirection[1] = fillDirection2;
	viewDef.lights.fillColor[0] 	= lightColor;
	viewDef.lights.fillColor[1] 	= lightColor;
	viewDef.lights.fillBrightness[0] = 1.1;
	viewDef.lights.fillBrightness[1] = .2;

	viewDef.view.clearColor = TQ3ColorRGBA_FromInt(0x5e63ffff);

	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);

			/************/
			/* LOAD ART */
			/************/
			
	LoadASkeleton(SKELETON_TYPE_SPIDER);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:MainMenu.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_MENU);	


			/***************/
			/* MAKE SPIDER */
			/***************/
		
	gNewObjectDefinition.type 		= SKELETON_TYPE_SPIDER;
	gNewObjectDefinition.animNum 	= 0;
	gNewObjectDefinition.scale 		= .2;
	gNewObjectDefinition.coord.y 	= -40;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.z 	= 7;
	gNewObjectDefinition.slot 		= 100;
	gNewObjectDefinition.flags 		= 0;
	gNewObjectDefinition.moveCall 	= nil;
	gNewObjectDefinition.rot 		= 0;
	gSpider 						= MakeNewSkeletonObject(&gNewObjectDefinition);	
	gSpider->Rot.x = PI/2;	
	UpdateObjectTransforms(gSpider);


			/*******************/
			/* MAKE BACKGROUND */
			/*******************/
			
			/* WEB */
			
	gNewObjectDefinition.group 		= MODEL_GROUP_MENU;	
	gNewObjectDefinition.type 		= MENU_MObjType_Background;	
	gNewObjectDefinition.coord.z 	= 0;
	gNewObjectDefinition.flags 		= STATUS_BIT_NOTRICACHE | STATUS_BIT_NOZWRITE;
	gNewObjectDefinition.moveCall 	= nil;
	gNewObjectDefinition.rot 		= 0;
	gNewObjectDefinition.scale 		= .3f;
	gNewObjectDefinition.slot 		= 1000;
	newObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	MakeObjectTransparent(newObj, .4);

			/* CYC */

	gNewObjectDefinition.slot 		= 100;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.y 	= -6;		// Source port change from -40 (looks better in widescreen)
	gNewObjectDefinition.coord.z 	= 0;
	gNewObjectDefinition.scale 		= .3f;
	gNewObjectDefinition.type 		= MENU_MObjType_Cyc;	
	gNewObjectDefinition.flags 		= STATUS_BIT_NOFOG | STATUS_BIT_NULLSHADER;
	ObjNode* cyc = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	QD3D_MirrorMeshesZ(cyc);

		/* MAIN MENU TEXT */

	gNewObjectDefinition.group 		= MODEL_GROUP_MENU;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.y 	= 100;
	gNewObjectDefinition.coord.z 	= 0;
	gNewObjectDefinition.scale 		= .3f;
	gNewObjectDefinition.type 		= MENU_MObjType_MainMenu;	
	gNewObjectDefinition.flags 		= 0; 
	MakeNewDisplayGroupObject(&gNewObjectDefinition);

			/**************/
			/* MAKE ICONS */
			/**************/
			
	for (int i = 0; i < NUM_MENU_ICONS; i++)
	{
		static const short iconType[NUM_MENU_ICONS] =
		{
			[kMenuChoice_About]		= MENU_MObjType_AboutIcon,
			[kMenuChoice_Scores]	= MENU_MObjType_ScoresIcon,
			[kMenuChoice_Play]		= MENU_MObjType_PlayIcon,
			[kMenuChoice_Quit]		= MENU_MObjType_QuitIcon,
			[kMenuChoice_Restore]	= MENU_MObjType_RestoreIcon,
			[kMenuChoice_Settings]	= MENU_MObjType_SettingsIcon
		};

		static const TQ3Point3D iconCoords[NUM_MENU_ICONS] =
		{
			[kMenuChoice_About]		= { -90, -67, 4 },			// about
			[kMenuChoice_Scores]	= {  10,  50, 4 },			// scores
			[kMenuChoice_Play]		= { -90,  30, 4 },			// play
			[kMenuChoice_Quit]		= {  30,-110, 4 },			// quit
			[kMenuChoice_Restore]	= { 100, -40, 4 },			// restore
			[kMenuChoice_Settings]	= {  80,  30, 4 },			// settings
		};

		gNewObjectDefinition.type 		= iconType[i];	
		gNewObjectDefinition.coord		= iconCoords[i];
		gNewObjectDefinition.flags 		= STATUS_BIT_NULLSHADER;
		gNewObjectDefinition.moveCall 	= nil;
		gNewObjectDefinition.rot 		= 0;
		gNewObjectDefinition.scale 		= .25;
		gMenuIcons[i] = MakeNewDisplayGroupObject(&gNewObjectDefinition);
		gMenuIcons[i]->IsPickable = true;
		gMenuIcons[i]->PickID = i;
	}
	
	MakeFadeEvent(true);
}


/****************** MOVE MENU CAMERA *********************/

static void MoveMenuCamera(void)
{
TQ3Point3D	p;
TQ3Vector3D	grav;
float		d,fps = gFramesPerSecondFrac;

	p = gGameViewInfoPtr->currentCameraCoords;
	
	grav.x = gCamCenter.x - p.x;						// calc vector to center
	grav.y = gCamCenter.y - p.y;
	grav.z = gCamCenter.z - p.z;	
	Q3Vector3D_Normalize(&grav,&grav);
	
	d =  Q3Point3D_Distance(&p, &gCamCenter);
	if (d != 0.0f)
	{
		if (d < 20.0f)
			d = 20.0f;
		d = 1.0f / (d);		// calc 1/distance to center
	}
	else
		d = 10;
	
	gCamDX += grav.x * fps * d * 600.0f;
	gCamDY += grav.y * fps * d * 600.0f;
	gCamDZ += grav.z * fps * d * 600.0f;

	p.x += gCamDX * fps;
	p.y += gCamDY * fps;
	p.z += gCamDZ * fps;
	
	QD3D_UpdateCameraFrom(gGameViewInfoPtr, &p);	
}


/********** WALK SPIDER TO ICON *************/

static bool WalkSpiderToIcon(void)
{
	static bool active, b;
	if (!active)
	{
		active = true;
		b = FlushMouseButtonPress() || GetNewKeyState(kKey_UI_PadConfirm);
		MorphToSkeletonAnim(gSpider->Skeleton, 2, 6);
	}
	{		
		if (FlushMouseButtonPress() || GetNewKeyState(kKey_UI_PadConfirm))			// see if user wants to hurry up
		{
			if (!b)
				goto complete;
		}
		else
			b = false;
	
			
		/* SEE IF CLOSE ENOUGH */
		
		if (CalcDistance(gSpider->Coord.x, gSpider->Coord.y,
						 gMenuIcons[gMenuSelection]->Coord.x, gMenuIcons[gMenuSelection]->Coord.y) < 20.0f)
			goto complete;

		
		/* MOVE SPIDER */
		
		TurnObjectTowardTargetZ(gSpider, gMenuIcons[gMenuSelection]->Coord.x,
								gMenuIcons[gMenuSelection]->Coord.y, 1.0);
		gSpider->Coord.x += -sin(gSpider->Rot.z) * gFramesPerSecondFrac * 40.0f;
		gSpider->Coord.y += cos(gSpider->Rot.z) * gFramesPerSecondFrac * 40.0f;
								
		UpdateObjectTransforms(gSpider);
		
		/* DO OTHER STUFF */
		
		MoveObjects();
		MoveMenuCamera();
		QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);		
		DoSDLMaintenance();
		UpdateInput();									// keys get us out
	}

	return false;
complete:
	MorphToSkeletonAnim(gSpider->Skeleton, 0, 4);
	active = false;
	return true;
}








