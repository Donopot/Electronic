#pragma once
#include "protocomm.h"
struct protocomm_ble_name_uuid_t {const char* name;uint16_t uuid;};
struct protocomm_ble_config_t {char device_name[30];uint8_t service_uuid[16];protocomm_ble_name_uuid_t* nu_lookup;ssize_t nu_lookup_count;};
inline esp_err_t protocomm_ble_start(protocomm_t*,const protocomm_ble_config_t* config){
    if(host::failBleStart())return ESP_FAIL;
    assert(strlen(config->device_name)<=29&&config->nu_lookup_count==4);
    host::bleRunning()=true;++host::bleStarts();return ESP_OK;
}
inline esp_err_t protocomm_ble_stop(protocomm_t*){host::bleRunning()=false;++host::bleStops();return ESP_OK;}
