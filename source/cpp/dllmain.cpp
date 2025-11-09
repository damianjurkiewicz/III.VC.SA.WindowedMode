#include "dxhandler.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
        if (reason == DLL_PROCESS_ATTACH)
        {
                CDxHandler::InitializeLogger(hModule);
                CDxHandler::LogMessage("DllMain: DLL_PROCESS_ATTACH received");
                CDxHandler::ProcessIni();

                if (injector::address_manager::singleton().IsSA())
                {
                        CDxHandler::LogMessage("DllMain: Detected GTA San Andreas executable");
                        CDxHandler::SetupHooksSA();
                }
        }

        if (reason == DLL_PROCESS_DETACH)
        {
                CDxHandler::LogMessage("DllMain: DLL_PROCESS_DETACH received");
                if (CDxHandler::ShExecInfo.hProcess != nullptr)
                        TerminateProcess(CDxHandler::ShExecInfo.hProcess, 0);
                CDxHandler::ShutdownLogger();
        }
        return TRUE;
}
