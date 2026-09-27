#pragma once

#include <string>
#include <vector>

// The pre-canonical HeadTracking.ini reader, frozen. It reads a file the way the
// dev build 0.0.0-nightly.20260916.a18847d, the only build published before the
// canonical config format, did, so a player's old file is carried over as that
// build read it. Never edit anything in this folder: CMakeLists.txt pins every
// file here by hash.
//
// Frozen from src/core/config.cpp and src/core/config.h at 4737ab7, which hold
// a18847d's bytes, with three changes: it fills this frozen copy of that
// commit's Config and its defaults instead of the mod's own, it writes nothing
// (WriteDefaultConfig, the first-run writer, is not part of it), and it lives in
// namespace TWHT::legacy. The defaults are the literals the code held then
// (cameraunlock-core's smoothing, PositionSettings and LeanClampSettings
// defaults), so a later change to core cannot move what an old file means.
namespace TWHT::legacy {

struct Config {
    unsigned short udpPort = 4242;

    float localSmoothing  = 0.0f;
    float remoteSmoothing = 0.15f;

    // Chose the startup mode, rotation and position or rotation only. The mode
    // hotkey cycled through all three modes either way.
    bool  positionEnabled = true;
    // LimitY set both vertical bounds, up and down.
    float posLimitX     = 0.30f;
    float posLimitY     = 0.20f;
    float posLimitZ     = 0.40f;
    float posLimitZBack = 0.10f;

    bool  collisionEnabled = false;
    float collisionMargin  = 0.15f;
    float collisionReleaseSmoothing = 0.9f;

    // Virtual key codes, each registered NavGuarded (fires unless Ctrl and Shift
    // are both held). 0 is a binding the reader dropped as a duplicate. The
    // Ctrl+Shift+Y, G and H chords were fixed in code and read from no key.
    int toggleKey        = 0x23;
    int cycleModeKey     = 0x21;
    int toggleYawModeKey = 0x22;

    bool autoEnable = true;
    bool logToFile  = true;
    bool worldSpaceYaw = true;
    bool logDiagnostics = false;

    // One line per corrected value, for the caller to log.
    std::vector<std::string> warnings;

    // Reads the file at `path` over this Config. Missing keys keep their
    // defaults. Returns false, leaving the Config untouched, when the file
    // cannot be opened.
    bool LoadFromIni(const std::string& path);
};

struct Key {
    const char* section;
    const char* key;
};

// Every key LoadFromIni takes a value from. The retired keys it reads only to
// warn that they are ignored are not among them.
std::vector<Key> ReadKeys();

} // namespace TWHT::legacy
