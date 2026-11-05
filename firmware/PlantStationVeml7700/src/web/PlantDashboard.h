#pragma once
#include "config.h"
#if PLANT_DASHBOARD
#include "ui/PlantView.h"
namespace plant {
struct DashboardNetwork {
    bool online = false, provisioning = false;
    char url[40] = {}, hostname[40] = {}, bluetoothName[32] = {}, proof[13] = {};
    char message[64] = {};
};
class PlantDashboard {
public:
    void begin();
    void publish(const PlantView& view);
    void configuration(PlantConfig* out) const;
    DashboardNetwork network() const;
    void requestStop();
    bool stopped() const;
};
}
#endif
