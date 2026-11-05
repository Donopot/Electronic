#include "power/PrgButton.h"
#include "power/SleepPower.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <assert.h>
#include <algorithm>
namespace host { uint32_t now = 0; }
FakeSerial Serial;
FakeEsp ESP;
TwoWire Wire;
using plant::ButtonEvent;

static void buttonCycle(uint32_t base) {
    plant::PrgButton button;
    button.begin(base, false);
    assert(button.update(base + 40, false) == ButtonEvent::None);
    assert(button.update(base + 100, true) == ButtonEvent::None);
    assert(button.update(base + 139, true) == ButtonEvent::None);
    assert(button.update(base + 140, true) == ButtonEvent::None);
    assert(button.update(base + 300, false) == ButtonEvent::None);
    assert(button.update(base + 340, false) == ButtonEvent::None); // appui court
    button.update(base + 400, true);
    button.update(base + 420, false); // rebond
    button.update(base + 430, true);
    button.update(base + 470, true);
    assert(button.update(base + 3429, true) == ButtonEvent::None);
    assert(button.update(base + 3430, true) == ButtonEvent::LongPress);
    assert(button.update(base + 4000, true) == ButtonEvent::None);
    assert(button.update(base + 4100, false) == ButtonEvent::None);
    assert(button.update(base + 4139, false) == ButtonEvent::None);
    assert(button.update(base + 4140, false) == ButtonEvent::ReleasedForSleep);
    assert(button.update(base + 4200, false) == ButtonEvent::None);
}
int main() {
    buttonCycle(0);
    buttonCycle(0xFFFFF800U);
    plant::PrgButton firstPress;
    firstPress.begin(0, false);
    firstPress.update(100, true); // aucun tour relache avant cet appui
    firstPress.update(140, true);
    assert(firstPress.update(3099, true) == ButtonEvent::None);
    assert(firstPress.update(3100, false) == ButtonEvent::LongPress);
    assert(firstPress.update(3140, false) == ButtonEvent::ReleasedForSleep);
    plant::PrgButton justShort;
    justShort.begin(0, false);
    justShort.update(100, true);
    justShort.update(140, true);
    assert(justShort.update(3099, false) == ButtonEvent::None);
    assert(justShort.update(3139, false) == ButtonEvent::None);
    plant::PrgButton heldAtBoot;
    heldAtBoot.begin(0, true);
    assert(heldAtBoot.update(5000, true) == ButtonEvent::None);
    heldAtBoot.update(5100, false);
    assert(heldAtBoot.update(5140, false) == ButtonEvent::None);
    heldAtBoot.update(5200, true);
    heldAtBoot.update(5240, true);
    assert(heldAtBoot.update(8240, true) == ButtonEvent::LongPress);
    plant::SleepPower::restore();
    assert(host::gpioCalls()[0] == "deep_hold_off");
    assert(host::gpioCalls()[1] == "release8" && host::gpioCalls()[5] == "release36");
    host::gpioCalls().clear();
    plant::SleepPower::prepare();
    const auto& calls = host::gpioCalls();
    assert(std::find(calls.begin(), calls.end(), "write12:0") != calls.end());
    assert(std::find(calls.begin(), calls.end(), "write35:0") != calls.end());
    assert(std::find(calls.begin(), calls.end(), "write36:1") != calls.end());
    assert(calls[calls.size()-2] == "deep_hold_on" && calls.back() == "wake_off");
    try { plant::SleepPower::enter(); assert(false); }
    catch (const SleepEntered&) { assert(Serial.flushed); }
    puts("PASS veille : bouton court/long/rebond/release/boot/wrap, maintiens GPIO et flush avant sommeil");
}
