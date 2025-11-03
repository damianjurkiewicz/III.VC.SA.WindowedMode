#pragma once

// KROK 1: Dołącz prawdziwe D3D9 JAKO PIERWSZE.
// To rozwiąże błędy konfliktu z D3D8.
#include <d3d9.h>
#include <d3d9types.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
// KROK 2: Dołącz resztę swoich zależności
#include "misc.h"

// Definicje typów dla naszych wskaźników na oryginalne funkcje
// Teraz kompilator wie, czym jest IDirect3D9 i IDirect3DDevice9
typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT SDKVersion);
typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);




class CDxHandler
{
public:
    // --- NOWY MECHANIZM HAKUJĄCY D3D9 ---
    static void InstallD3D9Hook(void); // Nowy punkt wejścia
    static IDirect3D9* WINAPI Hook_Direct3DCreate9(UINT SDKVersion);
    static HRESULT STDMETHODCALLTYPE Hook_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
    static HRESULT STDMETHODCALLTYPE Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

    // Wskaźniki na oryginalne funkcje
    static Direct3DCreate9_t original_Direct3DCreate9;
    static CreateDevice_t    original_CreateDevice;
    static Reset_t           original_Reset;

    // Nowa, natywna funkcja AdjustParams dla D3D9
    static void AdjustPresentParams_D3D9(D3DPRESENT_PARAMETERS* pParams);


    // --- USUNIĘTE STARE FUNKCJE ---
    // HandleReset, AdjustPresentParams<template>, Direct3DDeviceReplace,
    // Direct3DDeviceReplaceSA, HookDirect3DDeviceReplacer, HookDirect3DDeviceReplacerSA,
    // ResetSA, oldReset, oldSetViewport, SetViewport

    // --- Zachowane funkcje pomocnicze ---
    static void ToggleFullScreen(void);
    static void StoreRestoreWindowInfo(bool bRestore);
    static void AdjustGameToWindowSize(void);
    static void InjectWindowProc(void);
    static void RemoveWindowProc(void);
    static void ActivateGameMouse(void);
    static LRESULT APIENTRY MvlWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void DxInputCreateDevice(bool bExclusive);
    static bool IsCursorInClientRect(void);
    static int ProcessMouseState(void);
    static void ProcessIni(void);

    // --- Zmienne statyczne (w większości bez zmian) ---
    static int nResetCounter;
    static int nCurrentWidth, nCurrentHeight;
    static bool bFullMode, bRequestFullMode, bRequestNoBorderMode;
    static bool bChangingLocked;
    static HMENU hMenuWindows;
    static WNDPROC wndProcOld;
    static bool bIsInputExclusive;
    static bool bGameMouseInactive;
    static bool bStopRecursion;
    static bool bSizingLoop;

    // Te wskaźniki z gry mogą nadal być potrzebne do innych funkcji, więc je zostawiamy
    static GameDxInput** pInputData;
    static bool* bMenuVisible;
    static HWND* hGameWnd;
    static DisplayMode** pDisplayModes;

    static void(*CPostEffectsDoScreenModeDependentInitializations)();
    static void(*CPostEffectsSetupBackBufferVertex)();
    static void(*CMBlurMotionBlurOpen)(RwCamera*);
    static int(*DxInputGetMouseState)(int a1);
    static void(*ReinitializeRw)(int a1);
    static int(*RwEngineGetCurrentVideoMode)();
    static RwCamera* (*RwCameraClear)(RwCamera* pCamera, void* pColor, int32_t nClearMode);
    static bool(*RwRasterDestroy)(RwRaster* pRaster);
    static RwRaster* (*RwRasterCreate)(int32_t nWidth, int32_t nHeight, int32_t nDepth, int32_t nFlags);
    static RwCamera** pRenderCamera;
    static bool* bBlurOn;
    static RsGlobalType* RsGlobal;
    static uint32_t RwD3D8AdapterInformation_DisplayMode;
    static uint32_t CamCol;

    static bool bInGameSA;
    static bool bResChanged;
    static bool bWindowed;
    static bool bUseMenus;
    static bool bUseBorder;
    static SHELLEXECUTEINFOA ShExecInfo;
    static char lpWindowName[MAX_PATH];

    // Zmienne konfiguracyjne INI
    static int ini_BackBufferFormat;
    static int ini_EnableAutoDepthStencil;
    static int ini_AutoDepthStencilFormat;
    static int ini_BackBufferCount;
    static int ini_MultiSampleType;
    static int ini_SwapEffect;
    static int ini_PresentationInterval;
    static int ini_RefreshRateInHz;
    static int ini_MultiSampleQuality;
    static int ini_Flags;
};