#include "pch.h"
#include "core/config.h"

#include "default_config.h"

#include "legacy_config/legacy_config.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace TWHT {

bool Config::LoadFromIni(const std::string& path) {
    legacy::Config read;
    if (!read.LoadFromIni(path)) return false;

    udpPort = read.udpPort;
    localSmoothing = read.localSmoothing;
    remoteSmoothing = read.remoteSmoothing;
    positionEnabled = read.positionEnabled;
    posLimitX = read.posLimitX;
    posLimitY = read.posLimitY;
    posLimitZ = read.posLimitZ;
    posLimitZBack = read.posLimitZBack;
    collisionEnabled = read.collisionEnabled;
    collisionMargin = read.collisionMargin;
    collisionReleaseSmoothing = read.collisionReleaseSmoothing;
    toggleKey = read.toggleKey;
    cycleModeKey = read.cycleModeKey;
    toggleYawModeKey = read.toggleYawModeKey;
    autoEnable = read.autoEnable;
    logToFile = read.logToFile;
    worldSpaceYaw = read.worldSpaceYaw;
    logDiagnostics = read.logDiagnostics;
    warnings = read.warnings;
    return true;
}

bool WriteDefaultConfig(const std::string& path, int& error) {
    error = 0;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        error = errno;
        return false;
    }

    const std::size_t length = std::strlen(kDefaultConfigIni);
    const bool wrote = std::fwrite(kDefaultConfigIni, 1, length, f) == length;
    if (!wrote) error = errno;
    const bool closed = std::fclose(f) == 0;
    if (wrote && !closed) error = errno;
    return wrote && closed;
}

} // namespace TWHT
