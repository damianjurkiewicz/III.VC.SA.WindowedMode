#include "dxhandler.h"
#include "injector/injector.hpp"
#include "injector/hooking.hpp"
#include "injector/utility.hpp"
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996 4838)

const bool FORCE_FULLSCREEN_MODE = true;

CIniReader iniReader("WindowedMode.ini");

int CDxHandler::nResetCounter = 0;
int CDxHandler::nCurrentWidth = 0, CDxHandler::nCurrentHeight = 0;
// Usunięto zmienne nNonFull...
bool CDxHandler::bFullMode = false, CDxHandler::bRequestFullMode = false, CDxHandler::bRequestNoBorderMode = false;
bool CDxHandler::bChangingLocked = false;
HMENU CDxHandler::hMenuWindows = NULL; // Pozostawione na wszelki wypadek, ale nieużywane
WNDPROC CDxHandler::wndProcOld = NULL;
bool CDxHandler::bIsInputExclusive = false;
bool CDxHandler::bGameMouseInactive = false;
bool CDxHandler::bStopRecursion = false;
bool CDxHandler::bSizingLoop = false;
GameDxInput** CDxHandler::pInputData;
bool* CDxHandler::bMenuVisible;
HWND* CDxHandler::hGameWnd;
DisplayMode** CDxHandler::pDisplayModes;

// Usunięto bRequestWindowedMode



void(*CDxHandler::CPostEffectsDoScreenModeDependentInitializations)();
void(*CDxHandler::CPostEffectsSetupBackBufferVertex)();
void(*CDxHandler::CMBlurMotionBlurOpen)(RwCamera*);
int(*CDxHandler::DxInputGetMouseState)(int a1);
void(*CDxHandler::ReinitializeRw)(int a1);
int(*CDxHandler::RwEngineGetCurrentVideoMode)();
bool(*CDxHandler::RwRasterDestroy)(RwRaster* pRaster);
RwRaster* (*CDxHandler::RwRasterCreate)(int32_t nWidth, int32_t nHeight, int32_t nDepth, int32_t nFlags);
RwCamera* (*CDxHandler::RwCameraClear)(RwCamera* pCamera, void* pColor, int32_t nClearMode);
RwCamera** CDxHandler::pRenderCamera;
RsGlobalType* CDxHandler::RsGlobal;
uint32_t CDxHandler::RwD3D8AdapterInformation_DisplayMode;
uint32_t CDxHandler::CamCol;
bool* CDxHandler::bBlurOn;
bool CDxHandler::bInGameSA = false;
bool CDxHandler::bResChanged = false;
bool CDxHandler::bWindowed = true;
bool CDxHandler::bUseMenus = true; // Ustawiane na false w AdjustPresentParams
bool CDxHandler::bUseBorder = true; // Ustawiane na false w AdjustPresentParams
SHELLEXECUTEINFOA CDxHandler::ShExecInfo = { 0 };
char CDxHandler::lpWindowName[MAX_PATH];


//ini
int CDxHandler::ini_BackBufferFormat = -1;
int CDxHandler::ini_EnableAutoDepthStencil = -1;
int CDxHandler::ini_AutoDepthStencilFormat = -1;
int CDxHandler::ini_BackBufferCount = -1;
int CDxHandler::ini_MultiSampleType = -1;
int CDxHandler::ini_SwapEffect = -1;
int CDxHandler::ini_PresentationInterval = -1;
int CDxHandler::ini_RefreshRateInHz = -1;
int CDxHandler::ini_MultiSampleQuality = -1;
int CDxHandler::ini_Flags = -1;


// ... (zaraz po ini_Flags = -1;)
Direct3DCreate9_t CDxHandler::original_Direct3DCreate9 = NULL;
CreateDevice_t    CDxHandler::original_CreateDevice = NULL;
Reset_t           CDxHandler::original_Reset = NULL;

std::tuple<int32_t, int32_t> GetDesktopRes()
{
    HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    int32_t DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
    int32_t DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;
    return std::make_tuple(DesktopResW, DesktopResH);
}

void SetCursorVisible(bool state)
{
    // ShowCursor returns state. Use with current visibility to prevent flickering
    CURSORINFO info = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&info)) return;
    bool currState = ShowCursor(info.flags & CURSOR_SHOWING) >= 0;
    while (currState != state)
    {
        currState = ShowCursor(state) >= 0;
    }
}

