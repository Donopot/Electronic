#include "PlantBleSetup.h"
#if PLANT_DASHBOARD
#include "PlantDashboardApi.h"
#include <protocomm.h>
#include <protocomm_ble.h>
#include <protocomm_security1.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
namespace plant {
namespace {
protocomm_t* communication=nullptr;
QueueHandle_t commands=nullptr,snapshots=nullptr;
std::atomic<bool> commandReserved{false};
uint8_t proof[16]={};
protocomm_security_pop_t possession{};
protocomm_ble_name_uuid_t endpoints[]={
    {"proto-ver",0xFF50},{"prov-session",0xFF51},{"plant-config",0xFF52},{"plant-status",0xFF53}};
esp_err_t respond(const char* text,uint8_t** out,ssize_t* size){
    *size=strlen(text);*out=static_cast<uint8_t*>(malloc(*size));
    if(!*out)return ESP_ERR_NO_MEM;memcpy(*out,text,*size);return ESP_OK;
}
esp_err_t configure(uint32_t,const uint8_t* in,ssize_t size,uint8_t** out,ssize_t* outSize,void*){
    if(!in||size<1||size>512||memchr(in,0,size))return respond("{\"error\":\"invalid\"}",out,outSize);
    char text[513]={};memcpy(text,in,size);FormFields fields;BleSetupCommand command;
    uint64_t version=0,id=0;
    bool valid=parseForm(text,fields)&&fields.count==4&&parseUnsigned(fields.get("v"),1,version)&&version==1&&
        parseUnsigned(fields.get("id"),UINT32_MAX,id)&&id&&fields.get("ssid")&&fields.get("password");
    if(valid){
        const char* ssid=fields.get("ssid"),*password=fields.get("password");size_t n=strlen(password);
        valid=*ssid&&strlen(ssid)<=32&&(n==0||(n>=8&&n<=63)||n==64);
        if(n==64)for(size_t i=0;i<n;++i)valid&=hexDigit(password[i])>=0;
        if(valid){command.id=id;memcpy(command.ssid,ssid,strlen(ssid));memcpy(command.password,password,n);}
    }
    clearSetupSecret(text,sizeof(text));clearSetupSecret(&fields,sizeof(fields));
    bool expected=false;
    bool queued=valid&&commandReserved.compare_exchange_strong(expected,true);
    if(queued&&xQueueSend(commands,&command,0)!=pdTRUE){commandReserved.store(false);queued=false;}
    clearSetupSecret(&command,sizeof(command));
    return respond(!valid?"{\"error\":\"invalid\"}":queued?"{\"queued\":true}":"{\"error\":\"busy\"}",out,outSize);
}
esp_err_t status(uint32_t,const uint8_t* in,ssize_t size,uint8_t** out,ssize_t* outSize,void*){
    if(size!=1||!in||*in!='1')return respond("{\"error\":\"version\"}",out,outSize);
    BleSetupSnapshot value;
    if(xQueuePeek(snapshots,&value,0)!=pdTRUE)return respond("{\"v\":1,\"id\":0,\"state\":\"idle\"}",out,outSize);
    return respond(value.json,out,outSize);
}
}
void clearSetupSecret(void* data,size_t size){volatile uint8_t* p=static_cast<volatile uint8_t*>(data);while(size--)*p++=0;}
bool PlantBleSetup::active() const{return communication!=nullptr;}
bool PlantBleSetup::start(const char* name,const char* pop){
    if(active())return true;
    if(!name||!pop||strlen(name)>29||strlen(pop)!=16)return false;
    if(!commands)commands=xQueueCreate(1,sizeof(BleSetupCommand));
    if(!snapshots)snapshots=xQueueCreate(1,sizeof(BleSetupSnapshot));
    if(!commands||!snapshots)return false;
    memcpy(proof,pop,16);possession.data=proof;possession.len=16;
    communication=protocomm_new();if(!communication){clearSetupSecret(proof,sizeof(proof));return false;}
    protocomm_ble_config_t config{};snprintf(config.device_name,sizeof(config.device_name),"%s",name);
    // UUID 021a9004-0382-4aea-bff4-6b3f1c5adfb4, little endian SDK representation.
    const uint8_t uuid[]={0xb4,0xdf,0x5a,0x1c,0x3f,0x6b,0xf4,0xbf,0xea,0x4a,0x82,0x03,0x04,0x90,0x1a,0x02};
    memcpy(config.service_uuid,uuid,16);config.nu_lookup=endpoints;config.nu_lookup_count=4;
    if(protocomm_ble_start(communication,&config)!=ESP_OK){protocomm_delete(communication);communication=nullptr;clearSetupSecret(proof,sizeof(proof));return false;}
    if(protocomm_set_version(communication,"proto-ver","{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[]},\"plant\":{\"ver\":1}}")!=ESP_OK||
       protocomm_set_security(communication,"prov-session",&protocomm_security1,&possession)!=ESP_OK||
       protocomm_add_endpoint(communication,"plant-config",configure,nullptr)!=ESP_OK||
       protocomm_add_endpoint(communication,"plant-status",status,nullptr)!=ESP_OK){stop();return false;}
    return true;
}
void PlantBleSetup::stop(){
    if(communication){protocomm_ble_stop(communication);protocomm_delete(communication);communication=nullptr;}
    BleSetupCommand command;while(receive(command))clearSetupSecret(&command,sizeof(command));
    clearSetupSecret(proof,sizeof(proof));possession={};
}
bool PlantBleSetup::receive(BleSetupCommand& command){
    if(!commands||xQueueReceive(commands,&command,0)!=pdTRUE)return false;
    // Admission remains reserved while overwriting the queue's retained storage.
    // Removing an item alone does not erase its bytes in a FreeRTOS queue.
    BleSetupCommand empty;clearSetupSecret(&empty,sizeof(empty));
    xQueueOverwrite(commands,&empty);xQueueReceive(commands,&empty,0);
    commandReserved.store(false);return true;
}
void PlantBleSetup::publish(const BleSetupSnapshot& snapshot){if(snapshots)xQueueOverwrite(snapshots,&snapshot);}
}
#endif
