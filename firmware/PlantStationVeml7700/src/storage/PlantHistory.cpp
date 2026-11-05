#include "config.h"
#if PLANT_DASHBOARD
#include "PlantHistory.h"
#include <string.h>
#include <algorithm>
namespace plant {
namespace {
const uint8_t owner[16] = {'P','L','A','N','T','H','I','S','T','0','1',0xA5,0x5A,0x7E,0x81,1};
const uint32_t sectorMagic = 0x314A5350;
void put64(uint8_t* p, uint64_t x) { for (unsigned i=0;i<8;++i) p[i] = x >> (8*i); }
uint64_t get64(const uint8_t* p) { uint64_t x=0; for (unsigned i=0;i<8;++i) x |= uint64_t(p[i]) << (8*i); return x; }
bool erased(const uint8_t* p, size_t n) { for (size_t i=0;i<n;++i) if (p[i]!=255) return false; return true; }
}
uint32_t PlantHistory::crc(const uint8_t* bytes, size_t size) {
    uint32_t c=0xFFFFFFFF;
    for(size_t i=0;i<size;++i) { c ^= bytes[i]; for(unsigned b=0;b<8;++b) c=(c>>1)^(0xEDB88320U & (0U-(c&1))); }
    return ~c;
}
const char* PlantHistory::stateName(State state) {
    switch(state) { case State::Ready:return "ready"; case State::UnknownData:return "unknown_data_preserved";
        case State::TooSmall:return "partition_too_small"; default:return "io_error"; }
}
bool PlantHistory::sectorGeneration(size_t sector, uint32_t& generation) {
    uint8_t bytes[kHeader];
    if(!flash_.read(sector*kSector,bytes,sizeof(bytes))) { state_=State::IoError; return false; }
    if(plantsoil::get32(bytes)!=sectorMagic || plantsoil::get32(bytes+8)!=crc(bytes,8)) return false;
    generation=plantsoil::get32(bytes+4);
    return generation && (generation-1) % sectors_ + 1 == sector;
}
bool PlantHistory::begin() {
    state_=State::IoError;
    sectors_=flash_.size()/kSector;
    if(sectors_<60) { state_=State::TooSmall; return false; }
    --sectors_; // Sector zero is a permanent ownership marker, never erased.
    uint8_t bytes[256];
    if(!flash_.read(0,bytes,sizeof(bytes))) return false;
    if(memcmp(bytes,owner,sizeof(owner)) || plantsoil::get32(bytes+16)!=crc(owner,sizeof(owner))) {
        // Inspect every byte before initializing a blank partition. Never mount or format SPIFFS.
        for(size_t offset=0;offset<flash_.size();offset+=sizeof(bytes)) {
            const auto n=std::min(sizeof(bytes),flash_.size()-offset);
            if(!flash_.read(offset,bytes,n)) return false;
            if(!erased(bytes,n)) { state_=State::UnknownData; return false; }
        }
        memset(bytes,255,32); memcpy(bytes,owner,sizeof(owner)); plantsoil::put32(bytes+16,crc(owner,sizeof(owner)));
        if(!flash_.write(0,bytes,32)) return false;
    }
    state_=State::Ready; generation_=0; slot_=kPerSector; corrupt_=0;anchorCount_=anchorNext_=0;
    for(size_t sector=1;sector<=sectors_;++sector) {
        uint32_t generation;
        if(sectorGeneration(sector,generation) && generation>generation_) generation_=generation;
        if(state_!=State::Ready) return false;
    }
    if(!generation_) return startSector(1);
    slot_=0;
    const uint32_t first=generation_>sectors_?generation_-sectors_+1:1;
    for(uint32_t generation=first;generation<=generation_;++generation) {
        const size_t sector=(generation-1)%sectors_+1;uint32_t actual=0;
        if(!sectorGeneration(sector,actual)||actual!=generation) {if(state_!=State::Ready)return false;continue;}
        for(size_t slot=0;slot<kPerSector;++slot) {
            uint8_t record[kRecord];
            if(!flash_.read(sector*kSector+kHeader+slot*kRecord,record,sizeof(record))) {state_=State::IoError;return false;}
            if(erased(record,sizeof(record)))continue;
            if(generation==generation_)slot_=slot+1;
            HistoryRecord decoded;
            if(!decode(record,decoded))++corrupt_;
            else if(!decoded.node)rememberAnchor(decoded);
        }
    }
    return true;
}
bool PlantHistory::startSector(uint32_t generation) {
    const size_t sector=(generation-1)%sectors_+1;
    uint8_t bytes[kHeader]; memset(bytes,255,sizeof(bytes));
    plantsoil::put32(bytes,sectorMagic); plantsoil::put32(bytes+4,generation); plantsoil::put32(bytes+8,crc(bytes,8));
    if(!flash_.erase(sector*kSector,kSector) || !flash_.write(sector*kSector,bytes,sizeof(bytes))) {state_=State::IoError; return false;}
    generation_=generation; slot_=0; return true;
}
bool PlantHistory::decode(const uint8_t* bytes, HistoryRecord& value) const {
    if(plantsoil::get32(bytes+36)!=crc(bytes,36) ||
       (!(plantsoil::validNode(bytes[0])&&bytes[1]<=3) && !(bytes[0]==0&&bytes[1]==0xA1))) return false;
    value.node=bytes[0]; value.status=bytes[1];
    const uint16_t raw=bytes[2]|(uint16_t(bytes[3])<<8);
    value.raw=static_cast<int16_t>(raw<32768 ? int32_t(raw) : int32_t(raw)-65536);
    value.boot=plantsoil::get32(bytes+4); value.session=plantsoil::get32(bytes+8); value.sequence=plantsoil::get32(bytes+12);
    value.uptimeMs=get64(bytes+16); value.utcMs=get64(bytes+24);
    if(!value.node&&(!value.boot||!value.utcMs))return false;
    return true;
}
bool PlantHistory::append(const HistoryRecord& value) {
    if(state_!=State::Ready || (!(plantsoil::validNode(value.node)&&value.status<=3) &&
       !(value.node==0&&value.status==0xA1&&value.boot&&value.utcMs))) return false;
    if(slot_==kPerSector && !startSector(generation_+1)) return false;
    uint8_t bytes[kRecord]={}; bytes[0]=value.node; bytes[1]=value.status;
    bytes[2]=uint16_t(value.raw); bytes[3]=uint16_t(value.raw)>>8;
    plantsoil::put32(bytes+4,value.boot); plantsoil::put32(bytes+8,value.session); plantsoil::put32(bytes+12,value.sequence);
    put64(bytes+16,value.uptimeMs); put64(bytes+24,value.utcMs); plantsoil::put32(bytes+36,crc(bytes,36));
    const size_t offset=((generation_-1)%sectors_+1)*kSector+kHeader+slot_++*kRecord;
    if(!flash_.write(offset,bytes,sizeof(bytes))) {state_=State::IoError; return false;}
    return true;
}
void PlantHistory::rememberAnchor(const HistoryRecord& value) {
    for(size_t i=0;i<anchorCount_;++i)if(anchors_[i].boot==value.boot){anchors_[i]=value;return;}
    anchors_[anchorNext_]=value;anchorNext_=(anchorNext_+1)%64;if(anchorCount_<64)++anchorCount_;
}
bool PlantHistory::anchor(uint32_t boot,uint64_t uptimeMs,uint64_t utcMs) {
    HistoryRecord value;value.boot=boot;value.uptimeMs=uptimeMs;value.utcMs=utcMs;value.status=0xA1;
    if(!append(value))return false;
    // Verify before publishing an established date; a failed write remains unknown.
    uint8_t bytes[kRecord];HistoryRecord check;
    const size_t offset=((generation_-1)%sectors_+1)*kSector+kHeader+(slot_-1)*kRecord;
    if(!flash_.read(offset,bytes,sizeof(bytes))||!decode(bytes,check)||check.node||check.boot!=boot||check.uptimeMs!=uptimeMs||check.utcMs!=utcMs) {state_=State::IoError;return false;}
    rememberAnchor(value);return true;
}
uint64_t PlantHistory::resolveUtc(const HistoryRecord& value) {
    if(value.utcMs)return value.utcMs;
    HistoryRecord anchor;bool found=false;
    for(size_t i=0;i<anchorCount_;++i)if(anchors_[i].boot==value.boot){anchor=anchors_[i];found=true;break;}
    // The small RAM index is a cache, never a cap on durable boot anchors.
    if(!found) {
        const uint32_t first=generation_>sectors_?generation_-sectors_+1:1;
        for(uint32_t generation=generation_;generation>=first&&!found;--generation) {
            const size_t sector=(generation-1)%sectors_+1;uint32_t actual=0;
            if(!sectorGeneration(sector,actual)||actual!=generation)continue;
            for(size_t slot=generation==generation_?slot_:kPerSector;slot>0;--slot) {
                uint8_t bytes[kRecord];HistoryRecord candidate;
                if(!flash_.read(sector*kSector+kHeader+(slot-1)*kRecord,bytes,sizeof(bytes))){state_=State::IoError;return 0;}
                if(decode(bytes,candidate)&&candidate.node==0&&candidate.boot==value.boot){anchor=candidate;rememberAnchor(anchor);found=true;break;}
            }
        }
    }
    if(!found){anchor.boot=value.boot;rememberAnchor(anchor);return 0;}
    if(!anchor.utcMs)return 0;
    if(value.uptimeMs>=anchor.uptimeMs)return anchor.utcMs+value.uptimeMs-anchor.uptimeMs;
    const auto delta=anchor.uptimeMs-value.uptimeMs;return anchor.utcMs>=delta?anchor.utcMs-delta:0;
}
size_t PlantHistory::page(uint8_t node,uint64_t after,uint64_t nowUtc,uint64_t periodMs,
                         HistoryRecord* out,size_t limit,uint64_t& next,bool& more) {
    next=after; more=false;
    if(state_!=State::Ready || !plantsoil::validNode(node) || !limit) return 0;
    limit=std::min(limit,size_t(96)); periodMs=std::min(periodMs,kHistoryMs);
    const uint32_t first=generation_>sectors_ ? generation_-sectors_+1 : 1;
    size_t count=0;
    for(uint32_t generation=first;generation<=generation_;++generation) {
        const size_t sector=(generation-1)%sectors_+1;
        uint32_t actual=0;
        if(!sectorGeneration(sector,actual) || actual!=generation) continue;
        const size_t slots=generation==generation_ ? slot_ : kPerSector;
        for(size_t slot=0;slot<slots;++slot) {
            const uint64_t cursor=uint64_t(generation)*kPerSector+slot;
            if(cursor<=after) continue;
            uint8_t bytes[kRecord]; HistoryRecord value;
            if(!flash_.read(sector*kSector+kHeader+slot*kRecord,bytes,sizeof(bytes))) {state_=State::IoError; return count;}
            if(!decode(bytes,value) || value.node!=node) continue;
            value.utcMs=resolveUtc(value);
            if(state_!=State::Ready)return count;
            // Unknown clock records stay explicitly undated. Never manufacture dates across boots.
            if(nowUtc && value.utcMs && (value.utcMs>nowUtc || nowUtc-value.utcMs>periodMs)) continue;
            if(count==limit) {more=true; return count;}
            out[count++]=value; next=cursor;
        }
    }
    return count;
}
}

#endif
