#include "SleepPower.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace plant {
namespace {
constexpr gpio_num_t held[] = {GPIO_NUM_8, GPIO_NUM_12, GPIO_NUM_21, GPIO_NUM_35, GPIO_NUM_36};
}
void SleepPower::restore() {
    gpio_deep_sleep_hold_dis();
    for (const auto pin : held) gpio_hold_dis(pin);
    pinMode(0, INPUT_PULLUP);
    pinMode(35, OUTPUT);
    digitalWrite(35, LOW);
}
void SleepPower::prepare(bool radioAsleep) {
    pinMode(4, INPUT);
    pinMode(5, INPUT);
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // SX1262 non selectionne.
    pinMode(12, OUTPUT);
    digitalWrite(12, radioAsleep ? HIGH : LOW); // Reset de secours si sommeil radio non confirme.
    pinMode(21, OUTPUT);
    digitalWrite(21, LOW);
    pinMode(35, OUTPUT);
    digitalWrite(35, LOW);
    pinMode(36, OUTPUT);
    digitalWrite(36, HIGH); // Alimentation OLED coupee, meme pendant la veille.
    for (const auto pin : held) gpio_hold_en(pin);
    gpio_deep_sleep_hold_en();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}
void SleepPower::enter() {
    Serial.flush();
    esp_deep_sleep_start();
}
}
