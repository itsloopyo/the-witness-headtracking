#pragma once

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/config/config_concepts.g.h>
#include <cameraunlock/config/config_owner.h>
#include <cameraunlock/config/config_table.h>
#include <cameraunlock/config/defaults_file.h>
#include <cameraunlock/config/legacy_import.h>
#include <cameraunlock/data/position_settings.h>
#include <cameraunlock/math/smoothing_utils.h>
#include <cameraunlock/tracking/tracking_mode.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace TWHT {

// The settings CameraUnlock.ini holds, at their defaults.
//
// No sensitivity, deadzone, response curve or axis inversion, for rotation or
// for position. The tracker owns the shape of the pose: this mod consumes it at
// 1:1 and one tracker profile then behaves the same in every game.
struct Config {
    // Held as the socket's own type so no value outside the port range reaches
    // UdpReceiver::Start.
    std::uint16_t udpPort = 4242;

    bool enableOnStartup = true;

    // The tracking mode at startup, the pair the mode hotkey saves.
    bool rotationEnabled = true;
    bool positionEnabled = true;

    // Head yaw turns about the world's vertical rather than the camera's own, so
    // looking down a slope and turning the head sweeps the horizon instead of
    // tipping it. The yaw hotkey saves it.
    bool worldSpaceYaw = true;

    // One value per connection locality, picked from the packet source address.
    // Both cover rotation and position.
    float localSmoothing  = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // How far the eye may leave the body, in metres.
    float posLimitX     = cameraunlock::PositionSettings{}.limit_x;
    float posLimitY     = cameraunlock::PositionSettings{}.limit_y;
    float posLimitYDown = cameraunlock::PositionSettings{}.limit_y_down;
    float posLimitZ     = cameraunlock::PositionSettings{}.limit_z;
    float posLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;

    // Camera collision. Sweeps the engine's own world ray from the clean eye
    // toward the lean target and cuts the offset to whatever the level leaves
    // room for.
    bool  collisionEnabled = true;
    // Metres held off the blocking surface. Must exceed the camera's near clip
    // distance (0.06m here) or the wall is culled and the player still sees
    // through it.
    float collisionMargin  = 0.15f;
    float collisionReleaseSmoothing =
        cameraunlock::camera::LeanClampSettings{}.release_smoothing;

    std::string toggleKey = cameraunlock::config::schema::ConceptTraits<
        cameraunlock::config::schema::Concept::ToggleKey>::kCanonicalDefault;
    std::string cycleTrackingModeKey = cameraunlock::config::schema::ConceptTraits<
        cameraunlock::config::schema::Concept::CycleTrackingModeKey>::kCanonicalDefault;
    std::string yawModeKey = cameraunlock::config::schema::ConceptTraits<
        cameraunlock::config::schema::Concept::YawModeKey>::kCanonicalDefault;

    bool logToFile = true;
    // Periodic per-frame numbers - pose in, rotation and lean actually applied,
    // where the reticle was put. Off by default because it writes a line about
    // twice a second.
    bool logDiagnostics = false;
};

// CameraUnlock.ini, beside witness64_d3d11.exe, in cameraunlock-core's canonical
// config format. One ConfigOwner reads and writes it; nothing else in the mod
// touches it. HeadTracking.ini, the file the earlier build read, is imported
// once while CameraUnlock.ini is absent and is never written.
namespace config {

cameraunlock::config::ConfigTable<Config> Table();

cameraunlock::config::RenderHeader Header();

// HeadTracking.ini through the frozen reader in src/legacy_config/, mapped into
// Config.
cameraunlock::config::LegacyImport<Config> Import();

// The owner's options for CameraUnlock.ini in `folder`, with HeadTracking.ini
// beside it as the legacy file and Defaults.ini where `defaults` says.
cameraunlock::config::ConfigOwnerOptions<Config> OwnerOptions(const std::filesystem::path& folder,
                                                              cameraunlock::config::DefaultsFile defaults);

// Reads, imports or creates CameraUnlock.ini in `folder`. Call once, from the
// init thread. The log cannot be open yet, since whether to open it is itself a
// setting, so the caller writes the result's lines once it has opened it.
// `defaults` is DefaultsFile::PerUser() in the mod.
cameraunlock::config::ConfigLoadResult<Config> Load(const std::filesystem::path& folder,
                                                    cameraunlock::config::DefaultsFile defaults);

// The corrections the frozen reader made to HeadTracking.ini's values during
// the last import, one line each, for the caller to log with the load's lines.
const std::vector<std::string>& ImportWarnings();

// A concept row takes the schema's range, and CollisionMargin's is 0 and up,
// since its unit is each engine's. The earlier build read 0.06 to 10 metres and
// kept the default outside it: a margin inside the camera's 0.06 m near clip
// lets the wall the view is held off be culled.
constexpr float kMinCollisionMargin = 0.06f;
constexpr float kMaxCollisionMargin = 10.0f;

// Puts a CollisionMargin outside kMinCollisionMargin to kMaxCollisionMargin
// back to the default. False when it did, for the caller to log.
bool KeepCollisionMarginInRange(Config& config);

// The tracking mode the settings start in. The table never gives both rows
// false.
cameraunlock::TrackingMode StartupTrackingMode(const Config& config);

// What the position processor runs on: the limits and the smoothing pair.
cameraunlock::PositionSettings ToPositionSettings(const Config& config);

// Save the mode the cycle hotkey, and the yaw mode the yaw hotkey, has just
// applied. The session keeps it whether or not the save succeeds; a failed save
// is logged. Called on the hotkey thread.
void SaveTrackingMode(cameraunlock::TrackingMode mode);
void SaveWorldSpaceYaw(bool worldSpaceYaw);

}  // namespace config

} // namespace TWHT
