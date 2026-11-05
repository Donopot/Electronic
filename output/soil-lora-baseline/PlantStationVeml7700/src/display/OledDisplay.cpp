#include "OledDisplay.h"
#if PLANT_OLED
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

namespace plant {
bool OledDisplay::begin() {
    lastAttempt_ = millis();
    ready_ = false;
    pinMode(36, OUTPUT);
    digitalWrite(36, LOW); // Vext Heltec : alimentation OLED active basse.
    delay(100);
    pinMode(21, OUTPUT);
    digitalWrite(21, LOW);
    delay(10);
    digitalWrite(21, HIGH);
    delay(20);
    wire_.end();
    if (!wire_.begin(17, 18, 100000)) return false;
    wire_.setTimeOut(kTimeoutMs);
    wire_.beginTransmission(0x3C);
    if (wire_.endTransmission(true) != 0) return false;
    // Ne pas laisser begin() reconfigurer les broches du bus.
    ready_ = display_.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
    if (ready_) {
        display_.setTextWrap(false);
        show(Measurement{}, millis());
    }
    return ready_;
}

void OledDisplay::show(const Measurement& value, uint64_t uptime) {
    if (!ready_ && elapsed(millis(), lastAttempt_, 10000)) begin();
    if (!ready_) return;
    wire_.beginTransmission(0x3C);
    if (wire_.endTransmission(true) != 0) {
        ready_ = false; // Un ecran perdu ne retarde pas les acquisitions suivantes.
        lastAttempt_ = millis();
        return;
    }
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(0, 0);
    display_.print("PLANT STATION");
    display_.setCursor(92, 0);
    display_.print("PRG3s");
    display_.setCursor(0, 14);
    display_.print("VEML7700 / lux");
    display_.setCursor(0, 30);
    char text[32];
    if (value.status == Status::Ok && isfinite(value.lux) && value.lux >= 0 && value.lux <= kMaxLux) {
        display_.setTextSize(2);
        snprintf(text, sizeof(text), "%.1f", value.lux);
        display_.print(text);
    } else {
        const char* message = "ERREUR CAPTEUR";
        if (value.status == Status::WarmingUp) message = "MESURE EN COURS";
        else if (value.status == Status::Nack) message = "CAPTEUR ABSENT";
        else if (value.status == Status::BusError || value.status == Status::ShortRead) message = "ERREUR I2C";
        else if (value.status == Status::Saturated) message = "HORS PLAGE";
        display_.print(message);
    }
    display_.setTextSize(1);
    display_.setCursor(0, 54);
    snprintf(text, sizeof(text), "Depuis %llu s", static_cast<unsigned long long>(uptime / 1000));
    display_.print(text);
    display_.display();
}
void OledDisplay::showSleepPrompt() {
    if (!ready_) return;
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(0, 0);
    display_.print("MISE EN VEILLE");
    display_.setCursor(0, 22);
    display_.print("Relacher PRG");
    display_.setCursor(0, 44);
    display_.print("RST pour reprendre");
    display_.display();
}
void OledDisplay::sleep() {
    if (ready_) display_.ssd1306_command(SSD1306_DISPLAYOFF);
    wire_.end();
    pinMode(17, INPUT);
    pinMode(18, INPUT);
    ready_ = false;
}
}
#endif
