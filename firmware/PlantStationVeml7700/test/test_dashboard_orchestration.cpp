#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <freertos/task.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>
#include <esp_sleep.h>
#include "web/PlantDashboard.h"
#include <assert.h>
#include <string.h>
#include <algorithm>
namespace host {uint32_t now=0;}
FakeSerial Serial;FakeEsp ESP;TwoWire Wire;FakeSpi SPI;
namespace {
plant::PlantView latest;
plant::PlantConfig names[4];
plant::DashboardNetwork network;
bool stopRequested=false,workerStopped=false;
unsigned published=0;
}
// Worker boundary is simulated; the real setup/loop/radio/model/OLED/PRG execute.
namespace plant {
void PlantDashboard::begin(){defaultPlants(names);}
void PlantDashboard::publish(const PlantView& view){latest=view;++published;}
void PlantDashboard::configuration(PlantConfig* out)const{memcpy(out,names,sizeof(names));}
DashboardNetwork PlantDashboard::network()const{return ::network;}
void PlantDashboard::requestStop(){stopRequested=true;}
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
    host::now=1000;loop();assert(latest.luxValid && latest.radioReady && latest.warningMask==15);
    for(uint8_t id=1;id<=4;++id) {
        plantsoil::Packet p;p.node=id;p.session=77;p.sequence=1;p.address=0x48;p.status=plantsoil::AdcStatus::Ok;p.raw=16000+id*1000;
        uint8_t bytes[plantsoil::kPacketSize];assert(plantsoil::encode(p,bytes,sizeof(bytes))==plantsoil::Result::Accepted);
        host::inject(bytes,sizeof(bytes));host::now=1100+id;loop();
    }
    assert(latest.mood==plant::Mood::Neutral && latest.warningMask==0 && latest.dryMask==0);
    for(unsigned i=0;i<4;++i){assert(latest.plants[i].id==i+1 && !latest.plants[i].batteryKnown);}
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
    }
    host::now=1104+plantsoil::kStaleMs;loop();assert(latest.warningMask==15 && latest.dryMask==0 && latest.mood==plant::Mood::Worried);
    host::buttonPressed()=true;host::now+=100;loop();host::now+=40;loop();host::now+=3000;loop();
    host::buttonPressed()=false;host::now+=100;loop();host::now+=40;loop();
    assert(stopRequested && !Serial.flushed); // Main never waits synchronously on the worker.
    workerStopped=true;try{loop();assert(false);}catch(const SleepEntered&){}
    assert(Serial.flushed);
    puts("PASS actual dashboard main/radio/OLED: shared 4-node model/names, unknown battery, offline, QR layout contract, stale, lux, nonblocking worker-stop gate, PRG sleep (worker and QR encoder simulated)");
}
