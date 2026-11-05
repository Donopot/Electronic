#pragma once
#include "config.h"
#if PLANT_DASHBOARD
#include "ui/PlantView.h"
namespace plant {
struct DashboardNetwork {
    bool online = false, provisioning = false, setupSuccess = false, bleSetup = false;
    char url[40] = {}, hostname[40] = {}, setupSsid[20] = {}, setupPassword[17] = {};
    char setupUrl[32] = "http://192.168.4.1/";
    char message[64] = {};
};
class PlantDashboard {
public:
    void begin();
    void publish(const PlantView& view);
    void configuration(PlantConfig* out) const;
    DashboardNetwork network() const;
    void requestSetup();
    void requestStop();
    bool stopped() const;
};
}
#endif
