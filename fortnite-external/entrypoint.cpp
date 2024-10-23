#include <core/core.hpp>

auto main() -> int
{
    if (atx::core.Initialize())
    {
        printf(_("[ core ] core failure whilst initializing"));
        std::cin.get();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

        std::thread([&]()
            {              if (!atx::core.Initialize())
        {
            MessageBoxA(GetForegroundWindow(), _("core error whilst initializing.\ncheck console output for error."), _("core error"), MB_OK | MB_ICONERROR);
        }
        ExitProcess(0);
            }).detach();
            break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}