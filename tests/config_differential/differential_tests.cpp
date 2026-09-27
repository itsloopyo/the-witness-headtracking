// The differential test for the conversion of HeadTracking.ini to the canonical
// config format.
//
//   Oracle     the dev build 0.0.0-nightly.20260916.a18847d's reader and startup
//              code, the only published build (oracle/oracle_reader.cpp)
//   Import     the frozen reader in src/legacy_config/, and the startup code of
//              the commit that froze it
//
// Comparison 1, oracle against import, is what a player sees change that the
// conversion did not cause: commits since a18847d that change how the file is
// read. There are none. src/ was byte for byte a18847d's when the reader was
// frozen, the oracle compiles a18847d's reader from byte copies, and every core
// source either reader compiles holds the same bytes at a18847d's pin and at this
// repo's (CMakeLists.txt pins them), so the comparison may find no difference at
// all, floats bit for bit.
//
// Inputs: the dev build is the one published build. Its installer ZIP shipped
// plugins/HeadTracking.ini for install.cmd to seed, its launcher manifest seeded
// nothing, and its first start with no file wrote the same text from
// kDefaultConfigIni; the repo's HeadTracking.ini has one committed version up to
// a18847d, the same bytes again. data/headtracking-a18847d.ini holds it. Then no
// file, an empty file, core's mutation corpus over that file, and that file with
// each of ToggleKey, CycleModeKey and ToggleYawModeKey set to every code from
// 0x01 to 0xFE.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "legacy_config/legacy_config.h"
#include "oracle/oracle_reader.h"

#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"

namespace {

namespace fs = std::filesystem;
namespace cfg = cameraunlock::config;
namespace legacy = TWHT::legacy;
namespace testing = cameraunlock::config::testing;

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const std::string& what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL: %s\n", what.c_str());
}

std::string ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFileBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

// ---- Scratch folders -----------------------------------------------------------
//
// One folder per reading: GetPrivateProfileString, which both readers sit on,
// is free to cache the file it last read. `game` stands for the folder
// witness64_d3d11.exe is in. Every folder lives under one root for the run,
// removed at the end.

void RemoveTree(const fs::path& root) {
    if (!fs::exists(root)) return;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    fs::remove_all(root);
}

const fs::path& ScratchRoot() {
    static const fs::path root = [] {
        wchar_t temp[MAX_PATH + 1] = {};
        if (GetTempPathW(MAX_PATH + 1, temp) == 0) throw std::runtime_error("GetTempPathW failed");
        fs::path r = fs::path(temp) / ("twht_diff_" + std::to_string(GetCurrentProcessId()));
        RemoveTree(r);
        return r;
    }();
    return root;
}

class Scratch {
public:
    Scratch() {
        static unsigned s_next = 0;
        root_ = ScratchRoot() / std::to_string(s_next++);
        fs::create_directories(root_ / "game");
    }
    ~Scratch() { RemoveTree(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    fs::path game() const { return root_ / "game"; }
    fs::path legacy() const { return game() / "HeadTracking.ini"; }

    void WriteLegacy(const std::string& bytes) const { WriteFileBytes(legacy(), bytes); }

    std::set<std::string> Names() const {
        std::set<std::string> names;
        for (const auto& entry : fs::directory_iterator(game())) names.insert(entry.path().filename().string());
        return names;
    }

private:
    fs::path root_;
};

// ---- What a reading does -----------------------------------------------------------
//
// A Record names everything the running mod acts on after reading the file:
// `field.*` what the session and the lean clamp were handed, `start.*` the state
// the session starts in, `hotkey.*` the bindings that can fire, each as
// `modifiers:code` (Ctrl 1, Shift 2, as cameraunlock::input::KeyModifiers
// numbers them) in ascending order. Floats are their bits.

using Record = std::map<std::string, std::string>;

std::string Bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(bits));
    return text;
}

std::string Flag(bool value) { return value ? "1" : "0"; }

const char* const kActionNames[] = {"Toggle", "CycleTrackingMode", "YawMode"};
const char* const kModeNames[] = {"RotationAndPosition", "RotationOnly", "PositionOnly"};

void AddHotkeys(Record& r, const std::vector<twht_oracle::Registration>& registrations) {
    std::map<int, std::vector<std::pair<unsigned, int>>> byAction;
    for (int action = 0; action < 3; ++action) byAction[action];
    for (const auto& [action, vk, modifiers] : registrations) byAction[action].push_back({modifiers, vk});
    for (auto& [action, items] : byAction) {
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        std::string text;
        for (const auto& [modifiers, vk] : items) {
            char item[32];
            std::snprintf(item, sizeof(item), "%s%u:0x%02X", text.empty() ? "" : " ", modifiers,
                          static_cast<unsigned>(vk));
            text += item;
        }
        r[std::string("hotkey.") + kActionNames[action]] = text;
    }
}

