#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Hooks/ExeIdentity.h"
#include "Hooks/Hook.h"
#include "Hooks/Log.h"
#include "TrafficAI/AIActionTraffic.h"
#include "TrafficAI/AITrafficManager.h"
#include "TrafficAI/AIVehicleTraffic.h"
#include "TrafficAI/RandomSurveyor.h"

static_assert(sizeof(void*) == 4, "ImpTrafficAI hooks a 32-bit game and must be built for x86.");

namespace {

    bool gInstalled = false;

    void Install(HMODULE module) {
        Log::Open(module);
        Log::Line("ImpTrafficAI starting.");
        if (!ExeIdentity::IsSupported()) return;

        AIActionTraffic::Install();
        AIVehicleTraffic::Install();
        RandomSurveyor::Install();
        AITrafficManager::Install();
        gInstalled = true;

        const int skipped = Hook::Failures();
        if (skipped == 0)
            Log::Line("Ready.");
        else
            Log::Line("Ready, but %d hook(s) were skipped; see the lines above.", skipped);
    }

    void Uninstall() {
        if (gInstalled) {
            Hook::RemoveAll();
            gInstalled = false;
        }
        Log::Close();
    }

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        Install(module);
        break;

    case DLL_PROCESS_DETACH:
        if (reserved == nullptr)
            Uninstall();
        else
            Log::Close();
        break;

    default:
        break;
    }
    return TRUE;
}