void CDxHandler::ProcessIni(void)
{
    ini_BackBufferFormat = iniReader.ReadInteger("Direct3D", "BackBufferFormat", -1);
    ini_EnableAutoDepthStencil = iniReader.ReadInteger("Direct3D", "EnableAutoDepthStencil", -1);
    ini_AutoDepthStencilFormat = iniReader.ReadInteger("Direct3D", "AutoDepthStencilFormat", -1);
    ini_BackBufferCount = iniReader.ReadInteger("Direct3D", "BackBufferCount", -1);
    ini_MultiSampleType = iniReader.ReadInteger("Direct3D", "MultiSampleType", -1);
    ini_SwapEffect = iniReader.ReadInteger("Direct3D", "SwapEffect", -1);
    ini_PresentationInterval = iniReader.ReadInteger("Direct3D", "PresentationInterval", -1);
    ini_RefreshRateInHz = iniReader.ReadInteger("Direct3D", "RefreshRateInHz", -1);
    ini_MultiSampleQuality = iniReader.ReadInteger("Direct3D", "MultiSampleQuality", -1);
    ini_Flags = iniReader.ReadInteger("Direct3D", "Flags", -1);
}





void CDxHandler::ToggleFullScreen(void)
{
}

void CDxHandler::StoreRestoreWindowInfo(bool bRestore)
{
    static HMENU hStoredMenu = NULL;
    static int nStoredWidth = 0, nStoredHeight = 0, nStoredPosX = 0, nStoredPosY = 0;
    static DWORD dwStoredFlags = 0;

    RECT rcWindowRect;

    if (!bRestore)
    {
        hStoredMenu = GetMenu(*hGameWnd);
        dwStoredFlags = GetWindowLong(*hGameWnd, GWL_STYLE);

        GetWindowRect(*hGameWnd, &rcWindowRect);
        nStoredWidth = rcWindowRect.right - rcWindowRect.left;
        nStoredHeight = rcWindowRect.bottom - rcWindowRect.top;
        nStoredPosX = rcWindowRect.left;
        nStoredPosY = rcWindowRect.top;

        bChangingLocked = true;
    }
    else
    {
        bChangingLocked = false;

        GetWindowRect(*hGameWnd, &rcWindowRect);
        int nCurWidth = rcWindowRect.right - rcWindowRect.left;
        int nCurHeight = rcWindowRect.bottom - rcWindowRect.top;

        if (nCurWidth != nStoredWidth || nCurHeight != nStoredHeight || GetWindowLong(*hGameWnd, GWL_STYLE) != dwStoredFlags || GetMenu(*hGameWnd) != hStoredMenu)
        {
            bool bOldRecursion = bStopRecursion;
            bStopRecursion = true;

            SetWindowLong(*hGameWnd, GWL_STYLE, dwStoredFlags);
            SetMenu(*hGameWnd, hStoredMenu);
            SetWindowPos(*hGameWnd, NULL, nStoredPosX, nStoredPosY, nStoredWidth, nStoredHeight, SWP_NOACTIVATE | SWP_NOZORDER);

            bStopRecursion = bOldRecursion;
        }
    }
}

void CDxHandler::AdjustGameToWindowSize(void)
{
    // Ta funkcja nie jest już potrzebna, ponieważ nie obsługujemy dynamicznej zmiany rozmiaru okna
}



void CDxHandler::ActivateGameMouse(void)
{
    if (bGameMouseInactive)
    {
        bGameMouseInactive = false;
        DxInputCreateDevice(true);
    }
}

