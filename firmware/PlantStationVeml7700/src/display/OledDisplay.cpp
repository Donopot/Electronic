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
#if PLANT_SOIL_LORA
void OledDisplay::showSoil(const Measurement& lux, const plantsoil::ReceiverBank& soil, bool radioReady, uint32_t now) {
    // Meme politique bornee de presence/reprise OLED que l'affichage lux seul.
    if (!ready_ && elapsed(millis(), lastAttempt_, 10000)) begin();
    if (!ready_) return;
    wire_.beginTransmission(0x3C);
    if (wire_.endTransmission(true) != 0) { ready_ = false; lastAttempt_ = millis(); return; }
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    char line[32];
    display_.setCursor(0, 0); display_.print("PLANT STATION  PRG3s");
    display_.setCursor(0, 9);
    if (lux.status == Status::Ok && isfinite(lux.lux) && lux.lux >= 0 && lux.lux <= kMaxLux)
        snprintf(line, sizeof(line), "Lux %.1f", lux.lux);
    else snprintf(line, sizeof(line), "Lux %s", lux.status == Status::WarmingUp ? "EN COURS" : "ERREUR CAPTEUR");
    display_.print(line);
    for (uint8_t id = 1; id <= plantsoil::kNodeCount; ++id) {
        const auto& node = soil.node(id);
        display_.setCursor(0, 18 + (id - 1) * 18);
        snprintf(line, sizeof(line), "SOIL-%02u NON AUTH", static_cast<unsigned>(id));
        display_.print(line);
        display_.setCursor(0, 27 + (id - 1) * 18);
        const auto state = node.state(now);
        if (state == plantsoil::Freshness::Current) {
            snprintf(line, sizeof(line), "%.6f V %lum", plantsoil::microvolts(node.packet().raw) / 1000000.0,
                     static_cast<unsigned long>(node.age(now) / 60000));
            display_.print(line);
        } else display_.print(state == plantsoil::Freshness::Waiting ? "ATTENTE SONDE" :
                             state == plantsoil::Freshness::Stale ? "MESURE PERIMEE" : "DEFAUT ADS");
    }
    display_.setCursor(0, 54); display_.print(radioReady ? "RADIO ECOUTE" : "ERREUR RADIO");
    display_.display();
}
#endif
void OledDisplay::sleep() {
    if (ready_) display_.ssd1306_command(SSD1306_DISPLAYOFF);
    wire_.end();
    pinMode(17, INPUT);
    pinMode(18, INPUT);
    ready_ = false;
}
}
#endif
