#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include "DashboardSdk.h"
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 257
#endif
using esp_err_t=int;
struct protocomm_security_t {int ver;};
struct protocomm_security_pop_t {const uint8_t* data;uint16_t len;};
using protocomm_req_handler_t=esp_err_t(*)(uint32_t,const uint8_t*,ssize_t,uint8_t**,ssize_t*,void*);
struct protocomm_t {std::map<std::string,protocomm_req_handler_t> handlers;bool security=false;std::string version;};
namespace host {
inline bool& bleRunning(){static bool v=false;return v;}
inline bool& failBleStart(){static bool v=false;return v;}
inline unsigned& bleStarts(){static unsigned v=0;return v;}
inline unsigned& bleStops(){static unsigned v=0;return v;}
// Explicit SDK boundary simulation, NOT a crypto implementation/proof.
inline std::string bleRequest(protocomm_t* pc,const char* endpoint,const std::string& input,bool authenticated=true){
    if(!pc||!pc->security||!authenticated)return "SDK_AUTH_REFUSED";
    auto found=pc->handlers.find(endpoint);if(found==pc->handlers.end())return "MISSING";
    uint8_t* output=nullptr;ssize_t length=0;
    if(found->second(1,reinterpret_cast<const uint8_t*>(input.data()),input.size(),&output,&length,nullptr)!=ESP_OK)return "SDK_ERROR";
    std::string result(reinterpret_cast<const char*>(output),length);free(output);return result;
}
}
inline protocomm_t* protocomm_new(){return new protocomm_t;}
inline void protocomm_delete(protocomm_t* pc){delete pc;}
inline esp_err_t protocomm_set_version(protocomm_t* pc,const char*,const char* version){pc->version=version;return ESP_OK;}
inline esp_err_t protocomm_set_security(protocomm_t* pc,const char*,const protocomm_security_t* sec,const protocomm_security_pop_t* pop){pc->security=sec->ver==1&&pop&&pop->len==16;return pc->security?ESP_OK:ESP_FAIL;}
inline esp_err_t protocomm_add_endpoint(protocomm_t* pc,const char* name,protocomm_req_handler_t handler,void*){pc->handlers[name]=handler;return ESP_OK;}
