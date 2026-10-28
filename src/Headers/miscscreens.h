//
// miscscreens.h
//

// Screen functions return false while active, true after cleanup.
bool DoPaused(void);
bool ShowLevelIntroScreen(void);
bool DoSettingsScreen(void);
bool DoAboutScreens(void);
bool DoLegalScreen(void);
bool DoPangeaLogo(void);
bool DoWinScreen(void);
bool DoLoseScreen(void);
bool DoModelDebug(void);
bool DoTitleScreen(void);
// -1 while active, 0 on cancellation, 1 after selecting a level.
int DoLevelSelect(void);



// UIRoutines.c

enum
{
	kUIBackground_Cyclorama,
	kUIBackground_Black,
	kUIBackground_White,
};

void SetupUIStuff(int backgroundType);
bool CleanupUIStuff(void);
void MakeSpiderButton(TQ3Point3D coord, const char* caption, int pickID);
int UpdateHoveredPickID(void);
void NukeObjectsInSlot(u_short objNodeSlotID);
