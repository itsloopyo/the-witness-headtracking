#include "pch.h"
#include "core/early_diag.h"

namespace TWHT {

namespace {

// This DLL's own path. Taken from an address inside it rather than from a
// handle passed down, so the OpenVR forwarder can write a line without a
// DllMain parameter to hand around; the two resolve to the same module.
bool DiagPath(std::string& out) {
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&DiagPath), &self)) {
        return false;
    }

    char dllPath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(self, dllPath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;

    out.assign(dllPath, len);
    const auto dot = out.rfind('.');
    if (dot != std::string::npos) out = out.substr(0, dot);
    out += "_diag.txt";
    return true;
}

} // namespace

void WriteEarlyDiag(const char* message) {
    std::string diagPath;
    if (!DiagPath(diagPath)) return;

    // Truncate on the first write of each launch, then append. Without this the
    // file accumulates every session the mod has ever run, so a user's bug
    // report arrives with the current launch buried at the bottom.
    //
    // Atomic because the forwarder's callers are the game's own threads, which
    // is a different thread from DllMain's.
    static std::atomic<bool> s_truncated{false};
    const bool firstWrite = !s_truncated.exchange(true, std::memory_order_acq_rel);

    FILE* f = std::fopen(diagPath.c_str(), firstWrite ? "w" : "a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::fprintf(f, "[%02d:%02d:%02d.%03d] %s\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
    std::fclose(f);
}

} // namespace TWHT
