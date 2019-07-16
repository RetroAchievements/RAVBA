#include "wxvbam.h"

#include "retroachievements.h"
#include "RA_BuildVer.h"


static void CauseUnpause()
{
}

static void CausePause()
{
}

static int GetMenuItemIndex(HMENU hMenu, const char* pItemName)
{
    int nIndex = 0;
    char pBuffer[256];

    while (nIndex < GetMenuItemCount(hMenu))
    {
        if (GetMenuStringA(hMenu, nIndex, pBuffer, sizeof(pBuffer) - 1, MF_BYPOSITION))
        {
            if (!strcmp(pItemName, pBuffer))
                return nIndex;
        }
        nIndex++;
    }

    return -1;
}

static void RebuildMenu()
{
    HMENU hMainMenu = GetMenu(wxGetApp().frame->GetHWND());
    if (!hMainMenu)
        return;

    // if RetroAchievements submenu exists, destroy it
    int index = GetMenuItemIndex(hMainMenu, "&RetroAchievements");
    if (index >= 0)
        DeleteMenu(hMainMenu, index, MF_BYPOSITION);

    // append RetroAchievements menu
    AppendMenu(hMainMenu, MF_POPUP | MF_STRING, (UINT_PTR)RA_CreatePopupMenu(), TEXT("&RetroAchievements"));

    // repaint
    DrawMenuBar(wxGetApp().frame->GetHWND());
}

static char s_sGameBeingLoaded[64];
static void GetEstimatedGameTitle(char* sNameOut)
{
    strncpy(sNameOut, s_sGameBeingLoaded, 64);
}

static void ResetEmulator()
{
}

static void LoadROM(const char* sFullPath) {}

void RA_Init(HWND hWnd)
{
    // initialize the DLL
    RA_Init(hWnd, RA_VisualboyAdvance, RAVBA_VERSION);
    RA_SetConsoleID(GBA);

    // provide callbacks to the DLL
    RA_InstallSharedFunctions(NULL, CauseUnpause, CausePause, RebuildMenu, GetEstimatedGameTitle, ResetEmulator, LoadROM);

    // add a placeholder menu item and start the login process - menu will be updated when login completes
    RebuildMenu();
    RA_AttemptLogin(false);

    // ensure titlebar text matches expected format
    RA_UpdateAppTitle("");
}

extern uint8_t gbReadMemory(uint16_t address);
extern void gbWriteMemory(uint16_t address, uint8_t value);

// GB/GBC Basic byte reader/writer
static unsigned char ByteReader(size_t nOffs) { return gbReadMemory(nOffs); }
static void ByteWriter(size_t nOffs, unsigned char nVal) { gbWriteMemory(nOffs, nVal); }

// GBC Byte reader/writer offset by the size of the map until the end of the work RAM
static unsigned char PostRAMByteReader(size_t nOffs) { return gbReadMemory(nOffs + 0xE000); }
static void PostRAMByteWriter(size_t nOffs, unsigned char nVal) { gbWriteMemory(nOffs + 0xE000, nVal); }

// GBC RAM reader/writer targeting the first bank on work RAM
static unsigned char GBCFirstRAMBankReader(size_t nOffs) { return gbWram[nOffs + 0x1000]; }
static void GBCFirstRAMBankWriter(size_t nOffs, unsigned char nVal) { gbWram[nOffs + 0x1000] = nVal; }

// GBC RAM reader/writer targeting work RAM banks 2-7
static unsigned char GBCBankedRAMReader(size_t nOffs) { return gbWram[nOffs + 0x2000]; }
static void GBCBankedRAMWriter(size_t nOffs, unsigned char nVal) { gbWram[nOffs + 0x2000] = nVal; }

// GBA RAM reader/writer
static unsigned char GBAByteReaderInternalRAM(size_t nOffs) { return internalRAM[nOffs]; }
static void GBAByteWriterInternalRAM(size_t nOffs, unsigned char nVal) { internalRAM[nOffs] = nVal; }

// GBA work RAM reader/writer
static unsigned char GBAByteReaderWorkRAM(size_t nOffs) { return workRAM[nOffs]; }
static void GBAByteWriterWorkRAM(size_t nOffs, unsigned char nVal) { workRAM[nOffs] = nVal; }

void RA_OnLoadNewRom(ConsoleID nConsole, uint8_t* rom, size_t size, const char* filename)
{
    RA_SetConsoleID(nConsole);

    RA_ClearMemoryBanks();
    switch (nConsole)
    {
        case GB:
            RA_InstallMemoryBank(0, ByteReader, ByteWriter, 0x10000);
            break;

        case GBC:
            // GBC has a weird quirk where the memory from $D000-$DFFF is a virtual block of memory pointing at one of the work RAM banks.
            // Bits 0-2 of $FF70 indicate which bank is currently accessible to the program in the $D000-$DFFF range.
            // Since that makes it hard to work with the memory in that region when building/running achievements, the memory exposed to
            // the achievements in $D000-$DFFF is always the first bank. The remaining banks are exposed in virtual memory above $10000.
            RA_InstallMemoryBank(0, ByteReader, ByteWriter, 0xD000);                        // Direct mapping ($0000-$CFFF)
            RA_InstallMemoryBank(1, GBCFirstRAMBankReader, GBCFirstRAMBankWriter, 0x1000);  // First bank ($D000-$DFFF)
            RA_InstallMemoryBank(2, PostRAMByteReader, PostRAMByteWriter, 0x2000);          // Direct mapping ($E000-$FFFF)
            RA_InstallMemoryBank(3, GBCBankedRAMReader, GBCBankedRAMWriter, 0x6000);        // RAM banks 2-7 ($10000-$15FFF)
            break;

        case GBA:
            RA_InstallMemoryBank(0, GBAByteReaderInternalRAM, GBAByteWriterInternalRAM, 0x8000);
            RA_InstallMemoryBank(1, GBAByteReaderWorkRAM, GBAByteWriterWorkRAM, 0x40000);
            break;
    }

    strncpy(s_sGameBeingLoaded, filename, sizeof(s_sGameBeingLoaded));

    RA_OnLoadNewRom(rom, size);
}
