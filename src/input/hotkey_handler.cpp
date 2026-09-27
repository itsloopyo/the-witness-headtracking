#include "pch.h"
#include "input/hotkey_handler.h"

#include "core/config.h"
#include "core/mod.h"

#include "core/debug_log.h"
#include <cameraunlock/input/key_binding_registration.h>
#include <cameraunlock/input/key_bindings.h>

#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace TWHT {

namespace {

// The table's hotkey codec lets only a list ParseKeyBindings reads into the
// settings.
void RegisterList(cameraunlock::input::HotkeyPoller& poller, const std::string& list,
                  std::function<void()> action) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) throw std::logic_error("hotkey list '" + list + "': " + parsed.error);
    cameraunlock::input::RegisterKeyBindings(poller, parsed.bindings, std::move(action));
}

} // namespace

void HotkeyHandler::Start(const Config& config) {
    RegisterList(m_poller, config.toggleKey, []() { Mod::Instance().Toggle(); });
    RegisterList(m_poller, config.cycleTrackingModeKey, []() { Mod::Instance().CycleTrackingMode(); });
    RegisterList(m_poller, config.yawModeKey, []() { Mod::Instance().ToggleYawMode(); });

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
               "this session; everything in CameraUnlock.ini still applies.", e.what());
    }
}

} // namespace TWHT
