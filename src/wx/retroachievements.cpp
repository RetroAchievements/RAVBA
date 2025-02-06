#include "wxvbam.h"

#include "../core/gb/gbGlobals.h"
#include "../core/gba/gbaGlobals.h"
#include "../core/gba/gbaCheats.h"
#include "../core/gba/gbaSound.h"

#include "retroachievements.h"
#include "../../RAInterface/RA_Emulators.h"
#include "RA_BuildVer.h"

static void CausePause(bool pause)
{
    bool value;

    MainFrame* mf = wxGetApp().frame;
    mf->GetMenuOptionBool("Pause", &value);
    if (value != pause)
    {
        wxCommandEvent evh(wxEVT_COMMAND_MENU_SELECTED, XRCID("Pause"));
        evh.SetEventObject(mf);
        mf->GetEventHandler()->ProcessEvent(evh);
    }
}

static void CauseUnpause()
{
    CausePause(false);
}

static void CausePause()
{
    CausePause(true);
}

void RA_ProcessInputs()
{
    if (RA_IsOverlayFullyVisible() && wxGetApp().frame->IsActive())
    {
        uint32_t nKeysDown = systemReadJoypad(-1);

        ControllerInput input;
        input.m_bDownPressed    = (nKeysDown & KEYM_DOWN) != 0;
        input.m_bUpPressed      = (nKeysDown & KEYM_UP) != 0;
        input.m_bLeftPressed    = (nKeysDown & KEYM_LEFT) != 0;
        input.m_bRightPressed   = (nKeysDown & KEYM_RIGHT) != 0;
        input.m_bConfirmPressed = (nKeysDown & KEYM_A) != 0;
        input.m_bCancelPressed  = (nKeysDown & KEYM_B) != 0;
        input.m_bQuitPressed    = (nKeysDown & KEYM_START) != 0;

        RA_NavigateOverlay(&input);
    }
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
    MainFrame* mf = wxGetApp().frame;
    if (mf->GetPanel()->emusys && mf->GetPanel()->emusys->emuReset)
        mf->GetPanel()->emusys->emuReset();

    // ensure all video layers are enabled
    wxCommandEvent evh(wxEVT_COMMAND_MENU_SELECTED, XRCID("VideoLayersReset"));
    evh.SetEventObject(mf);
    mf->GetEventHandler()->ProcessEvent(evh);

    // close debug windows
    while (!mf->popups.empty())
        mf->popups.front()->Close();

    // stop movie playback
    systemStopGamePlayback();

    // disabling the menu option prevents cheats from being evaluated each frame
    if (coreOptions.cheatsEnabled)
    {
        coreOptions.cheatsEnabled = false;
        mf->SetMenuOption("CheatsEnable", 0);
    }

    // pretend there aren't any cheats and call cheatsCheckKeys to reset any ROM
    // that was patched by the previous call to cheatsCheckKeys.
    if (cheatsNumber)
    {
        int oldCheatsNumber = cheatsNumber;
        cheatsNumber = 0;
        cheatsCheckKeys(0, 0);
        cheatsNumber = oldCheatsNumber;
    }

    // reset frame rate
    coreOptions.throttle = 100;
    if (mf->GetPanel()->game_type() != IMAGE_UNKNOWN)
        soundSetThrottle(coreOptions.throttle);
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

    // enfore hardcore behavior
    if (RA_HardcoreModeIsActive())
    {
        // disable cheats
        if (coreOptions.cheatsEnabled)
        {
            coreOptions.cheatsEnabled = false;
            MainFrame* mf = wxGetApp().frame;
            mf->SetMenuOption("CheatsEnable", 0);
        }

        // ensure throttle is not below 100%
        if (coreOptions.throttle < 100)
            coreOptions.throttle = 100;
    }
}

extern uint8_t gbReadMemory(uint16_t address);
extern void gbWriteMemory(uint16_t address, uint8_t value);
extern uint8_t* gbMemoryMap[16];

// GB/GBC Basic byte reader/writer
static unsigned char ByteReader(unsigned int nOffs) { return gbMemoryMap[nOffs >> 12][nOffs & 0x0fff]; }
static void ByteWriter(unsigned int nOffs, unsigned char nVal) { gbMemoryMap[nOffs >> 12][nOffs & 0x0fff] = nVal; }

