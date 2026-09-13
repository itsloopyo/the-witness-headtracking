#include "pch.h"
#include "input/hotkey_handler.h"

#include "core/config.h"
#include "core/mod.h"

#include "core/debug_log.h"
#include <cameraunlock/input/chord_hotkeys.h>

#include <exception>

namespace TWHT {

namespace {
// Ctrl+Shift chord letters per the shared T/Y/U/G/H/J cluster convention:
// Y = toggle tracking, G = cycle tracking mode, H = yaw mode.
constexpr int kVkY = 'Y';
constexpr int kVkG = 'G';
constexpr int kVkH = 'H';
} // namespace

void HotkeyHandler::Start(const Config& config) {
    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;

    const auto toggle     = []() { Mod::Instance().Toggle(); };
    const auto cycleMode  = []() { Mod::Instance().CycleTrackingMode(); };
    const auto toggleYaw  = []() { Mod::Instance().ToggleYawMode(); };

    // A key of 0 is a binding the config dropped, because two actions had landed
    // on it. The chord below still covers the action.
    if (config.toggleKey != 0) m_poller.SetToggleKey(config.toggleKey, NavGuarded(toggle));
    if (config.cycleModeKey != 0) m_poller.AddHotkey(config.cycleModeKey, NavGuarded(cycleMode));
    if (config.toggleYawModeKey != 0) {
        m_poller.AddHotkey(config.toggleYawModeKey, NavGuarded(toggleYaw));
    }

    m_poller.AddHotkey(kVkY, ChordGuarded(toggle));
    m_poller.AddHotkey(kVkG, ChordGuarded(cycleMode));
    m_poller.AddHotkey(kVkH, ChordGuarded(toggleYaw));

    // Start rethrows whatever the std::thread construction threw, by design, so
    // the mod can say so, and that is not only std::system_error: MSVC
    // allocates the thread's call state, so std::bad_alloc escapes the same way.
    // This runs on the init thread, and an exception leaving a thread procedure
    // is std::terminate - the game would die seconds after launch with the log
    // stopping mid-initialise. Losing the hotkeys is a degraded mod; losing the
    // process is a broken game.
    try {
        m_poller.Start(16);
    } catch (const std::exception& e) {
        HT_LOG("ERROR: could not start the hotkey thread (%s). Hotkeys are off for "
               "this session; everything in HeadTracking.ini still applies.", e.what());
    }
}

} // namespace TWHT
