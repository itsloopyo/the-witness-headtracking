#pragma once

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/data/position_settings.h>
#include <cameraunlock/math/smoothing_utils.h>

#include <string>
#include <vector>

namespace TWHT {

struct Config {
    // Network. There is no bind-address key: the receiver listens on every
    // interface, and offering an address that is never applied would read as a
    // control over who can reach the port when it is nothing of the kind.
    unsigned short udpPort = 4242;

    // No sensitivity, deadzone, response curve or axis inversion, for rotation
    // or for position. The tracker owns the shape of the pose: this mod consumes
    // it at 1:1 and one tracker profile then behaves the same in every game.
    // A tracker sending an axis the wrong way round is fixed in the tracker, and
    // a protocol-to-engine sign belongs at the engine boundary in
    // pose_conversion.cpp, where it is one conversion under test rather than a
    // knob the player has to discover.

    // Smoothing. One value per connection locality, picked from the packet
    // source address. Both cover rotation and position.
    float localSmoothing  = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // Position. The limits are how far the eye may leave the body, which is a
    // game-specific camera limit and so is the mod's to own.
    bool  positionEnabled = true;
    float posLimitX     = cameraunlock::PositionSettings{}.limit_x;
    float posLimitY     = cameraunlock::PositionSettings{}.limit_y;
    float posLimitZ     = cameraunlock::PositionSettings{}.limit_z;
    float posLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;

    // Camera collision. Sweeps the engine's own world ray from the clean eye
    // toward the lean target and cuts the offset to whatever the level leaves
    // room for. Ships off until the log shows it engaging on real walls at the
    // right distance - an unverified trace either blocks on nothing or blocks
    // on everything, and both are worse than no clamp.
    bool  collisionEnabled = false;
    // Metres held off the blocking surface. Must exceed the camera's near clip
    // distance (0.06m here) or the wall is culled and the player still sees
    // through it.
    float collisionMargin  = 0.15f;
    float collisionReleaseSmoothing =
        cameraunlock::camera::LeanClampSettings{}.release_smoothing;

    // Hotkeys (virtual key codes)
    int toggleKey        = 0x23; // End
    int cycleModeKey     = 0x21; // PageUp
    int toggleYawModeKey = 0x22; // PageDown

    // General
    bool autoEnable = true;
    bool logToFile  = true;
    // Head yaw turns about the world's vertical rather than the camera's own,
    // so looking down a slope and turning the head sweeps the horizon instead
    // of tipping it. The runtime flag on Mod starts from this and Page Down
    // flips it; the alternate mode is camera-local yaw.
    bool worldSpaceYaw = true;
    // Periodic per-frame numbers - pose in, rotation and lean actually applied,
    // where the reticle was put. Off by default because it writes a line about
    // twice a second; the axis and reticle checks are what it exists for.
    bool logDiagnostics = false;

    // Diagnostics produced while parsing, one line per corrected value. The log
    // file cannot be open yet during LoadFromIni - whether to open it at all is
    // itself a config key (logToFile) - so parse-time findings are collected
    // here and replayed by Mod::Initialize, the same way the load-failed flag
    // already is. Empty on a clean parse.
    std::vector<std::string> warnings;

    // Loads from the given INI path. Missing keys keep their defaults.
    // Returns false if the file cannot be opened.
    bool LoadFromIni(const std::string& path);
};

// Writes the shipped HeadTracking.ini, byte for byte, to `path`. Used when no
// config file is next to the game exe - a manual install that copied only the
// DLL, a user who deleted the ini to reset it, a launcher install that deployed
// the payload without it.
//
// Not a second set of defaults. The text is the repo's own HeadTracking.ini,
// compiled in by CMake, so the file the user is handed is the file the docs
// describe, comments included. A generated copy is how the two drifted before:
// it spelled its booleans 1 and 0 where every other source says true and false,
// wrote 0.3 for 0.30, and carried none of the explanations.
//
// False when the file cannot be written, with `error` set to the errno the
// failing call left. Returned rather than read from GetLastError by the caller,
// because the close happens inside here and would overwrite it.
bool WriteDefaultConfig(const std::string& path, int& error);

} // namespace TWHT
