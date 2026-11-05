#include "ui/PlantView.h"
#include "storage/PlantHistory.h"
#include "web/PlantDashboardApi.h"
#include "power/PrgButton.h"
#include "web/CaptiveDns.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
using namespace plant;
class MemoryFlash : public HistoryFlash {
public:
    std::vector<uint8_t> bytes;
    size_t writes=0,erases=0;
    int cut=-1;
    bool failReads=false;
    explicit MemoryFlash(size_t size=0x180000) : bytes(size,255) {}
    size_t size() const override {return bytes.size();}
    bool read(size_t offset,void* out,size_t length) override {if(failReads||offset+length>size())return false;memcpy(out,bytes.data()+offset,length);return true;}
    bool write(size_t offset,const void* in,size_t length) override {
        ++writes;const auto* p=static_cast<const uint8_t*>(in);
        if(offset+length>size())return false;
        const size_t n=cut<0?length:std::min(size_t(cut),length);
        for(size_t i=0;i<n;++i){assert((bytes[offset+i]&p[i])==p[i]);bytes[offset+i]&=p[i];}
        return cut<0;
    }
    bool erase(size_t offset,size_t length) override {++erases;if(offset+length>size())return false;std::fill(bytes.begin()+offset,bytes.begin()+offset+length,255);return true;}
};
void packet(plantsoil::ReceiverBank& bank,uint8_t id,uint32_t now,plantsoil::AdcStatus status=plantsoil::AdcStatus::Ok,int16_t raw=16000) {
    plantsoil::Packet p;p.node=id;p.session=1;p.sequence=now;p.status=status;p.address=0x48;p.raw=status==plantsoil::AdcStatus::Ok?raw:0;
    uint8_t bytes[plantsoil::kPacketSize];assert(plantsoil::encode(p,bytes,sizeof(bytes))==plantsoil::Result::Accepted);
    assert(bank.accept(bytes,sizeof(bytes),now)==plantsoil::Result::Accepted);
}
static void model() {
    PlantConfig config[4];defaultPlants(config);plantsoil::ReceiverBank bank;
    auto view=makePlantView(bank,config,true,0,0);
    assert(view.mood==Mood::Worried && view.warningMask==15 && view.dryMask==0);
    for(uint8_t i=1;i<=4;++i)packet(bank,i,100);
    view=makePlantView(bank,config,true,100,100);
    assert(view.mood==Mood::Neutral && view.warningMask==0 && view.dryMask==0);
    for(const auto& p:view.plants)assert(p.state==PlantState::Uncalibrated && !p.batteryKnown && p.voltageValid && p.microvolts==2000000);
    for(auto& c:config){c.calibrated=true;c.dryMicrovolts=2500000;}
    view=makePlantView(bank,config,true,101,101);assert(view.mood==Mood::Happy);
    packet(bank,1,102,plantsoil::AdcStatus::Ok,24000);packet(bank,4,102,plantsoil::AdcStatus::ReadError);
    view=makePlantView(bank,config,true,102,102);
    assert(view.mood==Mood::Sad && view.dryMask==1 && view.warningMask==8);
    assert(view.plants[3].state==PlantState::AdcFault && !view.plants[3].voltageValid);
    view=makePlantView(bank,config,true,100+plantsoil::kStaleMs,100+plantsoil::kStaleMs);
    assert(view.plants[1].state==PlantState::Stale && !(view.dryMask&2));
    plantsoil::ReceiverBank reboot;
    view=makePlantView(reboot,config,true,0,0);assert(view.warningMask==15 && view.dryMask==0);
    assert(validPlantName("Basilic du balcon"));assert(validPlantName("Été 🌿"));
    assert(!validPlantName(" "));assert(!validPlantName("plante "));assert(!validPlantName("ligne\n2"));
    assert(!validPlantName("1234567890123456789012345"));assert(!validPlantName("\xC0\xAF"));assert(!validPlantName("\xED\xA0\x80"));
    puts("PASS shared model: neutral/healthy/dry + independent warnings, battery unknown, stale, reboot, UTF-8 names");
}
static void historyTest() {
    MemoryFlash flash;PlantHistory history(flash);assert(history.begin());
    const uint64_t start=1793491200000ULL;
    // 31 days, every half hour, four distinct probes; omit one point and one ADC fault.
    for(unsigned slot=0;slot<=31*48;++slot)for(unsigned id=1;id<=4;++id) {
        if(slot==100 && id==2)continue;
        HistoryRecord r;r.boot=123;r.session=8;r.sequence=slot;r.node=id;r.raw=8000+id*1000;r.uptimeMs=uint64_t(slot)*1800000;r.utcMs=start+r.uptimeMs;
        if(slot==101 && id==3){r.status=2;r.raw=0;}
        assert(history.append(r));
    }
    PlantHistory reboot(flash);assert(reboot.begin());
    for(unsigned id=1;id<=4;++id) {
        uint64_t after=0,next;bool more;size_t count=0,faults=0;HistoryRecord out[96];
        do {
            size_t n=reboot.page(id,after,start+31ULL*86400000,kHistoryMs,out,96,next,more);
            for(size_t i=0;i<n;++i){assert(out[i].node==id);assert(out[i].utcMs>=start+86400000);assert(!out[i].status || (id==3 && out[i].sequence==101));if(out[i].status)++faults;else assert(out[i].raw==int(8000+id*1000));}
            assert(!more||next>after);after=next;count+=n;
        }while(more);
        assert(count==(id==2?1440:1441));assert(faults==(id==3?1:0));
    }
    HistoryRecord unknown;unknown.boot=124;unknown.node=1;unknown.raw=16000;assert(reboot.append(unknown));
    // An interrupted record is skipped; subsequent writes do not overwrite it.
    flash.cut=17;assert(!reboot.append(unknown));flash.cut=-1;
    PlantHistory interrupted(flash);assert(interrupted.begin());assert(interrupted.corruptRecords()==1);assert(interrupted.append(unknown));
    uint64_t next;bool more;HistoryRecord out[96];
    const auto n=interrupted.page(1,0,start+31ULL*86400000,86400000,out,96,next,more);
    assert(n>0); // History survives independently of live receiver data.
    MemoryFlash unknownFlash;unknownFlash.bytes[200000]=0x42;
    PlantHistory unsafe(unknownFlash);assert(!unsafe.begin());assert(unsafe.state()==PlantHistory::State::UnknownData);assert(!unknownFlash.writes&&!unknownFlash.erases);
    MemoryFlash bad;bad.failReads=true;PlantHistory failed(bad);assert(!failed.begin());assert(failed.state()==PlantHistory::State::IoError);
    MemoryFlash tiny(4096);PlantHistory tooSmall(tiny);assert(!tooSmall.begin());assert(!tiny.writes);
    puts("PASS history: 31 simulated days / four nodes / 30-day filter / pagination / gaps / ADC faults / reboot / interrupted writes / unknown data preserved");
}
static void ring() {
    MemoryFlash flash(60*4096);PlantHistory history(flash);assert(history.begin());
    HistoryRecord r;r.node=1;r.boot=1;r.raw=1000;
    for(unsigned i=0;i<8000;++i){r.sequence=i;assert(history.append(r));}
    assert(flash.erases>59);
    PlantHistory reboot(flash);assert(reboot.begin());
    uint64_t after=0,next;bool more;HistoryRecord out[96];size_t count=0;unsigned previous=0;
    do {size_t n=reboot.page(1,after,0,kHistoryMs,out,96,next,more);for(size_t i=0;i<n;++i){assert(!count||out[i].sequence>previous);previous=out[i].sequence;++count;}after=next;}while(more);
    assert(previous==7999 && count<=59*101 && count>58*101);
    puts("PASS full flash: bounded sector ring wraps and reboots without losing newest records");
}
static void anchorsAndCorruption() {
    MemoryFlash flash;PlantHistory history(flash);assert(history.begin());
    const uint64_t now=1793700000000ULL,day=86400000,up=40*day;
    const uint64_t times[]={up-30*day-1,up-30*day,up-7*day-1,up-7*day,up-day-1,up-day,up-1000,up+1};
    for(unsigned i=0;i<8;++i){HistoryRecord r;r.node=1;r.boot=5;r.uptimeMs=times[i];r.sequence=i;r.raw=16000;assert(history.append(r));}
    HistoryRecord values[96];uint64_t next;bool more;
    assert(history.page(1,0,now,kHistoryMs,values,96,next,more)==8&&values[0].utcMs==0);
    assert(history.anchor(5,up,now));
    for(const unsigned days:{1,7,30}) {
        const size_t expected=days==1?2:days==7?4:6;
        assert(history.page(1,0,now,days*day,values,96,next,more)==expected);
        uint64_t after=0;size_t count=0;
        do{size_t n=history.page(1,after,now,days*day,values,1,next,more);assert(n==1&&next>after);assert(values[0].utcMs>=now-days*day&&values[0].utcMs<=now);after=next;++count;}while(more);
        assert(count==expected);
    }
    PlantHistory reboot(flash);assert(reboot.begin());assert(reboot.page(1,0,now,day,values,96,next,more)==2&&values[0].utcMs==now-day);
    MemoryFlash failure;PlantHistory failed(failure);assert(failed.begin());HistoryRecord r;r.node=1;r.boot=9;r.uptimeMs=1;assert(failed.append(r));
    failure.cut=8;assert(!failed.anchor(9,10,now));failure.cut=-1;PlantHistory retry(failure);assert(retry.begin());
    assert(retry.page(1,0,now,day,values,96,next,more)==1&&values[0].utcMs==0);
    MemoryFlash rotated;PlantHistory old(rotated);assert(old.begin());r.boot=2;
    for(unsigned i=0;i<110;++i){r.sequence=i;assert(old.append(r));}
    rotated.bytes[4096+32+5]^=1; // Corruption is in the older sector, not the current one.
    PlantHistory corrupt(rotated);assert(corrupt.begin()&&corrupt.corruptRecords()==1);
    for(unsigned i=0;i<3;++i)corrupt.page(1,0,0,kHistoryMs,values,96,next,more);
    assert(corrupt.corruptRecords()==1);
    // More boot anchors than the RAM cache must still resolve from durable flash.
    MemoryFlash many;PlantHistory cached(many);assert(cached.begin());
    for(unsigned boot=1;boot<=70;++boot){r.boot=boot;r.uptimeMs=1;r.utcMs=0;assert(cached.append(r));assert(cached.anchor(boot,2,now));}
    PlantHistory manyReboot(many);assert(manyReboot.begin());assert(manyReboot.page(1,0,now,day,values,96,next,more)==70);
    for(unsigned i=0;i<70;++i)assert(values[i].utcMs==now-1);
    puts("PASS durable anchors: undated/synced/reboot, failed persistence, effective-date filtering before 1/7/30-day pagination, future/old/boundary records, cache overflow; corruption counted once across retained sectors");
}
static void apiAndNavigation() {
    uint64_t value;assert(parseUnsigned("18446744073709551615",UINT64_MAX,value)&&value==UINT64_MAX);
    assert(!parseUnsigned("18446744073709551616",UINT64_MAX,value));assert(!parseUnsigned("-1",4,value));assert(!parseUnsigned("",4,value));
    FormFields fields;assert(parseForm("id=4&name=%C3%89t%C3%A9+balcon",fields));assert(fields.count==2&&validPlantName(fields.get("name")));
    assert(!parseForm("id=1&id=2",fields));assert(!parseForm("name=%00bad",fields));assert(!parseForm("name=%xx",fields));assert(!parseForm("id=1&",fields));
    Screen screen=Screen::Face;PrgButton button;button.begin(0,false);button.update(100,true,true);button.update(140,true,true);button.update(200,false,true);
    assert(button.update(240,false,true)==ButtonEvent::ShortPress);screen=nextScreen(screen);assert(screen==Screen::Details);
    screen=nextScreen(screen);assert(screen==Screen::Qr);screen=nextScreen(screen);assert(screen==Screen::Face);
    button.update(300,true,true);button.update(340,true,true);assert(button.update(3300,true,true)==ButtonEvent::LongPress);
    button.update(3400,false,true);assert(button.update(3440,false,true)==ButtonEvent::ReleasedForSleep);
    PrgButton delayed;delayed.begin(0,false);delayed.update(100,true,true);delayed.update(140,true,true);
    assert(delayed.update(8100,true,true)==ButtonEvent::SetupHold);
    assert(delayed.update(8140,true,true)==ButtonEvent::None); // No late 3s event after the 8s confirmation.
    delayed.update(8200,false,true);assert(delayed.update(8240,false,true)==ButtonEvent::ReleasedForSetup);
    PrgButton boot;boot.begin(0,true);assert(boot.update(9000,true,true)==ButtonEvent::None);
    puts("PASS API validation and PRG face/details/QR, 3-second threshold and release-to-sleep");
}
static void dns() {
    const uint8_t valid[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};
    uint8_t out[528],query[512];
    const size_t n=captiveDnsReply(valid,sizeof(valid),out,sizeof(out));assert(n==sizeof(valid)+16&&out[0]==0x12&&out[n-4]==192&&out[n-1]==1&&out[7]==1);
    for(size_t length=0;length<sizeof(valid);++length)assert(!captiveDnsReply(valid,length,out,sizeof(out)));
    memcpy(query,valid,sizeof(valid));query[12]=0xC0;assert(!captiveDnsReply(query,sizeof(valid),out,sizeof(out)));
    memcpy(query,valid,sizeof(valid));query[12]=64;assert(!captiveDnsReply(query,sizeof(valid),out,sizeof(out)));
    memcpy(query,valid,sizeof(valid));query[5]=2;assert(!captiveDnsReply(query,sizeof(valid),out,sizeof(out)));
    memcpy(query,valid,sizeof(valid));query[sizeof(valid)-3]=28;assert(!captiveDnsReply(query,sizeof(valid),out,sizeof(out)));
    assert(!captiveDnsReply(valid,sizeof(valid),out,n-1));
    memcpy(query,valid,sizeof(valid));query[11]=1;const uint8_t opt[]={0,0,41,2,0,0,0,0,0,0,0};memcpy(query+sizeof(valid),opt,sizeof(opt));
    assert(captiveDnsReply(query,sizeof(valid)+sizeof(opt),out,sizeof(out))==n);
    assert(!captiveDnsReply(query,sizeof(valid)+sizeof(opt)-1,out,sizeof(out)));
    puts("PASS bounded captive DNS: A/IN reply, short/truncated datagrams and labels, compression/multiple questions/AAAA rejected, EDNS bounds and output capacity");
}
int main(){model();historyTest();ring();anchorsAndCorruption();apiAndNavigation();dns();}
