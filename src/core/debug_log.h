#pragma once

#include <cameraunlock/logging/file_log.h>

namespace TWHT {

// Opens HeadTracking.log next to the game EXE (truncated each launch), keeping
// the previous launch's file as HeadTracking.prev.log. HT_LOG lines emitted
// before this are dropped, matching file_log's not-open behavior.
void OpenLogFile();

} // namespace TWHT

#define HT_LOG(...) ::cameraunlock::logging::Line(__VA_ARGS__)
