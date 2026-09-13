#pragma once

namespace TWHT {

// Appends one timestamped line to <dll>_diag.txt next to this DLL, truncating
// on the first write of each launch.
//
// Separate from HeadTracking.log on purpose, and not a duplicate of it: the log
// file does not exist until the init thread has read the config three seconds
// in - whether to open it at all is a config key - so anything that has to be
// recorded before that, or that explains why the log never appeared, has
// nowhere else to go. The OpenVR forwarder is the other caller: the game calls
// its exports during startup, long before the log opens.
void WriteEarlyDiag(const char* message);

} // namespace TWHT
