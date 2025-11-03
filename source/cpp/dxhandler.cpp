#include "dxhandler.h"

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
IDirect3D8** CDxHandler::pIntDirect3DMain;
IDirect3DDevice8** CDxHandler::pDirect3DDevice;
GameDxInput** CDxHandler::pInputData;
bool* CDxHandler::bMenuVisible;
HWND* CDxHandler::hGameWnd;
DisplayMode** CDxHandler::pDisplayModes;

// Usunięto bRequestWindowedMode

HRESULT(__stdcall* CDxHandler::oldReset)(LPDIRECT3DDEVICE8 pDevice, void* pPresentationParameters);
HRESULT(__stdcall* CDxHandler::oldSetViewport)(LPDIRECT3DDEVICE8 pDevice, CONST D3DVIEWPORT8* pViewport);
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
uint32_t CDxHandler::HookParams;
uint32_t CDxHandler::HookDirect3DDeviceReplacerJmp;
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

template<class D3D_TYPE>
HRESULT CDxHandler::HandleReset(D3D_TYPE* pPresentationParameters, void* pSourceAddress)
{
    if (bWindowed)
    {
        CDxHandler::AdjustPresentParams(pPresentationParameters);
    }

    CDxHandler::nResetCounter++;

    bool bInitialLocked = CDxHandler::bChangingLocked;
    if (!bInitialLocked) CDxHandler::StoreRestoreWindowInfo(false);

    bool bOldRecursion = CDxHandler::bStopRecursion;
    CDxHandler::bStopRecursion = true;
    HRESULT hRes = oldReset(*pDirect3DDevice, pPresentationParameters);
    CDxHandler::bStopRecursion = bOldRecursion;

    if (!bInitialLocked) CDxHandler::StoreRestoreWindowInfo(true);

    if (SUCCEEDED(hRes))
    {
        int nModeIndex = RwEngineGetCurrentVideoMode();
        (*CDxHandler::pDisplayModes)[nModeIndex].nWidth = pPresentationParameters->BackBufferWidth;
        (*CDxHandler::pDisplayModes)[nModeIndex].nHeight = pPresentationParameters->BackBufferHeight;
    }

    return hRes;
}