LRESULT APIENTRY CDxHandler::MvlWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KILLFOCUS:
    case WM_ACTIVATE:
        if (uMsg == WM_KILLFOCUS || wParam == WA_INACTIVE) // focus lost
        {
            // Usunięto 'if (!bFullMode && bUseBorder)'
            SetWindowTextA(*hGameWnd, RsGlobal->AppName); // Nadal może być przydatne do paska zadań

            SetCursorVisible(true);
        }
        break;
        // Usunięto 'case WM_LBUTTONUP'
    case WM_SETCURSOR:
        return DefWindowProc(hwnd, uMsg, wParam, lParam); // restore proper handling of ShowCursor
    case WM_STYLECHANGING:
        if (bChangingLocked)
        {
            STYLESTRUCT* pStyleInfo = (STYLESTRUCT*)lParam;
            pStyleInfo->styleOld = pStyleInfo->styleNew;
        }
        return 0;
    case WM_ENTERSIZEMOVE:
        bSizingLoop = true;
        return 0;
    case WM_EXITSIZEMOVE:
    case WM_SIZE:
    case WM_SIZING:
    case WM_WINDOWPOSCHANGED:
    case WM_WINDOWPOSCHANGING:
    {
        if (uMsg == WM_EXITSIZEMOVE)
        {
            bSizingLoop = false;
        }

        if (uMsg == WM_WINDOWPOSCHANGING && bChangingLocked)
        {
            WINDOWPOS* pPosInfo = (WINDOWPOS*)lParam;
            pPosInfo->flags = SWP_NOZORDER | SWP_NOSIZE | SWP_NOREPOSITION | SWP_NOREDRAW | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOACTIVATE;
        }

        // Usunięto wywołanie AdjustGameToWindowSize()
        // if (!bChangingLocked && !bStopRecursion && bWindowed)
        // {
        //     AdjustGameToWindowSize();
        // }

        return 0;
    }
    }

    return CallWindowProc(wndProcOld, hwnd, uMsg, wParam, lParam);
}







void CDxHandler::InjectWindowProc(void)
{
    if (*hGameWnd != NULL && wndProcOld == NULL)
    {
        wndProcOld = (WNDPROC)GetWindowLong(*hGameWnd, GWL_WNDPROC);
        SetWindowLong(*hGameWnd, GWL_WNDPROC, (LONG)CDxHandler::MvlWndProc);
    }
}

void CDxHandler::RemoveWindowProc(void)
{
    if (*hGameWnd != NULL && wndProcOld != NULL)
    {
        if (GetWindowLong(*hGameWnd, GWL_WNDPROC) == (LONG)CDxHandler::MvlWndProc)
        {
            SetWindowLong(*hGameWnd, GWL_WNDPROC, (LONG)wndProcOld);
            wndProcOld = NULL;
        }
    }
}

void CDxHandler::DxInputCreateDevice(bool bExclusive)
{
    static GUID guidDxInputMouse = { 0x6F1D2B60, 0xD5A0, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 };

    if (!*pInputData || !(*pInputData)->pInput) return;

    if (bExclusive == true && bGameMouseInactive)
    {
        bExclusive = false;
    }

    bIsInputExclusive = bExclusive;

    DWORD dwCooperativeLevel = DISCL_FOREGROUND;

    if (bExclusive) dwCooperativeLevel |= DISCL_EXCLUSIVE;
    else dwCooperativeLevel |= DISCL_NONEXCLUSIVE;

    if ((*pInputData)->pInputDevice)
    {
        (*pInputData)->pInputDevice->Unacquire();
    }
    else
    {
        (*pInputData)->pInput->CreateDevice(guidDxInputMouse, &(*pInputData)->pInputDevice, NULL);
        if (!(*pInputData)->pInputDevice) return;

        (*pInputData)->pInputDevice->SetDataFormat((LPCDIDATAFORMAT)0x67EE9C);
    }

    (*pInputData)->pInputDevice->SetCooperativeLevel(*hGameWnd, dwCooperativeLevel);
    (*pInputData)->pInputDevice->Acquire();
}

bool CDxHandler::IsCursorInClientRect(void)
{
    POINT cursorPos;
    RECT rcClient;

    GetClientRect(*hGameWnd, &rcClient);

    if (GetForegroundWindow() == *hGameWnd && GetCursorPos(&cursorPos) && ScreenToClient(*hGameWnd, &cursorPos))
    {
        if (cursorPos.x >= rcClient.left && cursorPos.x < rcClient.right && cursorPos.y >= rcClient.top && cursorPos.y < rcClient.bottom)
        {
            return true;
        }
    }

    return false;
}

int CDxHandler::ProcessMouseState(void)
{
    static Fps _fps;
    _fps.update();

    // Usunięto blok 'if (!bFullMode && bUseBorder)' do ustawiania tytułu okna
    // (nie ma paska tytułowego)

    bool bShowCursor = true;
    bool bForeground = (GetForegroundWindow() == *hGameWnd);

    static DWORD dwLastCheck = 0;

    if (dwLastCheck + 1000 < GetTickCount())
    {
        dwLastCheck = GetTickCount();
    }
    else
    {
        if (bResChanged)
        {
            if (bInGameSA)
            {
                CPostEffectsSetupBackBufferVertex();
                // CPostEffectsDoScreenModeDependentInitializations();
            }

            bResChanged = false;
        }
    }

    if (*bMenuVisible)
    {
        bShowCursor = !IsCursorInClientRect();
        bGameMouseInactive = false;
    }
    else
    {
        if (!bGameMouseInactive && GetForegroundWindow() == *hGameWnd)
            bShowCursor = false;
    }

    SetCursorVisible(bShowCursor);

    if ((!*bMenuVisible && bGameMouseInactive) || (*bMenuVisible && !IsCursorInClientRect()))
    {
        return 0;
    }

    return 1;
}

