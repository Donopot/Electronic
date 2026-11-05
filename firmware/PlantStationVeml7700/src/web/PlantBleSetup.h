#pragma once
#include "config.h"
#if PLANT_DASHBOARD
#include <stdint.h>
#include <stddef.h>
namespace plant {
// All messages are copied. Only the dashboard worker owns Wi-Fi/NVS/Strings.
struct BleSetupCommand { uint32_t id=0; char ssid[33]={}, password[65]={}; };
struct BleSetupSnapshot { char json[384]={}; };
class PlantBleSetup {
public:
    bool start(const char* name,const char* pop);
    void stop();
    bool active() const;
    bool receive(BleSetupCommand& command);
    void publish(const BleSetupSnapshot& snapshot);
};
void clearSetupSecret(void* data,size_t size);
}
#endif
