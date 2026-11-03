#pragma once
#include <Arduino.h>
constexpr int ESP_SLEEP_WAKEUP_ALL = -1;
struct SleepEntered {};
inline void esp_sleep_disable_wakeup_source(int source) {
    if (source != ESP_SLEEP_WAKEUP_ALL) throw "reveil non desactive";
    host::gpioCalls().push_back("wake_off");
}
inline void esp_deep_sleep_start() {
    host::gpioCalls().push_back("deep_sleep");
    throw SleepEntered{};
}
