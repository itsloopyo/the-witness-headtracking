#include "pch.h"
#include "core/debug_log.h"

#include "core/constants.h"

namespace TWHT {

namespace {

// Directory of the running EXE, with its trailing separator. Empty when the
// path cannot be read, which puts the log in the working directory rather than
// nowhere.
std::wstring GameDirectory() {
    wchar_t buf[MAX_PATH] = {};
    // GetModuleFileNameW does not guarantee null-termination on truncation,
    // so bound the path by the returned length.
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return {};

    const std::wstring exe(buf, len);
    const auto slash = exe.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    return exe.substr(0, slash + 1);
}

} // namespace

void OpenLogFile() {
    // Open() keeps one previous generation itself, renaming HeadTracking.log to
    // HeadTracking.prev.log before it truncates and writing a warning into the
    // fresh file when that rename fails. The generation is worth keeping because
    // the session to diagnose is usually the one that just crashed, and the
    // player relaunches the game before sending the log.
    cameraunlock::logging::Open(GameDirectory() + kLogFileName);
}

} // namespace TWHT
