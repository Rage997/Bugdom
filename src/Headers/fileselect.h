//
// fileselect.h
//


enum
{
	FILE_SELECT_SCREEN_TYPE_LOAD,
	FILE_SELECT_SCREEN_TYPE_SAVE,
};

// -2 while pending, -1 when cancelled, otherwise the selected save slot.
int DoFileSelectScreen(int type);