Record ObserveOracle(const twht_oracle::Published& p) {
    Record r;
    r["field.udp_port"] = std::to_string(p.udp_port);
    r["field.limit_x"] = Bits(p.limit_x);
    r["field.limit_y"] = Bits(p.limit_y);
    r["field.limit_y_down"] = Bits(p.limit_y_down);
    r["field.limit_z"] = Bits(p.limit_z);
    r["field.limit_z_back"] = Bits(p.limit_z_back);
    r["field.local_smoothing"] = Bits(p.local_smoothing);
    r["field.remote_smoothing"] = Bits(p.remote_smoothing);
    r["field.collision_enabled"] = Flag(p.collision_enabled);
    r["field.collision_margin"] = Bits(p.collision_margin);
    r["field.collision_release_smoothing"] = Bits(p.collision_release_smoothing);
    r["field.log_to_file"] = Flag(p.log_to_file);
    r["field.log_diagnostics"] = Flag(p.log_diagnostics);
    r["start.enabled"] = Flag(p.tracking_enabled);
    r["start.mode"] = kModeNames[p.mode];
    r["start.world_yaw"] = Flag(p.world_yaw);
    AddHotkeys(r, p.hotkeys);
    return r;
}

// The frozen reader's settings through the startup code of the commit that
// froze it: src/core/config.cpp's LoadFromIni copies every field into the
// runtime Config, which Mod::ApplyConfigToSession, Mod::Initialize,
// HotkeyHandler::Start and CameraHook::ConfigureLeanClamp consume exactly as
// a18847d did.
Record ObserveImport(const legacy::Config& c) {
    twht_oracle::Published p;
    p.udp_port = c.udpPort;
    p.tracking_enabled = c.autoEnable;
    p.world_yaw = c.worldSpaceYaw;
    p.log_to_file = c.logToFile;
    p.log_diagnostics = c.logDiagnostics;
    p.limit_x = c.posLimitX;
    p.limit_y = c.posLimitY;
    p.limit_y_down = c.posLimitY;
    p.limit_z = c.posLimitZ;
    p.limit_z_back = c.posLimitZBack;
    p.local_smoothing = c.localSmoothing;
    p.remote_smoothing = c.remoteSmoothing;
    p.mode = c.positionEnabled ? twht_oracle::kRotationAndPosition : twht_oracle::kRotationOnly;
    p.collision_enabled = c.collisionEnabled;
    p.collision_margin = c.collisionMargin;
    p.collision_release_smoothing = c.collisionReleaseSmoothing;
    if (c.toggleKey != 0) p.hotkeys.emplace_back(twht_oracle::kToggle, c.toggleKey, 0u);
    if (c.cycleModeKey != 0) p.hotkeys.emplace_back(twht_oracle::kCycleMode, c.cycleModeKey, 0u);
    if (c.toggleYawModeKey != 0) p.hotkeys.emplace_back(twht_oracle::kYawMode, c.toggleYawModeKey, 0u);
    p.hotkeys.emplace_back(twht_oracle::kToggle, 'Y', 3u);
    p.hotkeys.emplace_back(twht_oracle::kCycleMode, 'G', 3u);
    p.hotkeys.emplace_back(twht_oracle::kYawMode, 'H', 3u);
    return ObserveOracle(p);
}

std::vector<std::string> Differences(const Record& a, const Record& b) {
    std::vector<std::string> out;
    for (const auto& [name, value] : a) {
        const auto it = b.find(name);
        if (it == b.end()) {
            out.push_back(name + " only on the left");
        } else if (it->second != value) {
            out.push_back(name + ": " + value + " / " + it->second);
        }
    }
    for (const auto& [name, value] : b) {
        if (a.find(name) == a.end()) out.push_back(name + " only on the right");
    }
    return out;
}

// ---- Inputs ------------------------------------------------------------------------

fs::path DataPath(const char* name) {
    return fs::path(TWHT_SOURCE_DIR) / "tests" / "config_differential" / "data" / name;
}

std::string PublishedFile() { return ReadFileBytes(DataPath("headtracking-a18847d.ini")); }

const char* const kPublishedName = "a18847d's HeadTracking.ini, seed and first-run file";

