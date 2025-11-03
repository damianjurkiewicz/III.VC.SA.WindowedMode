#include "dxhandler.h"

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// Sprawdź, czy to San Andreas
		if (injector::address_manager::singleton().IsSA())
		{
			// 1. Ustaw flagę, że jesteśmy w SA (jeśli jej używasz)
			CDxHandler::bInGameSA = true;

			// 2. Wczytaj konfigurację z pliku WindowedMode.ini
			CDxHandler::ProcessIni();

			// 3. Zainstaluj nasz nowy, uniwersalny hak D3D9
			// To zastępuje stare wywołanie CDxHandler::SetupHooksSA();
			CDxHandler::InstallD3D9Hook();
		}
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		if (CDxHandler::ShExecInfo.hProcess != nullptr)
			TerminateProcess(CDxHandler::ShExecInfo.hProcess, 0);
	}
	return TRUE;
}