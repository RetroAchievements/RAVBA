#ifndef __RETROACHIEVEMENTS_H_
#define __RETROACHIEVEMENTS_H_

#include "../../RAInterface/RA_Interface.h"

void RA_Init(HWND hWnd);
void RA_OnLoadNewRom(ConsoleID nConsole, uint8_t* rom, size_t size, const char* filename);

#endif __RETROACHIEVEMENTS_H_
