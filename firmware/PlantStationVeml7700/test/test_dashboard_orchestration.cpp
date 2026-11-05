#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <freertos/task.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>
#include <esp_sleep.h>
#include "web/PlantDashboard.h"
#include "display/OledDisplay.h"
#include <assert.h>
#include <string.h>
#include <algorithm>
namespace host {uint32_t now=0;}
FakeSerial Serial;FakeEsp ESP;TwoWire Wire;FakeSpi SPI;
namespace {
plant::PlantView latest;
plant::PlantConfig names[4];
plant::DashboardNetwork network;
bool stopRequested=false,workerStopped=false,setupRequested=false;
unsigned published=0;
}
// Worker boundary is simulated; the real setup/loop/radio/model/OLED/PRG execute.
namespace plant {
void PlantDashboard::begin(){defaultPlants(names);}
void PlantDashboard::publish(const PlantView& view){latest=view;++published;}
void PlantDashboard::configuration(PlantConfig* out)const{memcpy(out,names,sizeof(names));}
DashboardNetwork PlantDashboard::network()const{return ::network;}
void PlantDashboard::requestStop(){stopRequested=true;}
void PlantDashboard::requestSetup(){setupRequested=true;::network.provisioning=true;strcpy(::network.setupSsid,"PLANT-TEST12");strcpy(::network.setupPassword,"0123456789ABCDEF");}
bool PlantDashboard::stopped()const{return workerStopped;}
}
void setup();void loop();
void shortPress(uint32_t now) {
    host::buttonPressed()=true;host::now=now;loop();host::now+=40;loop();
    host::buttonPressed()=false;host::now+=60;loop();host::now+=40;loop();
}
void bounds() {
    for(const auto& draw:host::oledLayouts().back()) {
        assert(draw.x>=0&&draw.y>=0&&draw.x+int(draw.text.size())*6*draw.size<=128&&draw.y+8*draw.size<=64);
    }
}
int main(int argc,char** argv) {
    const bool absent=argc>1&&!strcmp(argv[1],"no_oled");FakeWire::oledPresent()=!absent;
    setup();host::finishInitialization();for(unsigned i=0;i<113;++i)loop();
    host::now=1000;loop();
#if PLANT_LUX
    assert(latest.luxValid && latest.radioReady && latest.warningMask==15);
#else
    // Profil sans capteur : le lux reste explicitement inconnu.
    assert(!latest.luxValid && latest.radioReady && latest.warningMask==15);
#endif
    for(uint8_t id=1;id<=4;++id) {
        plantsoil::Packet p;p.node=id;p.session=77;p.sequence=1;p.address=0x48;p.status=plantsoil::AdcStatus::Ok;p.raw=16000+id*1000;
        uint8_t bytes[plantsoil::kPacketSize];assert(plantsoil::encode(p,bytes,sizeof(bytes))==plantsoil::Result::Accepted);
        host::inject(bytes,sizeof(bytes));host::now=1100+id;loop();
    }
    assert(latest.mood==plant::Mood::Neutral && latest.warningMask==0 && latest.dryMask==0);
    for(unsigned i=0;i<4;++i){assert(latest.plants[i].id==i+1 && !latest.plants[i].batteryKnown);}
#if !PLANT_LUX
    if(!absent) {
        // Sans VEML7700, la ligne basse compte les sondes exploitables.
        plant::OledDisplay face;assert(face.begin());face.showDashboard(latest,plant::Screen::Face,network);bounds();
        assert(host::oledFrames().back().find("4/4 fraiches")!=std::string::npos);
        assert(host::oledFrames().back().find("Lux")==std::string::npos);
    }
#endif
    strcpy(names[2].name,"Mon basilic");host::now=2000;loop();assert(!strcmp(latest.plants[2].name,"Mon basilic"));
    shortPress(2100);
    if(!absent) {
        const auto& text=host::oledFrames().back();
        for(unsigned id=1;id<=4;++id){char label[16];snprintf(label,sizeof(label),"SOIL-%02u",id);assert(text.find(label)!=std::string::npos);}
        assert(text.find("A ETALONNER")!=std::string::npos && text.find("B--")!=std::string::npos);bounds();
    }
    shortPress(2400);
    if(!absent){assert(host::oledFrames().back().find("WI-FI ABSENT")!=std::string::npos);bounds();}
    network.online=true;strcpy(network.url,"http://192.168.100.100/");host::now=4000;loop();
    if(!absent) {
        assert(host::qrPayload()=="HTTP://192.168.100.100/");bounds();
        const auto found=std::find_if(host::oledRectangles().begin(),host::oledRectangles().end(),[](const host::OledRect& r){return r.x==0&&r.y==3&&r.width==58&&r.height==58&&r.color==SSD1306_WHITE;});
        assert(found!=host::oledRectangles().end());
        plant::OledDisplay maxLux;assert(maxLux.begin());auto view=latest;view.luxValid=true;view.lux=140000;
        maxLux.showDashboard(view,plant::Screen::Face,network);bounds();
    }
    host::now=1104+plantsoil::kStaleMs;loop();assert(latest.warningMask==15 && latest.dryMask==0 && latest.mood==plant::Mood::Worried);
    host::buttonPressed()=true;host::now+=100;loop();host::now+=40;loop();host::now+=3000;loop();
    if(!absent)assert(host::oledFrames().back().find("Ou tenir 8s")!=std::string::npos);
    const auto frames=host::oledFrames().size();const unsigned beforeHold=published;
    plantsoil::Packet held;held.node=1;held.session=77;held.sequence=2;held.address=0x48;held.status=plantsoil::AdcStatus::Ok;held.raw=19000;
    uint8_t heldBytes[plantsoil::kPacketSize];assert(plantsoil::encode(held,heldBytes,sizeof(heldBytes))==plantsoil::Result::Accepted);
    host::inject(heldBytes,sizeof(heldBytes));host::now+=100;loop();
    assert(published>beforeHold&&latest.plants[0].packet.sequence==2&&latest.plants[0].packet.raw==19000);
    held.sequence=3;assert(plantsoil::encode(held,heldBytes,sizeof(heldBytes))==plantsoil::Result::Accepted);host::inject(heldBytes,sizeof(heldBytes));host::now+=100;loop();
    assert(latest.plants[0].packet.sequence==3&&host::oledFrames().size()==frames);
    host::now+=5000;loop();assert(!stopRequested);
    host::buttonPressed()=false;host::now+=100;loop();host::now+=40;loop();assert(setupRequested&&!Serial.flushed);
    if(!absent){
        assert(host::qrPayload()=="WIFI:T:WPA;S:PLANT-TEST12;P:\"0123456789ABCDEF\";;");bounds();
        assert(host::oledFrames().back().find("01234567")!=std::string::npos&&host::oledFrames().back().find("89ABCDEF")!=std::string::npos);
        const auto white=std::find_if(host::oledRectangles().begin(),host::oledRectangles().end(),[](const host::OledRect& r){return r.x==0&&r.y==13&&r.width==37&&r.height==37&&r.color==SSD1306_WHITE;});assert(white!=host::oledRectangles().end());
        host::forceQrSide()=33;plant::OledDisplay qr;assert(qr.begin());qr.showDashboard(latest,plant::Screen::Qr,network);bounds();host::forceQrSide()=0;
    }
    if(!absent){
        plant::OledDisplay qr;assert(qr.begin());
        // BLE actif mais sans demande explicite : le QR reste celui du Wi-Fi du kit.
        network.bleSetup=true;network.setupExplicit=false;
        qr.showDashboard(latest,plant::Screen::Qr,network);bounds();
        assert(host::qrPayload()=="WIFI:T:WPA;S:PLANT-TEST12;P:\"0123456789ABCDEF\";;");
        network.setupExplicit=true;strcpy(network.setupSsid,"PLANT-EBF528");
        for(int side:{41,45,49,53}){
            host::forceQrSide()=side;qr.showDashboard(latest,plant::Screen::Qr,network);bounds();
            assert(host::qrPayload()=="{\"ver\":\"v1\",\"name\":\"PLANT-EBF528\",\"transport\":\"ble\",\"security\":1,\"pop\":\"0123456789ABCDEF\"}");
            const auto& rects=host::oledRectangles();const int extent=side+8;
            assert(std::any_of(rects.begin(),rects.end(),[&](const host::OledRect& r){return r.x==0&&r.y==(64-extent)/2&&r.width==extent&&r.height==extent&&r.color==SSD1306_WHITE;}));
        }
        host::forceQrSide()=0;network.bleSetup=false;network.setupExplicit=false;strcpy(network.setupSsid,"PLANT-TEST12");
    }
    const std::string singleQr=host::qrPayload();
    shortPress(host::now+100);
    if(!absent){bounds();assert(host::oledFrames().back().find("PLANT STATION")!=std::string::npos);assert(host::qrPayload()==singleQr);}
    shortPress(host::now+1200);
    if(!absent){bounds();assert(host::oledFrames().back().find("SOIL-01")!=std::string::npos);}
    shortPress(host::now+2200);
    if(!absent){bounds();assert(host::qrPayload()=="WIFI:T:WPA;S:PLANT-TEST12;P:\"0123456789ABCDEF\";;");}
    host::buttonPressed()=true;host::now+=100;loop();host::now+=40;loop();host::now+=3000;loop();
    host::buttonPressed()=false;host::now+=100;loop();host::now+=40;loop();
    assert(stopRequested && !Serial.flushed); // Main never waits synchronously on the worker.
    workerStopped=true;try{loop();assert(false);}catch(const SleepEntered&){}
    assert(Serial.flushed);
    puts("PASS actual dashboard main/radio/OLED: shared model, unknown battery, single adaptive QR (Android / Wi-Fi kit / acces local) with 4-module quiet zone and readable key, 3-screen cycle, 3s sleep / 8s setup release, OLED absent (worker and QR encoder simulated)");
}
