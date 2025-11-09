#include "Logger.h" 
#include "dxhandler.h"

void CDxHandler::SetupHooksSA(void)
{
	LOG_STREAM << "SetupHooksSA: Initializing San Andreas hooks...";

	bInGameSA = true;
	CPostEffectsDoScreenModeDependentInitializations = (void(*)())0x7046D0;
	CPostEffectsSetupBackBufferVertex = (void(*)())0x7043D0;
	DxInputGetMouseState = (int(*)(int))0x746ED0;
	ReinitializeRw = (void(*)(int))0x0;
	RwEngineGetCurrentVideoMode = (int(*)())0x7F2D20;
	RwCameraClear = (RwCamera * (*)(RwCamera*, void*, int32_t))0x7EE340;
	RwRasterDestroy = (bool(*)(RwRaster*))0x7FB020;
	RwRasterCreate = (RwRaster * (*)(int32_t, int32_t, int32_t, int32_t))0x7FB230;

	pRenderCamera = (RwCamera**)0xC1703C;
	pIntDirect3DMain = (IDirect3D8**)0xC97C20;
	pDirect3DDevice = (IDirect3DDevice8**)0xC97C28;
	pInputData = (GameDxInput**)0xC17054;
	bMenuVisible = (bool*)0xBA67A4;
	hGameWnd = (HWND*)0xC97C1C;
	pDisplayModes = (DisplayMode**)0xC97C48;
	RwD3D8AdapterInformation_DisplayMode = 0xC9BEE4;
	CamCol = 0xB72CA0;
	HookParams = 0xC9C040;
	RsGlobal = (RsGlobalType*)0xC17040;

	LOG_STREAM << "SetupHooksSA: All game pointers and addresses assigned.";

	// --- Hook 1 ---
	LOG_STREAM << "SetupHooksSA: Hooking Direct3DDeviceReplacer (MakeJMP)...";
	injector::MakeJMP(0x7F6781, HookDirect3DDeviceReplacerSA, true);
	HookDirect3DDeviceReplacerJmp = 0x7F6786;

	LOG_STREAM << "SetupHooksSA: Calling Direct3DDeviceReplaceSA()...";
	Direct3DDeviceReplaceSA();
	LOG_STREAM << "SetupHooksSA: Calling InjectWindowProc()...";
	InjectWindowProc();
	LOG_STREAM << "SetupHooksSA: WindowProc injected.";

	// --- Hook 2 (Mouse Updater) ---
	LOG_STREAM << "SetupHooksSA: Hooking Mouse Updater (Inline)...";
	struct HookDxMouseUpdater
	{
		void operator()(injector::reg_pack& regs)
		{
			if (CDxHandler::ProcessMouseState() == 0)
				*(uintptr_t*)regs.esp = 0x746F60;

			DxInputGetMouseState(regs.ecx);

		}
	}; injector::MakeInline<HookDxMouseUpdater>(0x53F417);
	LOG_STREAM << "SetupHooksSA: - HookDxMouseUpdater applied.";


	//struct HookDxInputCreateDevice
	//{
	//	void operator()(injector::reg_pack& regs)
	//	{
	//		static bool bExclusiveMode;
	//
	//		regs.eax = *(uintptr_t*)(regs.esp + 0x4);
	//		bExclusiveMode = LOBYTE(regs.eax);
	//
	//		CDxHandler::DxInputCreateDevice(bExclusiveMode);
	//
	//		*(uintptr_t*)(regs.esp) = 0x746A0C;
	//	}
	//}; injector::MakeInline<HookDxInputCreateDevice>(0x7469A0);

	// --- Hook 3 (Camera Clear Fix) ---
	LOG_STREAM << "SetupHooksSA: Hooking Camera Clear Fix (Inline)...";
	struct HookDxCameraClearFix
	{
		void operator()(injector::reg_pack& regs)
		{
			static RwCamera* pCamera;
			regs.edx = *(uintptr_t*)(regs.esp + 0x80);

			pCamera = (RwCamera*)regs.edx;
			CDxHandler::MainCameraRebuildRaster(pCamera);
		}
	}; injector::MakeInline<HookDxCameraClearFix>(0x7F7C41, 0x7F7C41 + 7);
	LOG_STREAM << "SetupHooksSA: - HookDxCameraClearFix applied.";

	// --- Hook 4 (NOP) ---
	LOG_STREAM << "SetupHooksSA: Applying NOP at 0x7481CD...";
	injector::MakeNOP(0x7481CD, 16, true);
	LOG_STREAM << "SetupHooksSA: - NOP applied.";

	// --- Hook 5 (Reload) ---
	LOG_STREAM << "SetupHooksSA: Hooking DxReload (Inline)...";
	struct HookDxReload
	{
		void operator()(injector::reg_pack& regs)
		{
			*(uintptr_t*)regs.esp = 0x748DA3;

			bStopRecursion = true;

			//ReinitializeRw(RwEngineGetCurrentVideoMode());
			RwCameraClear(*pRenderCamera, (void*)CamCol, 2);

			bStopRecursion = false;
		}
	}; injector::MakeInline<HookDxReload>(0x748C60);
	LOG_STREAM << "SetupHooksSA: - HookDxReload applied.";

	// --- Hook 6 (Resolution Change) ---
	LOG_STREAM << "SetupHooksSA: Hooking ResChangeJmp (Inline)...";
	struct HookResChangeJmp
	{
		void operator()(injector::reg_pack& regs)
		{
			*(uintptr_t*)(regs.esp) = 0x748DA3;
			bInGameSA = true;

			CDxHandler::AdjustPresentParams((D3DPRESENT_PARAMETERS_D3D9*)HookParams); // menu
		}
	}; injector::MakeInline<HookResChangeJmp>(0x748D1A);
	LOG_STREAM << "SetupHooksSA: - HookResChangeJmp applied.";


	LOG_STREAM << "SetupHooksSA: All San Andreas hooks applied successfully.";
}