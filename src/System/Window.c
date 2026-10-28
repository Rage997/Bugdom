/****************************/
/*        WINDOWS           */
/* (c)1997-99 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void MoveFadeEvent(ObjNode *theNode);


/****************************/
/*    CONSTANTS             */
/****************************/


/**********************/
/*     VARIABLES      */
/**********************/

/****************  INIT WINDOW STUFF *******************/

void InitWindowStuff(void)
{
		/* SET FULLSCREEN MODE ACCORDING TO PREFS */

	SetFullscreenMode(true);

	// The outer frame loop presents startup; do not block the browser on swaps.
	QD3DSetupInputType viewDef;
	QD3D_NewViewDef(&viewDef);
	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);
	QD3D_DrawScene(gGameViewInfoPtr, nil);
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);
	Pomme_FlushPtrTracking(true);
}


/**************** GAMMA FADE OUT *************************/

bool GammaFadeOut(Boolean fadeSound)
{
	static bool active = false;
	static float elapsed;
	static float startGamma;
	const float duration = .3f;

	if (!active)
	{
		active = true;
		elapsed = 0;
		startGamma = gGammaFadeFactor;
	}
	elapsed += gFramesPerSecondFrac;
	gGammaFadeFactor = startGamma * fmaxf(0, 1 - elapsed / duration);

	UpdateInput();
	if (gGameViewInfoPtr)
		QD3D_DrawScene(gGameViewInfoPtr, gSuperTileMemoryListExists ? DrawTerrain : DrawObjects);
	if (fadeSound)
		FadeGlobalVolume(gGammaFadeFactor);
	DoSDLMaintenance();

	if (elapsed < duration && startGamma > 0)
		return false;

	active = false;
	gGammaFadeFactor = 1;
	if (fadeSound)
	{
		StopAllEffectChannels();
		KillSong();
		FadeGlobalVolume(1);
	}
	FlushMouseButtonPress();
	return true;
}

/********************** GAMMA ON *********************/

void GammaOn(void)
{
	// Note: the game used to fade gamma in smoothly if it wasn't at 100% already. Changed to instant 100%.
	gGammaFadeFactor = 1.0f;
}



/******************** MAKE FADE EVENT *********************/
//
// INPUT:	fadeIn = true if want fade IN, otherwise fade OUT.
//

void MakeFadeEvent(Boolean	fadeIn)
{
ObjNode	*newObj;
ObjNode		*thisNodePtr;

		/* SCAN FOR OLD FADE EVENTS STILL IN LIST */

	thisNodePtr = gFirstNodePtr;
	
	while (thisNodePtr)
	{
		if ((thisNodePtr->Slot == SLOT_OF_DUMB) &&
			(thisNodePtr->MoveCall == MoveFadeEvent))
		{
			thisNodePtr->Flag[0] = fadeIn;								// set new mode
			return;
		}
		thisNodePtr = thisNodePtr->NextNode;							// next node
	}


		/* MAKE NEW FADE EVENT */
			
	gNewObjectDefinition.genre = EVENT_GENRE;
	gNewObjectDefinition.coord.x = 0;
	gNewObjectDefinition.coord.y = 0;
	gNewObjectDefinition.coord.z = 0;
	gNewObjectDefinition.flags = 0;
	gNewObjectDefinition.slot = SLOT_OF_DUMB;
	gNewObjectDefinition.moveCall = MoveFadeEvent;
	newObj = MakeNewObject(&gNewObjectDefinition);
	if (newObj == nil)
		return;

	newObj->Flag[0] = fadeIn;

	if (fadeIn)
	{
		gGammaFadeFactor = .01f;
	}
	else
	{
		gGammaFadeFactor = .99f;
	}
}


/***************** MOVE FADE EVENT ********************/

static void MoveFadeEvent(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;
		
			/* SEE IF FADE IN */
			
	if (theNode->Flag[0])
	{
		if (gGammaFadeFactor >= 1.0f)										// see if @ 100%
		{
			gGammaFadeFactor = 1.0f;
			DeleteObject(theNode);
			return;
		}
		gGammaFadeFactor += 3.0f * fps;
		if (gGammaFadeFactor >= 1.0f)										// see if @ 100%
			gGammaFadeFactor = 1.0f;
	}
	
			/* FADE OUT */
	else
	{
		if (gGammaFadeFactor <= 0.0f)													// see if @ 0%
		{
			gGammaFadeFactor = 0;
			DeleteObject(theNode);
			return;
		}
		gGammaFadeFactor -= 3.0f * fps;
		if (gGammaFadeFactor <= 0.0f)													// see if @ 0%
			gGammaFadeFactor = 0;
	}
}