// GB/GBC Byte reader/writer offset by the size of the map until the end of the work RAM
static unsigned char PostRAMByteReader(unsigned int nOffs)
{
    if (nOffs >= 0x1F03 && nOffs <= 0x1F0E)
    {
        // prevents "Undocumented memory register read" message
        if (nOffs == 0x1F03 || nOffs >= 0x1F08)
            return 0xFF;
    }
    return gbReadMemory(nOffs + 0xE000);
}
static void PostRAMByteWriter(unsigned int nOffs, unsigned char nVal) { gbWriteMemory(nOffs + 0xE000, nVal); }

// GBC RAM reader/writer targeting work RAM bank 1
static unsigned char GBCBank1RAMReader(unsigned int nOffs) { return gbWram ? gbWram[nOffs + 0x1000] : 0; }
static void GBCBank1RAMWriter(unsigned int nOffs, unsigned char nVal) { if (gbWram) gbWram[nOffs + 0x1000] = nVal; }
// GBC RAM reader/writer targeting work RAM banks 2-7
static unsigned char GBCBankedRAMReader(unsigned int nOffs) { return gbWram ? gbWram[nOffs + 0x2000] : 0; }
static void GBCBankedRAMWriter(unsigned int nOffs, unsigned char nVal) { if (gbWram) gbWram[nOffs + 0x2000] = nVal; }

// GBA RAM reader/writer
static unsigned char GBAByteReaderInternalRAM(unsigned int nOffs) { return g_internalRAM ? g_internalRAM[nOffs] : 0; }
static void GBAByteWriterInternalRAM(unsigned int nOffs, unsigned char nVal) { if (g_internalRAM) g_internalRAM[nOffs] = nVal; }

// GBA work RAM reader/writer
static unsigned char GBAByteReaderWorkRAM(unsigned int nOffs) { return g_workRAM ? g_workRAM[nOffs] : 0; }
static void GBAByteWriterWorkRAM(unsigned int nOffs, unsigned char nVal) { if (g_workRAM) g_workRAM[nOffs] = nVal; }

void RA_OnLoadNewRom(ConsoleID nConsole, uint8_t* rom, size_t size, const char* filename)
{
    RA_SetConsoleID(nConsole);

    RA_ClearMemoryBanks();
    switch (nConsole)
    {
        case GB:
            RA_InstallMemoryBank(0, ByteReader, ByteWriter, 0xE000);               // Direct mapping ($0000-$DFFF)
            RA_InstallMemoryBank(1, PostRAMByteReader, PostRAMByteWriter, 0x2000); // Echo RAM + controller registers ($E000-$FFFF)
            break;

        case GBC:
            // GBC has a weird quirk where the memory from $D000-$DFFF is a virtual block of memory pointing at one of the work RAM banks.
            // Bits 0-2 of $FF70 indicate which bank is currently accessible to the program in the $D000-$DFFF range.
            // Since that makes it hard to work with the memory in that region when building/running achievements, the memory exposed to
            // the achievements in $D000-$DFFF is always the first bank. The remaining banks are exposed in virtual memory above $10000.
            // Note that echo RAM does not fix bank 1 into $F000-$FFFF. But achievements shouldn't be using echo RAM anyway.
            RA_InstallMemoryBank(0, ByteReader, ByteWriter, 0xD000);                        // Direct mapping ($0000-$CFFF)
            RA_InstallMemoryBank(1, GBCBank1RAMReader, GBCBank1RAMWriter, 0x1000);          // RAM bank 1 ($D000-$DFFF)
            RA_InstallMemoryBank(2, PostRAMByteReader, PostRAMByteWriter, 0x2000);          // Echo RAM + controller registers ($E000-$FFFF)
            RA_InstallMemoryBank(3, GBCBankedRAMReader, GBCBankedRAMWriter, 0x6000);        // RAM banks 2-7 ($10000-$15FFF)
            break;

        case GBA:
            RA_InstallMemoryBank(0, GBAByteReaderInternalRAM, GBAByteWriterInternalRAM, 0x8000);
            RA_InstallMemoryBank(1, GBAByteReaderWorkRAM, GBAByteWriterWorkRAM, 0x40000);
            break;
    }

    strncpy(s_sGameBeingLoaded, filename, sizeof(s_sGameBeingLoaded));

    RA_OnLoadNewRom(rom, size);

    if (coreOptions.cheatsEnabled && RA_HardcoreModeIsActive())
    {
        coreOptions.cheatsEnabled = false;

        MainFrame* mf = wxGetApp().frame;
        mf->SetMenuOption("CheatsEnable", 0);
    }
}
