// OpenVR API proxy.
//
// The Witness loads openvr_api.dll natively at startup (it ships its own
// copy next to the executable for built-in VR support). We hijack that
// load path: the installer renames the stock DLL to
// `openvr_api.dll.backup`, drops our shim in as `openvr_api.dll`, and
// when the game starts it calls our DllMain.
//
// Every export of the original DLL is re-exported here. On first use
// each one lazily resolves the matching function pointer from the
// backed-up real DLL via LoadLibraryA + GetProcAddress and tail-calls
// it, so the game's actual VR initialization path is unchanged.
//
// Signatures follow the public OpenVR SDK (headers/openvr.h). We don't
// link the SDK; we only need wide-enough prototypes for the x64
// calling convention to forward arguments cleanly.

#include "pch.h"

#include "core/constants.h"
#include "core/debug_log.h"
#include "core/early_diag.h"

namespace {

HMODULE g_realOpenVR = nullptr;
std::once_flag g_loadOnce;

void LoadRealOpenVR() {
    char dllPath[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                       | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&LoadRealOpenVR), &self);
    DWORD n = GetModuleFileNameA(self, dllPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        // Every other failure below is reported twice over; this one used to
        // return in silence, leaving the game with no VR and both log files
        // empty. A Steam library nested deep enough to pass MAX_PATH gets here.
        char msg[128] = {};
        std::snprintf(msg, sizeof(msg),
                      "ERROR: could not read this DLL's own path (len=%lu, err=%lu). "
                      "Game VR features will not work until uninstalled.",
                      n, GetLastError());
        TWHT::WriteEarlyDiag(msg);
        return;
    }

    std::string path(dllPath, n);
    auto slash = path.find_last_of("\\/");
    std::string dir = (slash == std::string::npos) ? std::string{} : path.substr(0, slash + 1);
    // Two names, because two things displace the game's own openvr_api.dll and
    // they rename it differently: install.cmd writes `.backup`, and the Lopari
    // launcher's deploy engine writes `.lopari-backup`. Chaining through only
    // one leaves a launcher-installed copy with no VR and a log line nobody
    // connects to the installer that produced it.
    const std::string installerPath = dir + TWHT::kOriginalOpenVRBackup;
    const std::string launcherPath = dir + TWHT::kLauncherOpenVRBackup;

    g_realOpenVR = LoadLibraryA(installerPath.c_str());
    const DWORD installerError = g_realOpenVR ? 0 : GetLastError();
    DWORD launcherError = 0;
    if (!g_realOpenVR) {
        g_realOpenVR = LoadLibraryA(launcherPath.c_str());
        launcherError = g_realOpenVR ? 0 : GetLastError();
    }
    if (!g_realOpenVR) {
        // The game resolves these exports during its own startup, three seconds
        // before the init thread opens HeadTracking.log, so HT_LOG alone drops
        // the one line that explains a broken install. It goes to the early
        // diagnostic file as well - which is the file the README's
        // troubleshooting section already sends people to.
        // Both candidates named with their own error. One installer writes
        // each, so quoting a single path hands half the users a filename their
        // installer never created, alongside an error code for a different file.
        char msg[2 * MAX_PATH + 192] = {};
        std::snprintf(msg, sizeof(msg),
                      "ERROR: Failed to load the original OpenVR DLL at %s (err=%lu) "
                      "or %s (err=%lu). Game VR features will not work until "
                      "uninstalled.",
                      installerPath.c_str(), installerError,
                      launcherPath.c_str(), launcherError);
        TWHT::WriteEarlyDiag(msg);
        HT_LOG("%s", msg);
    }
}

template <typename Fn>
Fn Resolve(const char* name) {
    std::call_once(g_loadOnce, LoadRealOpenVR);
    if (!g_realOpenVR) return nullptr;
    return reinterpret_cast<Fn>(GetProcAddress(g_realOpenVR, name));
}

} // namespace

// --- OpenVR export signatures (from public openvr.h) ----------------

// Most return uint32_t (EVRInitError / token), bool, or const char*.
// Pointer args are forwarded verbatim. The Witness uses the standard
// VR_InitInternal / VR_GetGenericInterface path; the others are
// resolved on demand by callers that probe for them.

using FnVRControlPanel                       = void*       (*)();
using FnVRDashboardManager                   = void*       (*)();
using FnVRTrackedCamera                      = void*       (*)();
using FnVR_GetGenericInterface               = void*       (*)(const char*, uint32_t*);
using FnVR_GetInitToken                      = uint32_t    (*)();
using FnVR_GetStringForHmdError              = const char* (*)(uint32_t);
using FnVR_GetVRInitErrorAsEnglishDescription= const char* (*)(uint32_t);
using FnVR_GetVRInitErrorAsSymbol            = const char* (*)(uint32_t);
using FnVR_InitInternal                      = uint32_t    (*)(uint32_t*, uint32_t);
using FnVR_IsHmdPresent                      = bool        (*)();
using FnVR_IsInterfaceVersionValid           = bool        (*)(const char*);
using FnVR_IsRuntimeInstalled                = bool        (*)();
using FnVR_RuntimePath                       = const char* (*)();
using FnVR_ShutdownInternal                  = void        (*)();

#define FORWARD0(ret, name, defaultExpr) \
    extern "C" __declspec(dllexport) ret name() { \
        auto fn = Resolve<Fn##name>(#name); \
        return fn ? fn() : (defaultExpr); \
    }

#define FORWARD1(ret, name, T1, defaultExpr) \
    extern "C" __declspec(dllexport) ret name(T1 a1) { \
        auto fn = Resolve<Fn##name>(#name); \
        return fn ? fn(a1) : (defaultExpr); \
    }

#define FORWARD2(ret, name, T1, T2, defaultExpr) \
    extern "C" __declspec(dllexport) ret name(T1 a1, T2 a2) { \
        auto fn = Resolve<Fn##name>(#name); \
        return fn ? fn(a1, a2) : (defaultExpr); \
    }

#define FORWARD0_VOID(name) \
    extern "C" __declspec(dllexport) void name() { \
        auto fn = Resolve<Fn##name>(#name); \
        if (fn) fn(); \
    }

// Init / shutdown.
FORWARD2(uint32_t,    VR_InitInternal,         uint32_t*, uint32_t,  108) // VRInitError_Init_NoServerForBackgroundApp = 108
FORWARD0_VOID(VR_ShutdownInternal)

// Presence / availability probes.
FORWARD0(bool,        VR_IsHmdPresent,         false)
FORWARD0(bool,        VR_IsRuntimeInstalled,   false)
FORWARD0(const char*, VR_RuntimePath,          nullptr)

// Error formatting.
FORWARD1(const char*, VR_GetStringForHmdError,                uint32_t, "")
FORWARD1(const char*, VR_GetVRInitErrorAsSymbol,              uint32_t, "")
FORWARD1(const char*, VR_GetVRInitErrorAsEnglishDescription,  uint32_t, "")

// Interface resolution.
FORWARD2(void*,       VR_GetGenericInterface,  const char*, uint32_t*, nullptr)
FORWARD1(bool,        VR_IsInterfaceVersionValid, const char*, false)
FORWARD0(uint32_t,    VR_GetInitToken,         0)

// Legacy singleton accessors.
FORWARD0(void*,       VRControlPanel,          nullptr)
FORWARD0(void*,       VRDashboardManager,      nullptr)
FORWARD0(void*,       VRTrackedCamera,         nullptr)