/************************ GAME SCREEN TO BLACK ************************/

void GameScreenToBlack(void)
{
	FlushMouseButtonPress();
}

#pragma mark -



void QD3D_OnWindowResized(int windowWidth, int windowHeight)
{
	gWindowWidth = windowWidth;
	gWindowHeight = windowHeight;
}


/******************** GET DEFAULT WINDOW SIZE *******************/

void GetDefaultWindowSize(SDL_DisplayID display, int* width, int* height)
{
	const float aspectRatio = 16.0 / 9.0f;
	const float screenCoverage = .8f;

	SDL_Rect displayBounds = { .x = 0, .y = 0, .w = 640, .h = 480 };
	SDL_GetDisplayUsableBounds(display, &displayBounds);

	if (displayBounds.w > displayBounds.h)
	{
		*width = displayBounds.h * screenCoverage * aspectRatio;
		*height = displayBounds.h * screenCoverage;
	}
	else
	{
		*width = displayBounds.w * screenCoverage;
		*height = displayBounds.w * screenCoverage / aspectRatio;
	}
}

/******************** GET NUM DISPLAYS *******************/

int GetNumDisplays(void)
{
	int numDisplays = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);
	SDL_free(displays);
	return numDisplays;
}

/******************** MOVE WINDOW TO PREFERRED DISPLAY *******************/
//
// This works best in windowed mode.
// Turn off fullscreen before calling this!
//

void MoveToPreferredDisplay(void)
{
	SDL_DisplayID currentDisplay = SDL_GetDisplayForWindow(gSDLWindow);
	SDL_DisplayID desiredDisplay = gGamePrefs.displayNumMinus1 + 1;

	if (currentDisplay != desiredDisplay)
	{
		int w = 640;
		int h = 480;
		GetDefaultWindowSize(desiredDisplay, &w, &h);
		SDL_SetWindowSize(gSDLWindow, w, h);

		SDL_SetWindowPosition(
			gSDLWindow,
			SDL_WINDOWPOS_CENTERED_DISPLAY(desiredDisplay),
			SDL_WINDOWPOS_CENTERED_DISPLAY(desiredDisplay));
		SDL_SyncWindow(gSDLWindow);
	}
}

/*********************** SET FULLSCREEN MODE **********************/

void SetFullscreenMode(bool enforceDisplayPref)
{
#ifdef __EMSCRIPTEN__
	// Only the page's gesture handler may request browser fullscreen.
	(void) enforceDisplayPref;
	gGamePrefs.fullscreen = (SDL_GetWindowFlags(gSDLWindow) & SDL_WINDOW_FULLSCREEN) != 0;
	SDL_GL_SetSwapInterval(1);
#else
	if (!gGamePrefs.fullscreen)
	{
		SDL_SetWindowFullscreen(gSDLWindow, 0);
		SDL_SyncWindow(gSDLWindow);

		if (enforceDisplayPref)
		{
			MoveToPreferredDisplay();
		}
	}
	else
	{
		if (enforceDisplayPref)
		{
			SDL_DisplayID currentDisplay = SDL_GetDisplayForWindow(gSDLWindow);
			SDL_DisplayID desiredDisplay = gGamePrefs.displayNumMinus1 + 1;

			if (currentDisplay != desiredDisplay)
			{
				// We must switch back to windowed mode for the preferred monitor to take effect
				SDL_SetWindowFullscreen(gSDLWindow, false);
				SDL_SyncWindow(gSDLWindow);
				MoveToPreferredDisplay();
			}
		}

		// Enter fullscreen mode
		SDL_SetWindowFullscreen(gSDLWindow, true);
		SDL_SyncWindow(gSDLWindow);
	}

	SDL_GL_SetSwapInterval(gGamePrefs.vsync);
#endif
}