// Ta funkcja zastępuje stary, szablonowy AdjustPresentParams
// Używa natywnych typów D3D9 i poprawnych nazw pól
void CDxHandler::AdjustPresentParams_D3D9(D3DPRESENT_PARAMETERS* pParams)
{
    // --- START OF CONFIGURATION ---

    // 1. Zawsze wymuś tryb okienkowy (borderless)
    pParams->Windowed = TRUE;

    // 2. Zastosuj ustawienia z .ini (tylko jeśli nie są -1)
    if (ini_BackBufferFormat != -1)
        pParams->BackBufferFormat = (D3DFORMAT)ini_BackBufferFormat;

    if (ini_EnableAutoDepthStencil != -1)
        pParams->EnableAutoDepthStencil = (BOOL)ini_EnableAutoDepthStencil;

    if (ini_AutoDepthStencilFormat != -1)
        pParams->AutoDepthStencilFormat = (D3DFORMAT)ini_AutoDepthStencilFormat;

    if (ini_BackBufferCount != -1)
        pParams->BackBufferCount = ini_BackBufferCount;

    if (ini_MultiSampleType != -1)
        pParams->MultiSampleType = (D3DMULTISAMPLE_TYPE)ini_MultiSampleType;

    if (ini_MultiSampleQuality != -1)
        pParams->MultiSampleQuality = ini_MultiSampleQuality;

    if (ini_SwapEffect != -1)
        pParams->SwapEffect = (D3DSWAPEFFECT)ini_SwapEffect;

    if (ini_PresentationInterval != -1)
    {
        // Poprawna nazwa pola w D3D9
        pParams->PresentationInterval = ini_PresentationInterval;
    }

    if (ini_RefreshRateInHz != -1)
    {
        // Poprawna nazwa pola w D3D9 (ale ignorowana w trybie okienkowym)
        pParams->FullScreen_RefreshRateInHz = ini_RefreshRateInHz;
    }

    if (ini_Flags != -1)
        pParams->Flags = ini_Flags;

    // 3. Zabezpieczenie dla MSAA
    if (pParams->MultiSampleType > 0)
        pParams->SwapEffect = D3DSWAPEFFECT_DISCARD;

    // --- END OF CONFIGURATION ---


    // --- BORDERLESS FULLSCREEN LOGIC ---
    DWORD dwWndStyle = GetWindowLong(*hGameWnd, GWL_STYLE);
    auto [nMonitorWidth, nMonitorHeight] = GetDesktopRes();

    dwWndStyle &= ~WS_OVERLAPPEDWINDOW;
    pParams->BackBufferWidth = nMonitorWidth;
    pParams->BackBufferHeight = nMonitorHeight;
    bFullMode = true;
    bUseBorder = false;
    bUseMenus = false;

    nCurrentWidth = (int)pParams->BackBufferWidth;
    nCurrentHeight = (int)pParams->BackBufferHeight;

    if (RsGlobal) // Sprawdzenie, czy RsGlobal zostało znalezione
    {
        RsGlobal->MaximumWidth = pParams->BackBufferWidth;
        RsGlobal->MaximumHeight = pParams->BackBufferHeight;
    }

    bRequestFullMode = false;
    RECT rcClient = { 0, 0, pParams->BackBufferWidth, pParams->BackBufferHeight };
    AdjustWindowRectEx(&rcClient, dwWndStyle, FALSE, GetWindowLong(*hGameWnd, GWL_EXSTYLE));

    int nClientWidth = rcClient.right - rcClient.left;
    int nClientHeight = rcClient.bottom - rcClient.top;

    bool bOldRecursion = bStopRecursion;
    bStopRecursion = true;

    SetWindowLong(*hGameWnd, GWL_STYLE, dwWndStyle);
    SetMenu(*hGameWnd, NULL);
    nClientWidth = min(nClientWidth, nMonitorWidth);
    nClientHeight = min(nClientHeight, nMonitorHeight);
    SetWindowPos(*hGameWnd, HWND_NOTOPMOST, 0, 0, nClientWidth, nClientHeight, SWP_NOACTIVATE);

    bStopRecursion = bOldRecursion;

    pParams->hDeviceWindow = *hGameWnd;
    bResChanged = true;
}


