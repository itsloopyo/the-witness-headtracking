#pragma once

#include <string>
#include <tuple>
#include <vector>

// The oracle: what the dev build 0.0.0-nightly.20260916.a18847d, the only
// published build, ran on after reading HeadTracking.ini. oracle_reader.cpp
// compiles its reader from byte copies and transcribes the startup code that
// consumed it. The header names no type of the mod's, so the differential test
// can include it beside the mod's own config.h.
namespace twht_oracle {

enum Action { kToggle = 0, kCycleMode = 1, kYawMode = 2 };

// One HotkeyPoller registration: the action, the code, and 3 where the callback
// is ChordGuarded (fires only while Ctrl and Shift are both held), 0 where it is
// NavGuarded (fires unless Ctrl and Shift are both held).
using Registration = std::tuple<int, int, unsigned>;

// TrackingMode's numbers.
enum Mode { kRotationAndPosition = 0, kRotationOnly = 1, kPositionOnly = 2 };

struct Published {
    unsigned udp_port = 0;
    bool tracking_enabled = false;
    bool world_yaw = false;
    bool log_to_file = false;
    bool log_diagnostics = false;
    // What ApplyConfigToSession handed the session.
    float limit_x = 0;
    float limit_y = 0;
    float limit_y_down = 0;
    float limit_z = 0;
    float limit_z_back = 0;
    float local_smoothing = 0;
    float remote_smoothing = 0;
    int mode = -1;
    // What CameraHook read for the lean clamp.
    bool collision_enabled = false;
    float collision_margin = 0;
    float collision_release_smoothing = 0;
    std::vector<Registration> hotkeys;
};

// `game_dir` is the folder witness64_d3d11.exe is in, as Mod::Initialize
// resolved it, with no trailing backslash.
Published Read(const std::string& game_dir);

// What the dev build's Mod::Initialize writes into `game_dir` on a first start
// with no HeadTracking.ini there.
void WriteFirstRunFile(const std::string& game_dir);

}  // namespace twht_oracle
