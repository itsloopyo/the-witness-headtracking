#include "pch.h"
#include "core/config.h"

#include "core/debug_log.h"
#include "legacy_config/legacy_config.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TWHT {

namespace config {

namespace {

namespace cfg = ::cameraunlock::config;
using cfg::schema::Concept;

constexpr const wchar_t* kIniName = L"CameraUnlock.ini";
constexpr const wchar_t* kLegacyIniName = L"HeadTracking.ini";

// data/games.json's display_name for the-witness.
constexpr const char* kDisplayName = "The Witness";

std::unique_ptr<cfg::ConfigOwner<Config>> g_owner;
std::vector<std::string> g_importWarnings;

cfg::ImportResult RunImport(const cfg::LegacyInput& input, Config& out) {
    legacy::Config read;
    const bool present = read.LoadFromIni(input.ansi_path);
    g_importWarnings = read.warnings;

    std::vector<cfg::DroppedValue> dropped;

    // The reader keeps the port inside 1024-65535, each smoothing value finite
    // and inside 0-1, each limit inside 0.01-10 and the collision margin inside
    // 0.06-10, so all of them carry over as they are.
    out.udpPort = read.udpPort;
    out.enableOnStartup = read.autoEnable;
    out.worldSpaceYaw = read.worldSpaceYaw;
    out.localSmoothing = read.localSmoothing;
    out.remoteSmoothing = read.remoteSmoothing;
    out.collisionEnabled = read.collisionEnabled;
    out.collisionMargin = read.collisionMargin;
    out.collisionReleaseSmoothing = read.collisionReleaseSmoothing;
    out.logToFile = read.logToFile;
    out.logDiagnostics = read.logDiagnostics;

    // [Position] Enabled chose the startup mode and nothing else: the cycle
    // reached every mode either way.
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(
        read.positionEnabled ? cameraunlock::TrackingMode::RotationAndPosition
                             : cameraunlock::TrackingMode::RotationOnly);
    out.rotationEnabled = channels.rotation_enabled;
    out.positionEnabled = channels.position_enabled;

    // LimitY set both vertical bounds.
    out.posLimitX = read.posLimitX;
    out.posLimitY = read.posLimitY;
    out.posLimitYDown = read.posLimitY;
    out.posLimitZ = read.posLimitZ;
    out.posLimitZBack = read.posLimitZBack;

    // Each action had a key that fired with Ctrl and Shift not both held, and a
    // Ctrl+Shift chord fixed in code. A key of 0 is one the reader unbound as a
    // duplicate, which leaves the chord alone.
    const auto bindings = [&](int key, const char* keyName, const char* chord) {
        std::string list = cfg::LegacyVirtualKeyToBindings(key, "Hotkeys", keyName, dropped);
        list += (list.empty() ? "Ctrl+Shift+" : ", Ctrl+Shift+") + std::string(chord);
        return list;
    };
    out.toggleKey = bindings(read.toggleKey, "ToggleKey", "Y");
    out.cycleTrackingModeKey = bindings(read.cycleModeKey, "CycleModeKey", "G");
    out.yawModeKey = bindings(read.toggleYawModeKey, "ToggleYawModeKey", "H");

    // A setting the player never changed from what the dev build shipped
    // follows Defaults.ini. LimitY stood for both vertical bounds. The chords
    // were fixed in code, so each hotkey's code decides alone. CollisionMargin
    // is not global.
    const legacy::Config shipped;
    cfg::LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, read.udpPort, shipped.udpPort);
    follows.Setting(Concept::EnableOnStartup, read.autoEnable, shipped.autoEnable);
    follows.Setting(Concept::WorldSpaceYaw, read.worldSpaceYaw, shipped.worldSpaceYaw);
    follows.TrackingMode(read.positionEnabled, shipped.positionEnabled);
    follows.Setting(Concept::LocalSmoothing, read.localSmoothing, shipped.localSmoothing);
    follows.Setting(Concept::RemoteSmoothing, read.remoteSmoothing, shipped.remoteSmoothing);
    follows.Setting(Concept::PositionLimitX, read.posLimitX, shipped.posLimitX);
    follows.Setting(Concept::PositionLimitY, read.posLimitY, shipped.posLimitY);
    follows.Setting(Concept::PositionLimitYDown, read.posLimitY, shipped.posLimitY);
    follows.Setting(Concept::PositionLimitZ, read.posLimitZ, shipped.posLimitZ);
    follows.Setting(Concept::PositionLimitZBack, read.posLimitZBack, shipped.posLimitZBack);
    // Shipped off pending verification in game (follows_default): an untouched
    // false takes the schema's true through Defaults.ini.
    follows.Setting(Concept::CollisionEnabled, read.collisionEnabled, shipped.collisionEnabled);
    follows.Setting(Concept::CollisionReleaseSmoothing, read.collisionReleaseSmoothing,
                    shipped.collisionReleaseSmoothing);
    follows.Setting(Concept::ToggleKey, read.toggleKey, shipped.toggleKey);
    follows.Setting(Concept::CycleTrackingModeKey, read.cycleModeKey, shipped.cycleModeKey);
    follows.Setting(Concept::YawModeKey, read.toggleYawModeKey, shipped.toggleYawModeKey);

