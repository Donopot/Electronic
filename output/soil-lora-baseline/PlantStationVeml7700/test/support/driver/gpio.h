#pragma once
#include <Arduino.h>
enum gpio_num_t { GPIO_NUM_8 = 8, GPIO_NUM_12 = 12, GPIO_NUM_21 = 21, GPIO_NUM_35 = 35, GPIO_NUM_36 = 36 };
inline void gpio_deep_sleep_hold_dis() { host::gpioCalls().push_back("deep_hold_off"); }
inline void gpio_deep_sleep_hold_en() { host::gpioCalls().push_back("deep_hold_on"); }
inline void gpio_hold_dis(gpio_num_t pin) { host::gpioCalls().push_back("release" + std::to_string(pin)); }
inline void gpio_hold_en(gpio_num_t pin) { host::gpioCalls().push_back("hold" + std::to_string(pin)); }