template<class D3D_TYPE>
void CDxHandler::AdjustPresentParams(D3D_TYPE* pParams)
{
    if (!bWindowed) return;

    bool bOldRecursion = bStopRecursion;

    // --- START OF CONFIGURATION ---

    // 1. Always force windowed (borderless) mode
    pParams->Windowed = TRUE;

    // 2. Apply all settings from the .ini file (only if they are not -1)
    if (ini_BackBufferFormat != -1)
    {
        pParams->BackBufferFormat = (D3DFORMAT)ini_BackBufferFormat;
    }

    if (ini_EnableAutoDepthStencil != -1)
    {
        pParams->EnableAutoDepthStencil = (BOOL)ini_EnableAutoDepthStencil;
    }

    if (ini_AutoDepthStencilFormat != -1)
    {
        pParams->AutoDepthStencilFormat = (D3DFORMAT)ini_AutoDepthStencilFormat;
    }

    if (ini_BackBufferCount != -1)
        
    {
        pParams->BackBufferCount = ini_BackBufferCount;
    }

        if (ini_MultiSampleType != -1)
        {
            pParams->MultiSampleType = (D3DMULTISAMPLE_TYPE)ini_MultiSampleType;
        }

    // --- NEW ---
    if (ini_MultiSampleQuality != -1)
    {
        pParams->MultiSampleQuality = ini_MultiSampleQuality;
    }

    if (ini_SwapEffect != -1)
    {
        pParams->SwapEffect = (D3DSWAPEFFECT)ini_SwapEffect;
    }

    if (ini_PresentationInterval != -1)
    {
        pParams->FullScreen_PresentationInterval = ini_PresentationInterval;
    }

    if (ini_RefreshRateInHz != -1)
    {
        pParams->FullScreen_RefreshRateInHz = ini_RefreshRateInHz;
    }

    // --- NEW ---
    if (ini_Flags != -1)
    {
        // This will REPLACE all default flags. 
        // Use 0 to clear all flags.
        pParams->Flags = ini_Flags;
    }

    // 3. Safety check for MultiSampling (MSAA)
    // This MUST run after .ini settings are applied.
    if (pParams->MultiSampleType > 0)
    {
        // If MSAA is enabled (from game or .ini), we MUST force DISCARD swap effect
        // to prevent a crash.
        pParams->SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    // --- END OF CONFIGURATION ---


    // --- BORDERLESS FULLSCREEN LOGIC (No need to edit below) ---

    DWORD dwWndStyle = GetWindowLong(*hGameWnd, GWL_STYLE);

    auto [nMonitorWidth, nMonitorHeight] = GetDesktopRes();

    // Always force borderless fullscreen style
    dwWndStyle &= ~WS_OVERLAPPEDWINDOW;
    pParams->BackBufferWidth = nMonitorWidth;
    pParams->BackBufferHeight = nMonitorHeight;
    bFullMode = true;
    bUseBorder = false;
    bUseMenus = false;

    nCurrentWidth = (int)pParams->BackBufferWidth;
    nCurrentHeight = (int)pParams->BackBufferHeight;

    RsGlobal->MaximumWidth = pParams->BackBufferWidth;
    RsGlobal->MaximumHeight = pParams->BackBufferHeight;

    bRequestFullMode = false;

    RECT rcClient = { 0, 0, pParams->BackBufferWidth, pParams->BackBufferHeight };

    AdjustWindowRectEx(&rcClient, dwWndStyle, FALSE, GetWindowLong(*hGameWnd, GWL_EXSTYLE));

    int nClientWidth = rcClient.right - rcClient.left;
    int nClientHeight = rcClient.bottom - rcClient.top;

    bOldRecursion = bStopRecursion;
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

void CDxHandler::MainCameraRebuildRaster(RwCamera* pCamera)
{
    if (pCamera == *pRenderCamera)
    {
        IDirect3DSurface8* pSurface;
        D3DSURFACE_DESC stSurfaceDesc;

        (*pDirect3DDevice)->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pSurface);
        pSurface->GetDesc(&stSurfaceDesc);
        pSurface->Release();

        int nModeIndex = RwEngineGetCurrentVideoMode();

        if ((*CDxHandler::pDisplayModes)[nModeIndex].nWidth != (int)stSurfaceDesc.Width || (*CDxHandler::pDisplayModes)[nModeIndex].nHeight != (int)stSurfaceDesc.Height)
        {
            (*CDxHandler::pDisplayModes)[nModeIndex].nWidth = (int)stSurfaceDesc.Width;
            (*CDxHandler::pDisplayModes)[nModeIndex].nHeight = (int)stSurfaceDesc.Height;
        }

        int nGameWidth = (*CDxHandler::pDisplayModes)[nModeIndex].nWidth;
        int nGameHeight = (*CDxHandler::pDisplayModes)[nModeIndex].nHeight;

        *(int*)RwD3D8AdapterInformation_DisplayMode = nGameWidth;
        *(int*)(RwD3D8AdapterInformation_DisplayMode + 4) = nGameHeight;

        if (pCamera->frameBuffer && (pCamera->frameBuffer->nWidth != nGameWidth || pCamera->frameBuffer->nHeight != nGameHeight))
        {
            RwRasterDestroy(pCamera->frameBuffer);
            pCamera->frameBuffer = NULL;
        }

        if (!pCamera->frameBuffer)
        {
            pCamera->frameBuffer = RwRasterCreate(nGameWidth, nGameHeight, 32, rwRASTERTYPECAMERA);
        }

        if (pCamera->zBuffer && (pCamera->zBuffer->nWidth != nGameWidth || pCamera->zBuffer->nHeight != nGameHeight))
        {
            RwRasterDestroy(pCamera->zBuffer);
            pCamera->zBuffer = NULL;
        }

        if (!pCamera->zBuffer)
        {
            pCamera->zBuffer = RwRasterCreate(nGameWidth, nGameHeight, 0, rwRASTERTYPEZBUFFER);
        }
    }
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

HRESULT __stdcall ResetSA(LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS_D3D9* pPresentationParameters) {
    return CDxHandler::HandleReset(pPresentationParameters, nullptr);
}

HRESULT __stdcall SetViewport(LPDIRECT3DDEVICE8 pDevice, CONST D3DVIEWPORT8* pViewport) {
    bool bInitialLock = CDxHandler::bChangingLocked;

    if (!bInitialLock) CDxHandler::StoreRestoreWindowInfo(false);
    HRESULT hres = CDxHandler::oldSetViewport(*CDxHandler::pDirect3DDevice, pViewport);
    if (!bInitialLock) CDxHandler::StoreRestoreWindowInfo(true);

    return hres;
}

void CDxHandler::Direct3DDeviceReplaceSA(void)
{
    if (*pDirect3DDevice != NULL)
    {
        UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)*pDirect3DDevice));
        if (!oldReset)
        {
            oldReset = (HRESULT(__stdcall*)(LPDIRECT3DDEVICE8 pDevice, void* pPresentationParameters))(*(uint32_t*)&pVTable[16]);
            oldSetViewport = (HRESULT(__stdcall*)(LPDIRECT3DDEVICE8 pDevice, CONST D3DVIEWPORT8 * pViewport))(*(uint32_t*)&pVTable[47]);
        }

        injector::WriteMemory(&pVTable[16], &ResetSA, true);
        injector::WriteMemory(&pVTable[47], &SetViewport, true);
    }
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

void __declspec(naked) CDxHandler::HookDirect3DDeviceReplacerSA(void)
{
    static HRESULT hRes;
    static bool bOldRecursion;
    static bool bOldLocked;

    _asm pushad

    bOldRecursion = bStopRecursion;
    bStopRecursion = true;

    InjectWindowProc();
    AdjustPresentParams((D3DPRESENT_PARAMETERS_D3D9*)HookParams);

    bOldLocked = bChangingLocked;
    if (!bOldLocked) StoreRestoreWindowInfo(false); // <-- Poprawiona spacja
    RemoveWindowProc();
    bChangingLocked = true;

    _asm popad
    _asm call[edx + 40h]
        _asm mov hRes, eax
    _asm pushad

    bChangingLocked = bOldLocked;
    InjectWindowProc();
    if (!bOldLocked) StoreRestoreWindowInfo(true); // <-- Poprawiona spacja

    Direct3DDeviceReplaceSA();
    bStopRecursion = bOldRecursion;

    _asm popad
    _asm test eax, eax
    _asm jmp HookDirect3DDeviceReplacerJmp
}