    return present ? cfg::ImportResult::Imported(std::move(dropped), {}, follows.Concepts())
                   : cfg::ImportResult::Absent(std::move(dropped), {}, follows.Concepts());
}

void LogSave(const cfg::ConfigSaveResult& result, const char* rows) {
    if (result.status != cfg::ConfigSaveStatus::Saved) {
        HT_LOG("Config: %s %s: %s", rows, cfg::ConfigSaveStatusName(result.status), result.reason.c_str());
    }
    for (const std::string& line : result.log) HT_LOG("Config: %s", line.c_str());
}

}  // namespace

cfg::ConfigTable<Config> Table() {
    cfg::ConfigTable<Config> table;
    table.Concept<Concept::UdpPort>(&Config::udpPort)
        .Concept<Concept::EnableOnStartup>(&Config::enableOnStartup)
        .Concept<Concept::WorldSpaceYaw>(&Config::worldSpaceYaw)
        .Writable()
        .Concept<Concept::RotationEnabled>(&Config::rotationEnabled)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::localSmoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remoteSmoothing)
        .Concept<Concept::PositionEnabled>(&Config::positionEnabled)
        .Writable()
        .Concept<Concept::PositionLimitX>(&Config::posLimitX)
        .Concept<Concept::PositionLimitY>(&Config::posLimitY)
        .Concept<Concept::PositionLimitYDown>(&Config::posLimitYDown)
        .Concept<Concept::PositionLimitZ>(&Config::posLimitZ)
        .Concept<Concept::PositionLimitZBack>(&Config::posLimitZBack)
        .Concept<Concept::CollisionEnabled>(&Config::collisionEnabled)
        .Concept<Concept::CollisionMargin>(&Config::collisionMargin)
        .Comment("How far, in metres, the view is held off a wall when you lean into it. 0.06 to 10.\n"
                 "Below 0.06 the wall is inside the camera's near clip and is not drawn, so you would\n"
                 "see through it anyway.")
        .Concept<Concept::CollisionReleaseSmoothing>(&Config::collisionReleaseSmoothing)
        .Concept<Concept::ToggleKey>(&Config::toggleKey)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycleTrackingModeKey)
        .Concept<Concept::YawModeKey>(&Config::yawModeKey)
        .Local("General", "LogToFile", &Config::logToFile, cfg::BoolCodec(),
               "true: write HeadTracking.log beside witness64_d3d11.exe, new at every launch, with the\n"
               "launch before kept as HeadTracking.prev.log. Attach it to a bug report.")
        .Local("General", "LogDiagnostics", &Config::logDiagnostics, cfg::BoolCodec(),
               "true: HeadTracking.log also gets the pose, the rotation and lean applied, and the\n"
               "reticle position about twice a second. For diagnosing a wrong axis.");
    return table;
}

cfg::RenderHeader Header() {
    cfg::RenderHeader header;
    header.display_name = kDisplayName;
    return header;
}

cfg::LegacyImport<Config> Import() {
    cfg::LegacyImport<Config> import;
    import.run = &RunImport;
    for (const legacy::Key& key : legacy::ReadKeys()) import.keys.push_back({key.section, key.key});
    return import;
}

cfg::ConfigOwnerOptions<Config> OwnerOptions(const std::filesystem::path& folder, cfg::DefaultsFile defaults) {
    cfg::ConfigOwnerOptions<Config> options;
    options.path = (folder / kIniName).wstring();
    options.table = Table();
    options.import = Import();
    options.legacy_path = (folder / kLegacyIniName).wstring();
    options.header = Header();
    options.defaults = std::move(defaults);
    return options;
}

cfg::ConfigLoadResult<Config> Load(const std::filesystem::path& folder, cfg::DefaultsFile defaults) {
    g_owner = std::make_unique<cfg::ConfigOwner<Config>>(OwnerOptions(folder, std::move(defaults)));
    return g_owner->Load();
}

const std::vector<std::string>& ImportWarnings() { return g_importWarnings; }

bool KeepCollisionMarginInRange(Config& config) {
    if (config.collisionMargin >= kMinCollisionMargin && config.collisionMargin <= kMaxCollisionMargin) return true;
    config.collisionMargin = Config{}.collisionMargin;
    return false;
}

cameraunlock::TrackingMode StartupTrackingMode(const Config& config) {
    const auto mode = cameraunlock::DecodeTrackingMode(config.rotationEnabled, config.positionEnabled);
    if (!mode) throw std::logic_error("RotationEnabled and PositionEnabled are both false, which the table never gives");
    return *mode;
}

cameraunlock::PositionSettings ToPositionSettings(const Config& config) {
    cameraunlock::PositionSettings position;
    position.limit_x = config.posLimitX;
    position.limit_y = config.posLimitY;
    position.limit_y_down = config.posLimitYDown;
    position.limit_z = config.posLimitZ;
    position.limit_z_back = config.posLimitZBack;
    position.local_smoothing = config.localSmoothing;
    position.remote_smoothing = config.remoteSmoothing;
    return position;
}

void SaveTrackingMode(cameraunlock::TrackingMode mode) {
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(mode);
    LogSave(g_owner->Save([channels](Config& c) {
                c.rotationEnabled = channels.rotation_enabled;
                c.positionEnabled = channels.position_enabled;
            }),
            "[General] RotationEnabled and [Position] PositionEnabled");
}

void SaveWorldSpaceYaw(bool worldSpaceYaw) {
    LogSave(g_owner->Save([worldSpaceYaw](Config& c) { c.worldSpaceYaw = worldSpaceYaw; }),
            "[General] WorldSpaceYaw");
}

}  // namespace config

} // namespace TWHT
