#include "pch.h"

#include "camera/camera_hook.h"
#include "core/early_diag.h"
#include "core/mod.h"
#include "core/window_centering.h"

static HMODULE g_hModule = nullptr;

// Long enough for the engine to finish its own startup before the hooks go
// in, and for the config read not to contend with the game's own disk work.
static constexpr DWORD kInitDelayMs = 3000;

static DWORD WINAPI InitThread(LPVOID) {
    TWHT::WriteEarlyDiag("InitThread: sleeping 3s before init");
    Sleep(kInitDelayMs);
    TWHT::WriteEarlyDiag("InitThread: calling Mod::Initialize");
    if (!TWHT::Mod::Instance().Initialize(g_hModule)) {
        TWHT::WriteEarlyDiag("InitThread: Initialize FAILED");
        return 0;
    }
    TWHT::WriteEarlyDiag("InitThread: Initialize OK");

    // Last, because it blocks for as long as the engine takes to place its
    // window: the hooks and the receiver are up before this waits on anything.
    // Skipped on a build the mod does not recognise, where no hook was
    // installed - dormant covers the player's desktop as well as the process.
    if (TWHT::CameraHook::Instance().IsActive()) {
        TWHT::CenterWindowWhenReady();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        // Refuse to be unloaded rather than tear down under the loader lock.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN
                                | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                reinterpret_cast<LPCWSTR>(&DllMain), &pinned)) {
            // The whole reason there is no detach handling below. Unpinned, a
            // FreeLibrary would pull MinHook's trampolines out from under a live
            // render thread, so this is worth a line even though the game
            // imports this DLL statically and should never release it.
            TWHT::WriteEarlyDiag("DllMain: WARNING - could not pin the module");
        }
        TWHT::WriteEarlyDiag("DllMain: DLL_PROCESS_ATTACH");

        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    // No DLL_PROCESS_DETACH handling, and the pin above is why there is nothing
    // to handle. Shutting down here is unsafe in both of the cases that reach
    // it: with the process terminating the worker threads are already gone,
    // possibly holding a lock; with a FreeLibrary the joins wait on the loader
    // lock this function holds, and removing the hooks frees a trampoline the
    // render thread may be about to return through. The game imports
    // openvr_api.dll statically, so its refcount never falls to zero anyway.
    return TRUE;
}
