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
void OledDisplay::showSleepPrompt(bool setupChoice) {
    if (!ready_) return;
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(0, 0);
    display_.print("MISE EN VEILLE");
    display_.setCursor(0, 22);
    display_.print("Relacher PRG");
    display_.setCursor(0, 44);
    display_.print(setupChoice?"Ou tenir 8s : Wi-Fi":"RST pour reprendre");
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
int qrExtent=0;
void drawQr(esp_qrcode_handle_t code) {
    // Choisir la plus grande echelle possible sur 64 pixels de haut, en
    // preferant 2 px par module : un QR de 1 px (~7 mm) n'est pas lisible
    // par un telephone.
    const int size=esp_qrcode_get_size(code);
    int scale=1,margin=4;
    bool found=false;
    for(int s=3;s>=1&&!found;--s)
        for(int m=4;m>=1;--m)
            if((size+2*m)*s<=64){scale=s;margin=m;found=true;break;}
    if(!found)return;
    const int extent=(size+2*margin)*scale,top=(64-extent)/2,left=(128-extent)/2;
    if(extent>64||!qrDisplay)return;
    qrDisplay->fillRect(left,top,extent,extent,SSD1306_WHITE);
    for(int y=0;y<size;++y)for(int x=0;x<size;++x)
        if(esp_qrcode_get_module(code,x,y))qrDisplay->fillRect(left+(x+margin)*scale,top+(y+margin)*scale,scale,scale,SSD1306_BLACK);
    qrExtent=extent;qrRendered=true;
}
void generateQr(Adafruit_SSD1306& display,const char* payload,int maxVersion) {
    qrDisplay=&display;qrRendered=false;qrExtent=0;
    esp_qrcode_config_t config=ESP_QRCODE_CONFIG_DEFAULT();config.display_func=drawQr;config.max_qrcode_version=maxVersion;
    esp_qrcode_generate(&config,payload);qrDisplay=nullptr;
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
void OledDisplay::showSetupPrompt() {
    if(!ready_)return;
    display_.clearDisplay();display_.setTextSize(1);display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(0,0);display_.print("CONFIGURATION WI-FI");
    display_.setCursor(0,22);display_.print("Relacher PRG");
    display_.setCursor(0,44);display_.print("Puis scanner le QR");display_.display();
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
        // Un seul QR, toujours celui de l'action courante : appairage Android,
        // puis Wi-Fi du kit pendant la configuration, puis acces local.
        char payload[192]={};int version=4;
        const bool android=network.bleSetup&&network.setupExplicit&&!network.setupSuccess;
        const bool configWifi=!android&&network.provisioning;
        if(android) {
            snprintf(payload,sizeof(payload),"{\"ver\":\"v1\",\"name\":\"%s\",\"transport\":\"ble\",\"security\":1,\"pop\":\"%s\"}",network.setupSsid,network.setupPassword);
            version=9;
        } else if(configWifi) {
            // Sans guillemets autour de la cle : certains analyseurs Wi-Fi les refusent.
            snprintf(payload,sizeof(payload),"WIFI:T:WPA;S:%s;P:%s;;",network.setupSsid,network.setupPassword);
            version=4;
        } else if(network.online) {
            snprintf(payload,sizeof(payload),"%s",network.url);
            for(char* p=payload;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
            version=3;
        }
        if(*payload) generateQr(display_,payload,version);
        if(!*payload) {
            display_.setCursor(0,0);display_.print("WI-FI ABSENT");
            display_.setCursor(0,20);display_.print("Reconnexion en cours");
            display_.setCursor(0,48);display_.print("PRG 8s : configurer");
        }
        if(!qrRendered&&*payload){display_.setCursor(0,20);display_.print("QR indisponible");}
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
        else if(kLux&&view.luxValid) {snprintf(line,sizeof(line),"Lx %.0f PRG:detail",view.lux);display_.print(line);}
        else if(kLux) display_.print("Lux -- PRG:details");
        else {
            // Profil sans capteur de lumiere : compter les sondes exploitables.
            unsigned fresh=0;
            for(unsigned i=0;i<4;++i) {
                const auto current=view.plants[i].state;
                if(current!=PlantState::Waiting&&current!=PlantState::Stale&&current!=PlantState::AdcFault)++fresh;
            }
            snprintf(line,sizeof(line),"%u/4 fraiches PRG:>",fresh);display_.print(line);
        }
    }
    display_.display();
}
#endif
}
#endif
