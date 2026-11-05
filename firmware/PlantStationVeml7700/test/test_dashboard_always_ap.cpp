// AP permanent : le tableau de bord doit rester joignable sur le Wi-Fi du kit,
// sans configuration, et l'AP ne doit plus se fermer apres une connexion reussie.
// Compile avec -DPLANT_ALWAYS_AP=1, en incluant les sources de production.
#include "../src/web/PlantBleSetup.cpp"
#include "../src/web/PlantDashboard.cpp"
#include <assert.h>
#include <stdio.h>
#include <string>
namespace host {uint32_t now=0;}
FakeSerial Serial;FakeEsp ESP;
using namespace plant;
static std::shared_ptr<host::Connection> queueHttp(const std::string& method,const std::string& path,const std::string& body="",const std::string& hostname="192.168.4.1",bool ap=true,const std::string& origin="") {
    auto connection=std::make_shared<host::Connection>();
    connection->local=ap?"192.168.4.1":"192.168.1.10";
    connection->request=method+" "+path+" HTTP/1.1\r\nHost: "+hostname+"\r\n";
    if(method=="POST")connection->request+="Content-Type: application/x-www-form-urlencoded\r\nX-Plant-Request: 1\r\nContent-Length: "+std::to_string(body.size())+"\r\n";
    if(method=="POST")connection->request+="Origin: "+(origin.empty()?"http://"+hostname:origin)+"\r\n";
    connection->request+="\r\n"+body;host::accepted()=connection;host::socketSend().bytes.clear();return connection;
}
static const std::string& completeHttp(){
    const auto& bytes=host::socketSend().bytes;const auto split=bytes.find("\r\n\r\n");assert(split!=std::string::npos);
    const auto lengthAt=bytes.find("Content-Length: ");assert(lengthAt!=std::string::npos);
    const auto length=std::stoul(bytes.substr(lengthAt+16));
    assert(bytes.size()-split-4==length);return bytes;
}
static std::string requestHttpSync(const std::string& method,const std::string& path,const std::string& body="",const std::string& hostname="192.168.4.1",bool ap=true) {
    const auto connection=queueHttp(method,path,body,hostname,ap);auto& socket=host::socketSend();
    for(unsigned tick=0;connection->open&&tick<40000;++tick){socket.result=tick%3?-1:512;socket.error=EAGAIN;serviceHttp(host::now++);}
    assert(!connection->open);
    return completeHttp();
}
static void seedNetwork(const char* ssid,const char* password) {
    wifi_config_t config{};memcpy(config.sta.ssid,ssid,strlen(ssid));memcpy(config.sta.password,password,strlen(password));
    host::net().ram=host::net().flash=config;loadSavedWifi();assert(setRamStorage());
}
int main() {
    static_assert(PLANT_ALWAYS_AP==1,"ce test ne vaut que pour le profil AP permanent");
    PlantDashboard dashboard;
    dashboard.begin();
    unsigned stage=0;
    std::shared_ptr<host::Connection> pending;
    host::runTask(6000,[&](unsigned tick){
        if(!tick)return;
        auto& socket=host::socketSend();socket.result=4096;socket.error=0;
        if(stage==0){
            // Demarrage : l'AP est ouvert sans aucune configuration.
            assert(provisioning&&host::net().apActive);
            assert(host::net().ap.ap.authmode==(kOpenAp?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK));
#if PLANT_OPEN_AP
            // AP ouvert : aucune cle, le formulaire reste joignable pendant la fenetre.
            assert(requestHttpSync("GET","/setup").find(PLANT_SETUP_PAGE)!=std::string::npos);
            assert(startSetup(true)&&setupExplicit);
#endif
            pending=queueHttp("GET","/");stage=1;
        } else if(stage==1&&pending&&!pending->open){
            // Racine du kit = tableau de bord complet.
            assert(completeHttp().find(PLANT_DASHBOARD_PAGE)!=std::string::npos);
            pending=queueHttp("GET","/setup");stage=2;
        } else if(stage==2&&pending&&!pending->open){
            // Formulaire Wi-Fi conserve, mais optionnel.
            assert(completeHttp().find(PLANT_SETUP_PAGE)!=std::string::npos);
            pending=queueHttp("GET","/generate_204","","connectivitycheck.gstatic.com");stage=3;
        } else if(stage==3&&pending&&!pending->open){
            // Portail captif : ouverture directe du tableau de bord.
            assert(completeHttp().find("Location: /")!=std::string::npos);
            pending=queueHttp("GET","/api/status");stage=4;
        } else if(stage==4&&pending&&!pending->open){
            const auto& reply=completeHttp();
            assert(reply.find("200 OK")!=std::string::npos&&reply.find("\"api_version\":2")!=std::string::npos);
            pending=queueHttp("POST","/api/plants","id=1&name=Basilic");stage=5;
        } else if(stage==5&&pending&&!pending->open){
            assert(completeHttp().find("202 Accepted")!=std::string::npos);
            // Connexion domestique reussie : le Wi-Fi du kit doit rester actif.
            seedNetwork("Home","HomePassword");host::gotIp("192.168.7.9");stage=6;
        } else if(stage==6){
            assert(online&&provisioning&&host::net().apActive);
            setupState="connected";successAt=host::now;host::now+=kSetupSuccessOpenMs+1;stage=7;
        } else if(stage==7){
            assert(provisioning&&host::net().apActive&&!bleSetup.active());
            pending=queueHttp("GET","/");stage=8;
        } else if(stage==8&&pending&&!pending->open){
            assert(completeHttp().find(PLANT_DASHBOARD_PAGE)!=std::string::npos);
            // Le reseau domestique garde son propre acces.
            pending=queueHttp("GET","/api/status","","192.168.7.9",false);stage=9;
        } else if(stage==9&&pending&&!pending->open){
            assert(completeHttp().find("200 OK")!=std::string::npos);
            dashboard.requestStop();stage=10;
        }
    });
    // L'AP n'est ferme qu'a l'arret du concentrateur (requestStop), pas apres
    // une connexion domestique reussie : verifie a l'etape 7.
    assert(stage==10&&dashboard.stopped());
    puts("PASS AP permanent : racine = tableau de bord, /setup optionnel, portail captif vers /, API et ecriture depuis l'AP, AP maintenu apres connexion domestique, acces LAN conserve");
    return 0;
}
