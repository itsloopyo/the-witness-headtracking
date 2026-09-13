#pragma once

#include <cameraunlock/input/hotkey_poller.h>

namespace TWHT {

struct Config;

class HotkeyHandler {
public:
    void Start(const Config& config);

private:
    cameraunlock::input::HotkeyPoller m_poller;
};

} // namespace TWHT
