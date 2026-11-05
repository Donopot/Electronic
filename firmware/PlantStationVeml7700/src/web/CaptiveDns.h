#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace plant {
// A single uncompressed A/IN question, optionally followed by one bounded EDNS OPT.
// No SDK DNSServer parser touches untrusted packet lengths or DNS labels.
inline size_t captiveDnsReply(const uint8_t* query,size_t length,uint8_t* out,size_t capacity) {
    if(!query||!out||length<12||length>512||capacity<12||(query[2]&0xF8)||
       query[4]||query[5]!=1||query[6]||query[7]||query[8]||query[9]||query[10]||query[11]>1)return 0;
    size_t at=12,nameBytes=0;
    while(true){
        if(at>=length)return 0;
        const uint8_t label=query[at++];
        if(label&0xC0||label>63||at+label>length)return 0;
        nameBytes+=label+1;if(nameBytes>255)return 0;
        if(!label)break;at+=label;
    }
    if(nameBytes<=1||at+4>length||query[at]||query[at+1]!=1||query[at+2]||query[at+3]!=1)return 0;
    const size_t questionEnd=at+4;
    at=questionEnd;
    if(query[11]){
        if(at+11>length||query[at]||query[at+1]||query[at+2]!=41)return 0;
        const size_t bytes=(size_t(query[at+9])<<8)|query[at+10];at+=11;
        if(bytes>length-at)return 0;at+=bytes;
    }
    if(at!=length||questionEnd+16>capacity)return 0;
    memcpy(out,query,questionEnd);out[2]=0x80|(query[2]&1);out[3]=0x80;
    out[6]=0;out[7]=1;out[8]=out[9]=out[10]=out[11]=0;
    const uint8_t answer[]={0xC0,0x0C,0,1,0,1,0,0,0,0,0,4,192,168,4,1};
    memcpy(out+questionEnd,answer,sizeof(answer));return questionEnd+sizeof(answer);
}
}
