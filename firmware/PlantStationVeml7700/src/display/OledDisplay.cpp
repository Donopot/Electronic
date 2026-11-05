#include "OledDisplay.h"
#if PLANT_OLED
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#if PLANT_DASHBOARD
#include <qrcode.h>
#include <string.h>
#endif

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
    display_.setCursor(0, 8);
    if (lux.status == Status::Ok && isfinite(lux.lux) && lux.lux >= 0 && lux.lux <= kMaxLux)
        snprintf(line, sizeof(line), "Lux %.1f", lux.lux);
    else snprintf(line, sizeof(line), "Lux %s", lux.status == Status::WarmingUp ? "EN COURS" : "ERREUR CAPTEUR");
    display_.print(line);
    static_assert(plantsoil::kNodeCount == 4, "Disposition OLED prevue pour quatre sondes");
    display_.setCursor(0, 16); display_.print("SONDES NON AUTH");
    for (uint8_t id = 1; id <= plantsoil::kNodeCount; ++id) {
        const auto& node = soil.node(id);
        display_.setCursor(0, 16 + id * 8);
        const auto state = node.state(now);
        if (state == plantsoil::Freshness::Current) {
            snprintf(line, sizeof(line), "SOIL-%02u %.3f V %lum", static_cast<unsigned>(id),
                     plantsoil::microvolts(node.packet().raw) / 1000000.0,
                     static_cast<unsigned long>(node.age(now) / 60000));
        } else snprintf(line, sizeof(line), "SOIL-%02u %s", static_cast<unsigned>(id),
                        state == plantsoil::Freshness::Waiting ? "ATTENTE" :
                        state == plantsoil::Freshness::Stale ? "PERIMEE" : "DEFAUT ADS");
        display_.print(line);
    }
    display_.setCursor(0, 56); display_.print(radioReady ? "RADIO ECOUTE" : "ERREUR RADIO");
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
#if PLANT_DASHBOARD
namespace {
Adafruit_SSD1306* qrDisplay = nullptr;
bool qrRendered = false;
void drawQr(esp_qrcode_handle_t code) {
    const int size=esp_qrcode_get_size(code);
    // Uppercase HTTP://IP/ is QR alphanumeric: version 1 fits at two pixels
    // per module with the full four-module quiet zone on this 64px display.
    if(size!=21 || !qrDisplay) return;
    qrDisplay->fillRect(0,3,58,58,SSD1306_WHITE);
    for(int y=0;y<size;++y) for(int x=0;x<size;++x)
        if(esp_qrcode_get_module(code,x,y)) qrDisplay->fillRect(8+x*2,11+y*2,2,2,SSD1306_BLACK);
    qrRendered=true;
}
const char* oledState(PlantState state) {
    switch(state) {
    case PlantState::Waiting:return "EN ATTENTE";
    case PlantState::Uncalibrated:return "A ETALONNER";
    case PlantState::Healthy:return "SATISFAISANT";
    case PlantState::Dry:return "SECHE";
    case PlantState::AdcFault:return "DEFAUT ADC";
    default:return "PERIMEE";
    }
}
}
void OledDisplay::showDashboard(const PlantView& view,Screen screen,const DashboardNetwork& network) {
    if(!ready_ && elapsed(millis(),lastAttempt_,10000)) begin();
    if(!ready_) return;
    wire_.beginTransmission(0x3C);
    if(wire_.endTransmission(true)!=0) {ready_=false;lastAttempt_=millis();return;}
    display_.clearDisplay();display_.setTextSize(1);display_.setTextColor(SSD1306_WHITE);
    char line[40];
    if(screen==Screen::Details) {
        for(unsigned i=0;i<4;++i) {
            const auto& p=view.plants[i];display_.setCursor(0,i*16);
            if(p.voltageValid) snprintf(line,sizeof(line),"SOIL-%02u %.3fV B--",i+1,p.microvolts/1000000.0);
            else snprintf(line,sizeof(line),"SOIL-%02u --V B--",i+1);
            display_.print(line);display_.setCursor(0,i*16+8);
            if(p.seen) snprintf(line,sizeof(line),"%s %lum",oledState(p.state),static_cast<unsigned long>(p.ageMs/60000));
            else snprintf(line,sizeof(line),"%s",oledState(p.state));
            display_.print(line);
        }
    } else if(screen==Screen::Qr) {
        if(network.online) {
            char url[40];snprintf(url,sizeof(url),"%s",network.url);
            for(char* p=url;*p;++p) if(*p>='a' && *p<='z') *p-=32;
            qrDisplay=&display_;qrRendered=false;
            esp_qrcode_config_t config=ESP_QRCODE_CONFIG_DEFAULT();config.display_func=drawQr;config.max_qrcode_version=2;
            esp_qrcode_generate(&config,url);qrDisplay=nullptr;
            display_.setCursor(62,3);display_.print("ACCES LOCAL");
            display_.setCursor(63,17);display_.print("Meme Wi-Fi");
            const char* ip=network.url+7;char ipText[24];snprintf(ipText,sizeof(ipText),"%s",ip);
            char* slash=strchr(ipText,'/');if(slash)*slash=0;
            // Split the IP into two groups so it remains readable beside QR.
            char* split=strchr(ipText,'.');if(split)split=strchr(split+1,'.');
            if(split) {*split=0;display_.setCursor(63,32);display_.print(ipText);display_.setCursor(63,42);display_.print(split+1);}
            if(!qrRendered) {display_.setCursor(0,20);display_.print("QR indisponible");}
            display_.setCursor(63,56);display_.print("PRG:visage");
        } else {
            display_.setCursor(0,0);display_.print(network.provisioning?"CONFIGURATION BLE":"WI-FI ABSENT");
            if(network.provisioning) {
                display_.setCursor(0,12);display_.print("ESP BLE Provisioning");
                display_.setCursor(0,25);display_.print(network.bluetoothName);
                display_.setCursor(0,38);display_.print("Code ");display_.print(network.proof);
            } else {display_.setCursor(0,18);display_.print("Reconnexion en cours");}
            display_.setCursor(0,54);display_.print("Radio/lux maintenus");
        }
    } else {
        display_.setCursor(0,0);display_.print("PLANT STATION");
        display_.drawRoundRect(3,20,43,30,7,SSD1306_WHITE);
        display_.drawLine(24,20,24,14,SSD1306_WHITE);display_.fillCircle(24,12,2,SSD1306_WHITE);
        display_.fillRect(12,28,5,5,SSD1306_WHITE);display_.fillRect(32,28,5,5,SSD1306_WHITE);
        if(view.mood==Mood::Happy) {display_.drawLine(13,39,23,45,SSD1306_WHITE);display_.drawLine(23,45,35,39,SSD1306_WHITE);}
        else if(view.mood==Mood::Sad) {display_.drawLine(13,44,23,38,SSD1306_WHITE);display_.drawLine(23,38,35,44,SSD1306_WHITE);}
        else if(view.mood==Mood::Worried) {display_.drawLine(13,42,20,38,SSD1306_WHITE);display_.drawLine(20,38,27,42,SSD1306_WHITE);display_.drawLine(27,42,35,38,SSD1306_WHITE);}
        else display_.drawLine(13,41,35,41,SSD1306_WHITE);
        for(unsigned i=0;i<4;++i) {
            display_.setCursor(53,15+i*9);
            const auto state=view.plants[i].state;
            const char* shortState=state==PlantState::Dry?"SECHE":state==PlantState::Healthy?"OK":state==PlantState::Uncalibrated?"ETAL.":state==PlantState::Waiting?"ATT.":state==PlantState::AdcFault?"ADC!":"PERIM.";
            snprintf(line,sizeof(line),"S%02u %s",i+1,shortState);display_.print(line);
        }
        display_.setCursor(0,56);
        if(!view.radioReady) display_.print("RADIO! PRG:details");
        else if(view.luxValid) {snprintf(line,sizeof(line),"Lux %.0f PRG:details",view.lux);display_.print(line);}
        else display_.print("Lux -- PRG:details");
    }
    display_.display();
}
#endif
}
#endif
