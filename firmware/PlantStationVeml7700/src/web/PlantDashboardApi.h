#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace plant {
inline bool parseUnsigned(const char* value, uint64_t maximum, uint64_t& out) {
    if(!value || !*value) return false;
    uint64_t result=0;
    for(const char* p=value;*p;++p) {
        if(*p<'0' || *p>'9' || result>maximum/10 || (result==maximum/10 && uint64_t(*p-'0')>maximum%10)) return false;
        result=result*10+*p-'0';
    }
    out=result; return true;
}
struct FormFields {
    char keys[5][20] = {}, values[5][160] = {};
    size_t count=0;
    const char* get(const char* key) const { for(size_t i=0;i<count;++i) if(!strcmp(key,keys[i])) return values[i]; return nullptr; }
};
inline int hexDigit(char c) { return c>='0'&&c<='9' ? c-'0' : c>='a'&&c<='f' ? c-'a'+10 : c>='A'&&c<='F' ? c-'A'+10 : -1; }
inline bool decodeFormPart(const char* p,size_t n,char* out,size_t capacity) {
    size_t length=0;
    for(size_t i=0;i<n;++i) {
        unsigned char c=p[i];
        if(c=='%') {
            if(i+2>=n || hexDigit(p[i+1])<0 || hexDigit(p[i+2])<0) return false;
            c=hexDigit(p[i+1])*16+hexDigit(p[i+2]); i+=2;
        } else if(c=='+') c=' ';
        if(!c || c<32 || c==127 || length+1>=capacity) return false;
        out[length++]=c;
    }
    out[length]=0; return true;
}
inline bool parseForm(const char* text,FormFields& out) {
    out=FormFields{};
    if(!text || !*text) return false;
    while(*text) {
        if(out.count>=5) return false;
        const char* end=strchr(text,'&'); if(!end) end=text+strlen(text);
        const char* equal=static_cast<const char*>(memchr(text,'=',end-text));
        if(!equal || equal==text || !decodeFormPart(text,equal-text,out.keys[out.count],20) ||
           !decodeFormPart(equal+1,end-equal-1,out.values[out.count],160)) return false;
        for(size_t i=0;i<out.count;++i) if(!strcmp(out.keys[i],out.keys[out.count])) return false;
        ++out.count;
        if(!*end) return true;
        text=end+1; if(!*text) return false;
    }
    return true;
}
}
