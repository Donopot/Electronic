#pragma once
#include "config.h"
#if PLANT_DASHBOARD
#include "ui/PlantView.h"
namespace plant {
struct DashboardNetwork {
    bool online = false, provisioning = false, setupSuccess = false, bleSetup = false;
    // Vrai seulement si l'assistant a ete demande explicitement (PRG 8 s) :
    // le QR d'appairage Android n'apparait alors qu'a la demande.
    bool setupExplicit = false;
    char url[40] = {}, hostname[40] = {}, setupSsid[20] = {}, setupPassword[17] = {};
    char apPassword[24] = {};
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
