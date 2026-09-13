#pragma once

namespace TWHT {

constexpr const char* kModName    = "TheWitnessHeadTracking";
constexpr const char* kModVersion = "0.0.0";

constexpr const char*    kConfigFileName = "HeadTracking.ini";
constexpr const wchar_t* kLogFileName    = L"HeadTracking.log";

// Renamed-original DLL we load and forward OpenVR exports to. The
// install script renames the stock openvr_api.dll to this name when it
// drops our shim in.
constexpr const char* kOriginalOpenVRBackup = "openvr_api.dll.backup";

// What the Lopari launcher renames the displaced DLL to when it deploys this
// package through launcher-manifest.json's `backup: true`. Its suffix is its
// own and does not match the install script's, so the shim tries both.
constexpr const char* kLauncherOpenVRBackup = "openvr_api.dll.lopari-backup";

} // namespace TWHT