// Every key the frozen reader reads, and how the corpus varies each one. The
// reader refuses a port outside 1024-65535, a limit outside 0.01-10, a collision
// margin outside 0.06-10 and a hotkey code that is not a bindable key, and
// clamps a smoothing value into 0-1.
std::vector<testing::MutationKey> CorpusKeys() {
    return {
        {"Network", "UDPPort", "5252", {"80", "70000"}},
        {"Smoothing", "LocalSmoothing", "0.3", {"-0.5", "1.5"}},
        {"Smoothing", "RemoteSmoothing", "0.6", {"-0.5", "1.5"}},
        {"Position", "Enabled", "false", {}},
        {"Position", "LimitX", "0.45", {"0.001", "10.5"}},
        {"Position", "LimitY", "0.35", {"0.001", "10.5"}},
        {"Position", "LimitZ", "0.5", {"0.001", "10.5"}},
        {"Position", "LimitZBack", "0.15", {"0.001", "10.5"}},
        {"Collision", "CollisionEnabled", "true", {}},
        {"Collision", "CollisionMargin", "0.1", {"0.01", "10.5"}},
        {"Collision", "CollisionReleaseSmoothing", "0.5", {"-0.5", "1.5"}},
        {"Hotkeys", "ToggleKey", "0x2D", {"0x11"}, true},
        {"Hotkeys", "CycleModeKey", "0x70", {"0x10"}, true},
        {"Hotkeys", "ToggleYawModeKey", "0x71", {"0xA0"}, true},
        {"General", "AutoEnable", "false", {}},
        {"General", "LogToFile", "false", {}},
        {"General", "WorldSpaceYaw", "false", {}},
        {"General", "LogDiagnostics", "true", {}},
    };
}

std::vector<cfg::LegacyKey> CorpusReads() {
    std::vector<cfg::LegacyKey> reads;
    for (const legacy::Key& key : legacy::ReadKeys()) reads.push_back({key.section, key.key});
    return reads;
}

struct Input {
    std::string name;
    bool present;
    std::string bytes;
};

std::string WithKeyCode(const std::string& base, const std::string& line, int code) {
    const std::size_t at = base.find(line);
    if (at == std::string::npos) throw std::logic_error("no " + line + " in the published file");
    char to[48];
    std::snprintf(to, sizeof(to), "%.*s0x%02X", static_cast<int>(line.find('=') + 1), line.c_str(),
                  static_cast<unsigned>(code));
    std::string out = base;
    return out.replace(at, line.size(), to);
}

std::vector<Input> Inputs() {
    std::vector<Input> inputs;
    inputs.push_back({kPublishedName, true, PublishedFile()});
    inputs.push_back({"no file", false, {}});
    inputs.push_back({"empty file", true, {}});
    for (testing::IniMutation& m : testing::GenerateIniMutations(PublishedFile(), CorpusReads(), CorpusKeys())) {
        inputs.push_back({"corpus: " + m.name, true, std::move(m.bytes)});
    }
    for (const char* line : {"ToggleKey=0x23", "CycleModeKey=0x21", "ToggleYawModeKey=0x22"}) {
        for (int code = 0x01; code <= 0xFE; ++code) {
            char name[48];
            std::snprintf(name, sizeof(name), "%.*s0x%02X", static_cast<int>(std::strchr(line, '=') - line + 1),
                          line, static_cast<unsigned>(code));
            inputs.push_back({name, true, WithKeyCode(PublishedFile(), line, code)});
        }
    }
    return inputs;
}

// ---- Comparison 1 ---------------------------------------------------------------------

void Compare(const std::vector<Input>& inputs) {
    const Record defaults = ObserveImport(legacy::Config{});
    int compared = 0;
    int changed = 0;
    for (const Input& input : inputs) {
        const std::string& name = input.name;
        Scratch s;
        if (input.present) s.WriteLegacy(input.bytes);
        const std::set<std::string> before = s.Names();
        const Record oracle = ObserveOracle(twht_oracle::Read(s.game().string()));
        legacy::Config read;
        const bool present = read.LoadFromIni(s.legacy().string());
        Check(present == input.present, name + ": the frozen reader finds the file exactly when it is there");
        if (!Differences(oracle, defaults).empty()) ++changed;
        const std::vector<std::string> diff = Differences(oracle, ObserveImport(read));
        for (const std::string& d : diff) std::printf("  comparison 1, %s: %s\n", name.c_str(), d.c_str());
        Check(diff.empty(), name + ": comparison 1, the oracle and the import agree");
        Check(s.Names() == before, name + ": neither reader writes a file");
        Check(!input.present || ReadFileBytes(s.legacy()) == input.bytes, name + ": neither reader changes the file");
        ++compared;
    }
    std::printf("comparison 1: %d inputs, %d of them read as something other than the defaults\n", compared,
                changed);
    Check(changed > 0, "the inputs reach settings other than the defaults");
}

// The dev build's first start wrote the file every player of it who installed
// without the seed then held, and it is the input the corpus is built on.
void FirstRunOutputIsThePublishedFile() {
    Scratch s;
    twht_oracle::WriteFirstRunFile(s.game().string());
    Check(ReadFileBytes(s.legacy()) == PublishedFile(),
          "data/headtracking-a18847d.ini is what a18847d writes on a first start");
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    FirstRunOutputIsThePublishedFile();
    Compare(Inputs());
    RemoveTree(ScratchRoot());
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
