// The dev build's reader and startup code: 0.0.0-nightly.20260916.a18847d, the
// only build published before the canonical config format.
//
// The reader is compiled from byte copies: src/core/config.cpp,
// src/core/config.h, src/core/constants.h, src/pch.h and
// cmake/default_config.h.in beside this file are a18847d's files (git show
// a18847d:<path>), which CMakeLists.txt pins by hash. default_config.h is
// generated from data/headtracking-a18847d.ini through that template, the way
// a18847d's CMakeLists.txt generated it from HeadTracking.ini. The reader is
// included inside namespace twht_oracle, after every header it includes, so
// the published TWHT::Config becomes twht_oracle::TWHT::Config and cannot
// collide with the mod's own. Every cameraunlock-core source it includes holds
// the same bytes at a18847d's pin (c480d8a) and at this repo's (CMakeLists.txt
// pins those too).
//
// The startup code is transcribed from a18847d:src/core/mod.cpp,
// a18847d:src/input/hotkey_handler.cpp and a18847d:src/camera/camera_hook.cpp,
// which hook the game and cannot be compiled into a test:
//
//   mod.cpp lines 31-68         ApplyConfigToSession, recording what it handed on
//   mod.cpp lines 118-119       the yaw mode and the enabled flag at startup
//   mod.cpp line 95             LogToFile opening the log
//   hotkey_handler.cpp 22-40    each nav key NavGuarded when not 0, and the
//                               Ctrl+Shift+Y, G and H chords ChordGuarded
//   camera_hook.cpp 454-458     ConfigureLeanClamp's margin and release
//   mod.h lines 54-55           the collision and diagnostics switches

#include "oracle_reader.h"

#include "pch.h"

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/config/ini_reader.h>
#include <cameraunlock/config/value_guards.h>
#include <cameraunlock/data/position_settings.h>
#include <cameraunlock/math/smoothing_utils.h>
#include <cameraunlock/protocol/port_utils.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace twht_oracle {
#include "core/config.cpp"
}  // namespace twht_oracle

namespace twht_oracle {

Published Read(const std::string& game_dir) {
    TWHT::Config config;
    config.LoadFromIni(game_dir + "\\" + ::TWHT::kConfigFileName);

    Published p;
    p.udp_port = config.udpPort;
    p.log_to_file = config.logToFile;
    p.log_diagnostics = config.logDiagnostics;

    // ApplyConfigToSession.
    p.limit_x = config.posLimitX;
    p.limit_y = config.posLimitY;
    p.limit_y_down = config.posLimitY;
    p.limit_z = config.posLimitZ;
    p.limit_z_back = config.posLimitZBack;
    p.local_smoothing = config.localSmoothing;
    p.remote_smoothing = config.remoteSmoothing;
    p.mode = config.positionEnabled ? kRotationAndPosition : kRotationOnly;

    p.world_yaw = config.worldSpaceYaw;
    p.tracking_enabled = config.autoEnable;

    p.collision_enabled = config.collisionEnabled;
    p.collision_margin = config.collisionMargin;
    p.collision_release_smoothing = config.collisionReleaseSmoothing;

    if (config.toggleKey != 0) p.hotkeys.emplace_back(kToggle, config.toggleKey, 0u);
    if (config.cycleModeKey != 0) p.hotkeys.emplace_back(kCycleMode, config.cycleModeKey, 0u);
    if (config.toggleYawModeKey != 0) p.hotkeys.emplace_back(kYawMode, config.toggleYawModeKey, 0u);
    p.hotkeys.emplace_back(kToggle, 'Y', 3u);
    p.hotkeys.emplace_back(kCycleMode, 'G', 3u);
    p.hotkeys.emplace_back(kYawMode, 'H', 3u);
    return p;
}

void WriteFirstRunFile(const std::string& game_dir) {
    int error = 0;
    if (!TWHT::WriteDefaultConfig(game_dir + "\\" + ::TWHT::kConfigFileName, error)) {
        throw std::runtime_error("the dev build's first-run writer failed, errno " + std::to_string(error));
    }
}

}  // namespace twht_oracle