// ===== NOWY MECHANIZM HAKUJĄCY =====

// Krok 3: Nasz hak na Reset()
HRESULT STDMETHODCALLTYPE CDxHandler::Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    // Uruchom naszą logikę przed wywołaniem oryginalnego Reset
    if (bWindowed)
    {
        AdjustPresentParams_D3D9(pPresentationParameters);
    }

    bool bInitialLocked = bChangingLocked;
    if (!bInitialLocked) StoreRestoreWindowInfo(false);

    // Wywołaj oryginalny Reset
    HRESULT hRes = original_Reset(pDevice, pPresentationParameters);

    if (!bInitialLocked) StoreRestoreWindowInfo(true);

    if (SUCCEEDED(hRes))
    {
        // Zaktualizuj szerokość/wysokość w grze
        int nModeIndex = RwEngineGetCurrentVideoMode();
        (*pDisplayModes)[nModeIndex].nWidth = pPresentationParameters->BackBufferWidth;
        (*pDisplayModes)[nModeIndex].nHeight = pPresentationParameters->BackBufferHeight;

        bResChanged = true; // Ustaw flagę, aby ProcessMouseState mogło zadziałać
    }

    return hRes;
}

// Krok 2: Nasz hak na CreateDevice()
HRESULT STDMETHODCALLTYPE CDxHandler::Hook_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
    // Wstrzyknij naszą procedurę okna, zanim urządzenie zostanie utworzone
    InjectWindowProc();

    // Uruchom naszą logikę na parametrach *przed* utworzeniem urządzenia
    if (bWindowed)
    {
        AdjustPresentParams_D3D9(pPresentationParameters);
    }

    // Wywołaj oryginalne CreateDevice
    HRESULT hRes = original_CreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    if (SUCCEEDED(hRes))
    {
        // Udało się! Mamy wskaźnik na urządzenie (*ppReturnedDeviceInterface)
        // Teraz możemy zahakować jego V-Table, aby złapać Reset()

        IDirect3DDevice9* pDevice = *ppReturnedDeviceInterface;
        UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pDevice));

        // Zapisz oryginalny wskaźnik na Reset
        original_Reset = (Reset_t)pVTable[16];

        // Nadpisz wskaźnik w V-Table naszym własnym
        bool bOldRecursion = bStopRecursion;
        bStopRecursion = true;
        injector::WriteMemory(&pVTable[16], &Hook_Reset, true);
        bStopRecursion = bOldRecursion;
    }

    return hRes;
}

// Krok 1: Nasz hak na Direct3DCreate9()
IDirect3D9* WINAPI CDxHandler::Hook_Direct3DCreate9(UINT SDKVersion)
{
    // Wywołaj oryginalną funkcję, aby uzyskać prawdziwy obiekt IDirect3D9
    IDirect3D9* pD3D = original_Direct3DCreate9(SDKVersion);

    if (pD3D)
    {
        // Udało się! Mamy "fabrykę".
        // Teraz hakujemy jej V-Table, aby złapać CreateDevice()

        UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pD3D));

        // Zapisz oryginalny wskaźnik na CreateDevice (indeks 16)
        original_CreateDevice = (CreateDevice_t)pVTable[16];

        // Nadpisz wskaźnik w V-Table naszym własnym
        bool bOldRecursion = bStopRecursion;
        bStopRecursion = true;
        injector::WriteMemory(&pVTable[16], &Hook_CreateDevice, true);
        bStopRecursion = bOldRecursion;
    }

    return pD3D;
}

// Nowy punkt wejścia, który uruchomi cały proces
void CDxHandler::InstallD3D9Hook(void)
{
    // Znajdź adres Direct3DCreate9 w importach gry
    // To jest najbardziej uniwersalna metoda
    HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
    if (!hD3D9)
    {
        hD3D9 = LoadLibraryA("d3d9.dll");
    }

    if (hD3D9)
    {
        original_Direct3DCreate9 = (Direct3DCreate9_t)GetProcAddress(hD3D9, "Direct3DCreate9");
        if (original_Direct3DCreate9)
        {
            // Hookuj globalny punkt wejścia
            injector::MakeCALL(original_Direct3DCreate9, Hook_Direct3DCreate9, true);
        }
    }

}