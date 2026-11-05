#include "PlantDashboard.h"
#if PLANT_DASHBOARD
#include "PlantDashboardApi.h"
#include "PlantBleSetup.h"
#include "NonBlockingHttp.h"
#include "PlantDashboardPage.h"
#include "PlantSetupPage.h"
#include "CaptiveDns.h"
#include "storage/PlantHistory.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>

namespace plant {
namespace {
struct ConfigSet { PlantConfig plants[4]; };
QueueHandle_t views=nullptr, records=nullptr, configurations=nullptr, networks=nullptr;
std::atomic<bool> stopping{false}, finished{true}, setupRequested{false}, gotIp{false}, stationDisconnected{false};
std::atomic<uint32_t> dropped{0};
uint32_t bootId=0;
class PartitionFlash : public HistoryFlash {
public:
    const esp_partition_t* partition=nullptr;
    size_t size() const override {return partition ? partition->size : 0;}
    bool read(size_t at,void* data,size_t size) override {return partition && esp_partition_read(partition,at,data,size)==ESP_OK;}
    bool write(size_t at,const void* data,size_t size) override {return partition && esp_partition_write(partition,at,data,size)==ESP_OK;}
    bool erase(size_t at,size_t size) override {return partition && esp_partition_erase_range(partition,at,size)==ESP_OK;}
};
PartitionFlash flash;
PlantHistory history(flash);
WiFiServer server(80);
WiFiClient client;
String request,response,header;
const char* pageResponse=nullptr;
size_t sent=0, bodyLength=0, headerEnd=0;
uint32_t clientAt=0;
DashboardNetwork networkState;
PlantBleSetup bleSetup;
uint32_t bleAttempt=0,bleRejected=0;
String bleState="idle";
ConfigSet config;
PlantView current;
uint64_t clockUtc=0, clockUptime=0;
bool clockKnown=false, provisioning=false, mdns=false, online=false, dirtyNames=false;
bool namesFailed=false, clockFailed=false, pendingClock=false;
uint32_t namesRevision=0,namesSavedRevision=0;
uint64_t pendingUtc=0,pendingUptime=0;
uint32_t connectAt=0,dirtyAt=0,apRetryAt=0,successAt=0,closeApAt=0,attemptAt=0,scanAt=0;
String hostName,setupSsid,setupPassword,setupState="idle",setupMessage="Choisissez votre Wi-Fi domestique.",scanJson="[]";
wifi_config_t savedWifi{},candidateWifi{};
bool hasSavedWifi=false,ramStorageReady=false,candidatePending=false,testingWifi=false,cancelWifi=false;
bool needDisconnectEvent=false,scanRunning=false,scanReady=false,closeApRequested=false;
WiFiUDP captiveDns;
constexpr uint32_t kWifiAttemptMs=25000,kSetupSuccessOpenMs=300000;
constexpr char kSetupHost[]="192.168.4.1",kSetupUrl[]="http://192.168.4.1/";

String number(uint64_t n) { char out[24]; snprintf(out,sizeof(out),"%llu",static_cast<unsigned long long>(n)); return String(out); }
String quote(const char* p) {
    String result="\"";
    for(;*p;++p) {unsigned char c=*p; if(c=='"'||c=='\\') {result+='\\';result+=char(c);} else if(c<32) {char escaped[7];snprintf(escaped,sizeof(escaped),"\\u%04x",c);result+=escaped;} else result+=char(c);}
    return result+'"';
}
uint64_t utcAt(uint64_t uptime) {
    if(!clockKnown) return 0;
    if(uptime>=clockUptime) return clockUtc+uptime-clockUptime;
    return clockUtc>=clockUptime-uptime ? clockUtc-(clockUptime-uptime) : 0;
}
void networkEvent(arduino_event_t* event) {
    if(event->event_id==ARDUINO_EVENT_WIFI_STA_GOT_IP)gotIp.store(true);
    if(event->event_id==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)stationDisconnected.store(true);
}
void publishNetwork(const char* message) {
    networkState.online=online;networkState.provisioning=provisioning;networkState.bleSetup=bleSetup.active();networkState.setupSuccess=setupState=="connected";
    snprintf(networkState.message,sizeof(networkState.message),"%s",message);
    snprintf(networkState.url,sizeof(networkState.url),"%s",online?("http://"+WiFi.localIP().toString()+"/").c_str():"");
    snprintf(networkState.hostname,sizeof(networkState.hostname),"%s.local",hostName.c_str());
    snprintf(networkState.setupSsid,sizeof(networkState.setupSsid),"%s",setupSsid.c_str());
    snprintf(networkState.setupPassword,sizeof(networkState.setupPassword),"%s",setupPassword.c_str());
    xQueueOverwrite(networks,&networkState);
}
bool setRamStorage(){ramStorageReady=esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK;return ramStorageReady;}
void loadSavedWifi(){hasSavedWifi=esp_wifi_get_config(WIFI_IF_STA,&savedWifi)==ESP_OK&&savedWifi.sta.ssid[0];}
bool loadSetupPassword(){
    Preferences prefs;if(!prefs.begin("plant-setup",false))return false;
    setupPassword=prefs.getString("password","");
    bool valid=setupPassword.length()==16;
    for(size_t i=0;i<setupPassword.length();++i)valid&=(setupPassword[i]>='0'&&setupPassword[i]<='9')||(setupPassword[i]>='A'&&setupPassword[i]<='F');
    if(!valid){
        char password[17];snprintf(password,sizeof(password),"%08lX%08lX",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
        setupPassword=password;
        valid=prefs.putString("password",setupPassword)==16&&prefs.getString("password","")==setupPassword;
    }
    prefs.end();if(!valid)setupPassword="";return valid;
}
void stopScan(){if(scanRunning)esp_wifi_scan_stop();WiFi.scanDelete();scanRunning=false;}
void resetSetupForm(){
    bleState="idle";bleAttempt=bleRejected=0;setupState="idle";setupMessage="Choisissez votre Wi-Fi domestique (2,4 GHz).";
    closeApAt=successAt=0;closeApRequested=false;
}
bool startSetup(bool explicitRequest=false){
    if(provisioning){
        if(!bleSetup.active()&&setupPassword.length())bleSetup.start(setupSsid.c_str(),setupPassword.c_str());
        if(explicitRequest&&!testingWifi&&!candidatePending){resetSetupForm();publishNetwork("Assistant ouvert ; reseau enregistre conserve");}
        return true;
    }
    if(!setupPassword.length()&&!loadSetupPassword()){publishNetwork("Configuration indisponible : stockage");apRetryAt=millis();return false;}
    bleSetup.start(setupSsid.c_str(),setupPassword.c_str());
    wifi_config_t ap{};memcpy(ap.ap.ssid,setupSsid.c_str(),setupSsid.length());ap.ap.ssid_len=setupSsid.length();
    memcpy(ap.ap.password,setupPassword.c_str(),setupPassword.length());ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
    ap.ap.channel=1;ap.ap.max_connection=2;ap.ap.beacon_interval=100;
    WiFi.setAutoReconnect(false);
    // Configure WPA2 while the radio is stopped; never briefly start a default open AP.
    if(!setRamStorage()||esp_wifi_stop()!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_APSTA)!=ESP_OK||
       esp_wifi_set_config(WIFI_IF_AP,&ap)!=ESP_OK||
       !WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0))||esp_wifi_start()!=ESP_OK){
        esp_wifi_set_mode(WIFI_MODE_STA);esp_wifi_start();WiFi.setAutoReconnect(hasSavedWifi);if(hasSavedWifi)WiFi.begin();
        publishNetwork("Wi-Fi du kit indisponible ; nouvel essai");apRetryAt=millis();return false;
    }
    WiFi.setAutoReconnect(hasSavedWifi);if(hasSavedWifi)WiFi.begin();
    if(!captiveDns.begin(WiFi.softAPIP(),53)){WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(hasSavedWifi);if(hasSavedWifi)WiFi.begin();publishNetwork("Assistant indisponible ; nouvel essai");apRetryAt=millis();return false;}
    provisioning=true;resetSetupForm();scanReady=false;publishNetwork("Scanner le Wi-Fi du kit puis ouvrir l'assistant");return true;
}
void closeSetup(){if(!WiFi.mode(WIFI_STA)){publishNetwork("Fermeture du Wi-Fi du kit impossible ; nouvel essai");return;}captiveDns.stop();stopScan();provisioning=false;bleSetup.stop();closeApRequested=false;publishNetwork(online?"Connecte au Wi-Fi domestique":"Wi-Fi absent ; reconnexion automatique");}
void serviceDns(){
    if(!provisioning)return;const int length=captiveDns.parsePacket();if(length<=0)return;
    if(length>512){captiveDns.flush();return;}
    uint8_t query[512],answer[528];const int received=captiveDns.read(query,length);
    if(received!=length){captiveDns.flush();return;}
    const size_t size=captiveDnsReply(query,length,answer,sizeof(answer));
    if(size&&captiveDns.beginPacket(captiveDns.remoteIP(),captiveDns.remotePort())){captiveDns.write(answer,size);captiveDns.endPacket();}
}
void rollbackCandidate(const char* message){
    testingWifi=candidatePending=false;if(bleAttempt)bleState="failed";gotIp.store(false);WiFi.setAutoReconnect(false);
    if(setRamStorage()){
        WiFi.disconnect(false,false);wifi_config_t restore{};if(hasSavedWifi)restore=savedWifi;
        if(esp_wifi_set_config(WIFI_IF_STA,&restore)==ESP_OK&&hasSavedWifi)WiFi.begin();
    }
    WiFi.setAutoReconnect(hasSavedWifi);memset(&candidateWifi,0,sizeof(candidateWifi));
    setupState="failed";setupMessage=message;publishNetwork("Assistant ouvert ; ancien reseau conserve");
}
void beginCandidate(uint32_t now){
    candidatePending=false;stopScan();
    if(!setRamStorage()){rollbackCandidate("Impossible de preparer une connexion temporaire. Reessayez.");return;}
    WiFi.setAutoReconnect(false);needDisconnectEvent=WiFi.status()==WL_CONNECTED;
    gotIp.store(false);stationDisconnected.store(false);testingWifi=true;attemptAt=now;setupState="testing";
    setupMessage="Connexion en cours (25 secondes maximum)…";
    WiFi.disconnect(false,false);
    WiFi.setMinSecurity(candidateWifi.sta.password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN);
    char ssid[33]={},password[65]={};memcpy(ssid,candidateWifi.sta.ssid,32);memcpy(password,candidateWifi.sta.password,64);
    WiFi.begin(ssid,password);memset(password,0,sizeof(password));
    publishNetwork("Test du Wi-Fi domestique ; radio maintenue");
}
bool commitCandidate(){
    wifi_config_t connected{};
    if(esp_wifi_get_config(WIFI_IF_STA,&connected)!=ESP_OK||memcmp(connected.sta.ssid,candidateWifi.sta.ssid,32))return false;
    // persistent(false) alone only affects driver initialization in this SDK.
    // Set the real SDK storage mode for every boundary: candidate RAM, commit FLASH.
    const bool flashSelected=esp_wifi_set_storage(WIFI_STORAGE_FLASH)==ESP_OK;
    const bool saved=flashSelected&&esp_wifi_set_config(WIFI_IF_STA,&connected)==ESP_OK;
    setRamStorage(); // A failed return to RAM cannot undo a successful durable commit.
    if(saved){savedWifi=connected;hasSavedWifi=true;}
    return saved; // beginCandidate must select RAM again before changing any credentials.
}
bool validUtf8(const String& text){
    const auto* p=reinterpret_cast<const unsigned char*>(text.c_str());const auto* end=p+text.length();
    while(p<end){
        uint32_t cp;unsigned n;
        if(*p<0x80){cp=*p;n=1;}
        else if(*p>=0xC2&&*p<=0xDF){cp=*p&31;n=2;}
        else if(*p>=0xE0&&*p<=0xEF){cp=*p&15;n=3;}
        else if(*p>=0xF0&&*p<=0xF4){cp=*p&7;n=4;}
        else return false;
        if(size_t(end-p)<n)return false;
        for(unsigned i=1;i<n;++i){if((p[i]&0xC0)!=0x80)return false;cp=(cp<<6)|(p[i]&63);}
        if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))return false;
        p+=n;
    }
    return true;
}
void serviceScan(uint32_t now){
    if(!scanRunning)return;const int count=WiFi.scanComplete();
    if(count==WIFI_SCAN_RUNNING&&uint32_t(now-scanAt)<10000)return;
    scanRunning=false;scanReady=true;scanJson="[";
    if(count>=0){
        const int limit=count<16?count:16;
        bool first=true;
        for(int i=0;i<limit;++i){const String ssid=WiFi.SSID(i);if(!validUtf8(ssid))continue;if(!first)scanJson+=',';first=false;scanJson+="{\"ssid\":"+quote(ssid.c_str())+",\"rssi\":"+String(WiFi.RSSI(i))+",\"secured\":"+String(WiFi.encryptionType(i)!=WIFI_AUTH_OPEN?"true":"false")+"}";}
    }else if(count==WIFI_SCAN_RUNNING)esp_wifi_scan_stop();
    scanJson+=']';WiFi.scanDelete();
}
void publishBle(){
    if(!bleSetup.active())return;
    bool savedConnection=false;
    if(!bleAttempt&&!candidatePending&&!testingWifi&&online&&hasSavedWifi&&WiFi.status()==WL_CONNECTED){
        wifi_config_t connected{};
        savedConnection=esp_wifi_get_config(WIFI_IF_STA,&connected)==ESP_OK&&
            !memcmp(connected.sta.ssid,savedWifi.sta.ssid,sizeof(connected.sta.ssid))&&
            !memcmp(connected.sta.password,savedWifi.sta.password,sizeof(connected.sta.password));
        clearSetupSecret(&connected,sizeof(connected));
    }
    const char* state=savedConnection?"connected":bleState.c_str();
    BleSetupSnapshot snapshot;
    const String payload="{\"v\":1,\"id\":"+String(bleAttempt)+",\"rejected_id\":"+String(bleRejected)+",\"state\":"+quote(state)+
        ",\"url\":"+quote(!strcmp(state,"connected")?networkState.url:"")+",\"hostname\":"+quote(networkState.hostname)+"}";
    snprintf(snapshot.json,sizeof(snapshot.json),"%s",payload.c_str());bleSetup.publish(snapshot);
}
void serviceBle(){
    BleSetupCommand command;
    if(!bleSetup.receive(command))return;
    if(testingWifi||candidatePending){bleRejected=command.id;}
    else {
        bleAttempt=command.id;bleRejected=0;bleState="testing";
        memset(&candidateWifi,0,sizeof(candidateWifi));memcpy(candidateWifi.sta.ssid,command.ssid,strlen(command.ssid));
        memcpy(candidateWifi.sta.password,command.password,strlen(command.password));
        candidatePending=true;setupState="testing";setupMessage="Preparation de la connexion…";
    }
    clearSetupSecret(&command,sizeof(command));
}
void serviceNetwork(uint32_t now){
    if(setupRequested.exchange(false))startSetup(true);
    serviceBle();
    if(cancelWifi){cancelWifi=false;if(candidatePending||testingWifi)rollbackCandidate("Essai annule. L'ancien reseau reste enregistre.");}
    if(candidatePending)beginCandidate(now);
    const bool event=gotIp.exchange(false);char expectedSsid[33]={};memcpy(expectedSsid,candidateWifi.sta.ssid,32);
    if(testingWifi&&event&&(!needDisconnectEvent||stationDisconnected.load())&&WiFi.status()==WL_CONNECTED&&WiFi.SSID()==expectedSsid){
        if(commitCandidate()){
            testingWifi=false;if(bleAttempt)bleState="connected";clearSetupSecret(&candidateWifi,sizeof(candidateWifi));WiFi.setAutoReconnect(true);
            setupState="connected";setupMessage="Wi-Fi enregistre. Rejoignez ce reseau pour ouvrir le tableau de bord.";successAt=now;
        }else rollbackCandidate("Connexion obtenue mais sauvegarde impossible. Reessayez avant de redemarrer.");
    }
    if(testingWifi&&uint32_t(now-attemptAt)>=kWifiAttemptMs)rollbackCandidate("Connexion impossible. Verifiez le nom et le mot de passe puis reessayez.");
    serviceScan(now);
    const bool connected=WiFi.status()==WL_CONNECTED;
    if((provisioning||bleSetup.active())&&setupState=="connected"&&!connected)resetSetupForm();
    const String url=connected?"http://"+WiFi.localIP().toString()+"/":String("");
    if(connected!=online||(connected&&url!=networkState.url)||networkState.setupSuccess!=(setupState=="connected")){
        online=connected;
        if(online){if(!mdns){mdns=MDNS.begin(hostName.c_str());if(mdns){MDNS.addService("http","tcp",80);MDNS.addService("plant","tcp",80);}}publishNetwork("Connecte au Wi-Fi domestique");Serial.printf("[PLANT_WEB] url=%s hostname=%s\n",networkState.url,networkState.hostname);}
        else{if(mdns)MDNS.end();mdns=false;connectAt=now;publishNetwork("Wi-Fi absent ; reconnexion automatique");}
    }
    if(!online&&!testingWifi&&!candidatePending&&uint32_t(now-connectAt)>=30000){
        connectAt=now;
        if(!provisioning&&uint32_t(now-apRetryAt)>=30000)startSetup();
        if(hasSavedWifi){WiFi.setAutoReconnect(true);WiFi.reconnect();}
    }
    if((provisioning||bleSetup.active())&&setupState=="connected"&&online&&
       ((closeApRequested&&uint32_t(now-closeApAt)>=2000)||uint32_t(now-successAt)>=kSetupSuccessOpenMs)){
        bleSetup.stop(); // A working BLE setup has its deadline even if AP/DNS failed.
        if(provisioning)closeSetup();else publishNetwork("Connecte au Wi-Fi domestique");
    }
    serviceDns();publishBle();
}
String statusJson() {
    String out;out.reserve(2200);
    out="{\"api_version\":1,\"source\":\"live\",\"mood\":"+quote(moodName(current.mood));
    out+=",\"dry_mask\":"+String(current.dryMask)+",\"warning_mask\":"+String(current.warningMask);
    out+=",\"radio_ready\":"+String(current.radioReady?"true":"false")+",\"uptime_ms\":"+number(current.uptimeMs);
    out+=",\"utc_ms\":"+(clockKnown?number(utcAt(current.uptimeMs)):String("null"));
    out+=",\"lux\":"+(current.luxValid?String(current.lux,1):String("null"));
    out+=",\"wifi_connected\":"+String(online?"true":"false")+",\"url\":"+quote(networkState.url);
    out+=",\"setup_active\":"+String(provisioning?"true":"false");
    out+=",\"hostname\":"+quote(networkState.hostname)+",\"network_message\":"+quote(networkState.message);
    out+=",\"history_state\":"+quote(PlantHistory::stateName(history.state()))+",\"history_dropped\":"+String(dropped.load());
    out+=",\"history_corrupt\":"+String(history.corruptRecords());
    out+=",\"names_persistence\":"+quote(namesFailed?"failed":dirtyNames?"pending":"saved")+",\"names_revision\":"+String(namesRevision)+",\"names_saved_revision\":"+String(namesSavedRevision);
    out+=",\"clock_persistence\":"+quote(clockFailed?"failed":pendingClock?"pending":clockKnown?"saved":"unknown")+",\"plants\":[";
    for(unsigned i=0;i<4;++i) {
        const auto& p=current.plants[i];if(i) out+=',';
        out+="{\"id\":"+String(i+1)+",\"name\":"+quote(p.name)+",\"state\":"+quote(stateName(p.state))+",\"label\":"+quote(stateLabel(p.state));
        out+=",\"age_ms\":"+(p.seen?String(p.ageMs):String("null"));
        out+=",\"microvolts\":"+(p.voltageValid?String(p.microvolts):String("null"));
        out+=",\"battery_percent\":null,\"battery_status\":\"not_reported\"";
        out+=",\"adc_status\":"+(p.seen?quote(plantsoil::statusName(p.packet.status)):String("null"))+"}";
    }
    return out+"]}";
}
void clearHttpRequest(){clearSetupSecret(const_cast<char*>(request.c_str()),request.length());request="";}
void reply(unsigned code,const char* type,const String& body) {
    response=body;pageResponse=nullptr;sent=0;
    const char* reason=code==200?"OK":code==202?"Accepted":code==400?"Bad Request":code==409?"Conflict":code==403?"Forbidden":code==404?"Not Found":code==413?"Payload Too Large":"Service Unavailable";
    header="HTTP/1.1 "+String(code)+" "+reason+"\r\nContent-Type: "+type+"\r\nContent-Length: "+String(response.length())+
        "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; frame-ancestors 'none'\r\n\r\n";
    clearHttpRequest();
}
void error(unsigned code,const char* message) {reply(code,"application/json; charset=utf-8","{\"error\":"+quote(message)+"}");}
bool readHeader(const String& name,String& value) {
    const String sought="\r\n"+name+":";
    String lower=request.substring(0,headerEnd);lower.toLowerCase();
    const int at=lower.indexOf(sought);if(at<0) return false;
    const int start=at+sought.length(),end=request.indexOf("\r\n",start);
    if(end<0 || lower.indexOf(sought,end)>=0) return false;
    value=request.substring(start,end);value.trim();return true;
}
bool apSide(){return provisioning&&client.localIP()==WiFi.softAPIP();}
String normalizedHost(String host){host.toLowerCase();if(host.endsWith(":80"))host=host.substring(0,host.length()-3);return host;}
bool lanHost(const String& host){const String name=normalizedHost(host);return name==WiFi.localIP().toString()||name==hostName+".local";}
bool sameOrigin(const String& origin,const String& host){return origin.indexOf("http://")==0&&normalizedHost(origin.substring(7))==normalizedHost(host);}
bool setupHost(const String& host){return host==kSetupHost||host==String(kSetupHost)+":80";}
void redirectSetup(){reply(302,"text/plain; charset=utf-8","");header="HTTP/1.1 302 Found\r\nLocation: "+String(kSetupUrl)+"\r\nContent-Length: 0\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";}
void sendPage(const char* page){reply(200,"text/html; charset=utf-8","");pageResponse=page;header.replace("Content-Length: 0","Content-Length: "+String(strlen(page)));}
String setupStatus(){
    const uint32_t age=millis()-successAt;
    String out="{\"state\":"+quote(setupState.c_str())+",\"message\":"+quote(setupMessage.c_str());
    out+=",\"ssid\":"+quote(setupSsid.c_str())+",\"ap_active\":"+String(provisioning?"true":"false");
    out+=",\"has_saved_network\":"+String(hasSavedWifi?"true":"false");
    out+=",\"lan_url\":"+quote(setupState=="connected"&&online?networkState.url:"");
    out+=",\"close_in_s\":"+String(setupState=="connected"&&age<kSetupSuccessOpenMs?(kSetupSuccessOpenMs-age)/1000:0)+"}";
    return out;
}
bool setupRoute(const String& method,const String& path,const String& host,int query){
    if(path.indexOf("/api/setup/")!=0)return false;
    if(!apSide()||!setupHost(host)){error(403,"setup_ap_only");return true;}
    if(query>=0){error(400,"setup_parameters");return true;}
    if(method=="GET"&&path=="/api/setup/status"){reply(200,"application/json; charset=utf-8",setupStatus());return true;}
    if(method=="GET"&&path=="/api/setup/networks"){
        if(!testingWifi&&!candidatePending&&!scanRunning&&(!scanReady||uint32_t(millis()-scanAt)>=10000)){
            scanAt=millis();scanReady=false;const int result=WiFi.scanNetworks(true,true,false,120);
            scanRunning=result==WIFI_SCAN_RUNNING||result>=0;
            if(!scanRunning){scanReady=true;scanJson="[]";}
        }
        reply(200,"application/json; charset=utf-8","{\"scanning\":"+String(scanRunning?"true":"false")+",\"networks\":"+scanJson+"}");return true;
    }
    if(method!="POST"){error(404,"not_found");return true;}
    String marker,type,origin;
    if(!readHeader("x-plant-request",marker)||marker!="1"||!readHeader("content-type",type)||type!="application/x-www-form-urlencoded"||
       !readHeader("origin",origin)||!sameOrigin(origin,host)){error(403,"setup_origin_or_type");return true;}
    const char* body=request.c_str()+headerEnd+4;
    if(path=="/api/setup/cancel"){
        if(*body){error(400,"setup_body");return true;}if(!candidatePending&&!testingWifi){error(409,"setup_not_testing");return true;}cancelWifi=true;reply(202,"application/json","{\"ok\":true}");return true;
    }
    if(path=="/api/setup/close"){
        if(*body||setupState!="connected"||!online){error(400,"setup_not_complete");return true;}
        closeApRequested=true;closeApAt=millis();reply(202,"application/json","{\"ok\":true,\"closing_in_s\":2}");return true;
    }
    if(path=="/api/setup/connect"){
        if(testingWifi||candidatePending){error(409,"setup_busy");return true;}
        FormFields fields;
        if(!parseForm(body,fields)||fields.count!=2||!fields.get("ssid")||!fields.get("password")){error(400,"setup_fields");return true;}
        const char* ssid=fields.get("ssid"),*password=fields.get("password");const size_t length=strlen(password);
        bool validPassword=length==0||(length>=8&&length<=63);
        if(length==64){validPassword=true;for(size_t i=0;i<64;++i)validPassword&=hexDigit(password[i])>=0;}
        if(!*ssid||strlen(ssid)>32||!validPassword){error(400,"setup_credentials");return true;}
        memset(&candidateWifi,0,sizeof(candidateWifi));memcpy(candidateWifi.sta.ssid,ssid,strlen(ssid));memcpy(candidateWifi.sta.password,password,length);
        bleAttempt=bleRejected=0;bleState="idle";
        candidatePending=true;setupState="testing";setupMessage="Preparation de la connexion…";
        reply(202,"application/json","{\"ok\":true}");return true;
    }
    error(404,"not_found");return true;
}
void route() {
    const int first=request.indexOf(' '),second=request.indexOf(' ',first+1);
    if(first<0 || second<0 || request.substring(second+1,request.indexOf("\r\n"))!="HTTP/1.1") {error(400,"request");return;}
    const String method=request.substring(0,first),target=request.substring(first+1,second);
    const int query=target.indexOf('?');const String path=query<0?target:target.substring(0,query);
    String host;if(!readHeader("host",host)) {error(400,"host");return;}
    if(setupRoute(method,path,host,query))return;
    if(method=="GET"&&apSide()&&(path=="/generate_204"||path=="/gen_204"||path=="/hotspot-detect.html"||path=="/library/test/success.html"||path=="/ncsi.txt"||path=="/connecttest.txt"||path=="/redirect"||path=="/fwlink"||!setupHost(host))){redirectSetup();return;}
    if(!apSide()&&!lanHost(host)){error(403,"host");return;}
    if(method=="GET"&&(path=="/"||path=="/setup")){
        if(apSide()){if(!setupHost(host)){redirectSetup();return;}sendPage(PLANT_SETUP_PAGE);return;}
        if(path=="/setup"){error(403,"setup_ap_only");return;}
        sendPage(PLANT_DASHBOARD_PAGE);return;
    }
    if(method=="GET" && path=="/api/status" && query<0) {reply(200,"application/json; charset=utf-8",statusJson());return;}
    if(method=="GET" && path=="/api/history") {
        FormFields fields;uint64_t node,days,after=0;
        if(query<0 || !parseForm(target.c_str()+query+1,fields) || fields.count<2 || fields.count>3 ||
           !parseUnsigned(fields.get("node"),4,node) || !node || !parseUnsigned(fields.get("days"),30,days) ||
           (days!=1 && days!=7 && days!=30) || (fields.count==3 && !parseUnsigned(fields.get("after"),UINT64_MAX,after))) {error(400,"history_parameters");return;}
        if(history.state()!=PlantHistory::State::Ready) {error(503,PlantHistory::stateName(history.state()));return;}
        HistoryRecord values[96];uint64_t next;bool more;
        const auto count=history.page(node,after,utcAt(current.uptimeMs),days*24ULL*60*60*1000,values,96,next,more);
        if(history.state()!=PlantHistory::State::Ready) {error(503,PlantHistory::stateName(history.state()));return;}
        String out="{\"node\":"+String(unsigned(node))+",\"days\":"+String(unsigned(days))+",\"next\":"+quote(number(next).c_str())+",\"more\":"+String(more?"true":"false")+",\"records\":[";
        out.reserve(14000);
        for(size_t i=0;i<count;++i) {
            const auto& r=values[i];if(i) out+=',';
            const uint64_t utc=r.utcMs;
            out+="{\"utc_ms\":"+(utc?number(utc):String("null"))+",\"boot\":"+String(r.boot)+",\"uptime_ms\":"+number(r.uptimeMs);
            out+=",\"microvolts\":"+(r.status==0?String(plantsoil::microvolts(r.raw)):String("null"))+",\"adc_status\":"+quote(plantsoil::statusName(static_cast<plantsoil::AdcStatus>(r.status)))+"}";
        }
        reply(200,"application/json; charset=utf-8",out+"]}");return;
    }
    if(method=="POST" && query<0 && (path=="/api/time" || path=="/api/plants")) {
        String marker,type,origin;
        if(!readHeader("x-plant-request",marker) || marker!="1" || !readHeader("content-type",type) || type!="application/x-www-form-urlencoded" ||
           (readHeader("origin",origin) && !sameOrigin(origin,host))) {error(403,"request_origin_or_type");return;}
        FormFields fields;if(!parseForm(request.c_str()+headerEnd+4,fields)) {error(400,"form");return;}
        if(path=="/api/time") {
            uint64_t utc;
            if(fields.count!=1 || !parseUnsigned(fields.get("utc_ms"),4102444800000ULL,utc) || utc<1577836800000ULL) {error(400,"utc_ms");return;}
            pendingUtc=utc;pendingUptime=current.uptimeMs;pendingClock=true;clockFailed=false;
            reply(202,"application/json","{\"ok\":true,\"persistence\":\"pending\"}");return;
        }
        uint64_t id;const char* name=fields.get("name");
        if(fields.count!=2 || !parseUnsigned(fields.get("id"),4,id) || !id || !validPlantName(name)) {error(400,"plant_name");return;}
        snprintf(config.plants[id-1].name,sizeof(config.plants[id-1].name),"%s",name);
        xQueueOverwrite(configurations,&config);dirtyNames=true;namesFailed=false;++namesRevision;dirtyAt=millis();
        reply(202,"application/json","{\"ok\":true,\"persistence\":\"pending\",\"revision\":"+String(namesRevision)+"}");return;
    }
    error(404,"not_found");
}
void closeClient() {client.stop();clearHttpRequest();response="";header="";pageResponse=nullptr;sent=0;headerEnd=0;bodyLength=0;}
void serviceHttp(uint32_t now) {
    if(!client) {
        client=server.available();if(!client) return;
        client.setTimeout(20);clientAt=now;clearHttpRequest();header="";response="";pageResponse=nullptr;sent=0;headerEnd=0;bodyLength=0;
    }
    if(uint32_t(now-clientAt)>4000) {closeClient();return;}
    if(header.length()) {
        const size_t total=header.length()+(pageResponse?strlen(pageResponse):response.length());
        const char* data;size_t remain;
        if(sent<header.length()) {data=header.c_str()+sent;remain=header.length()-sent;}
        else {const size_t offset=sent-header.length();data=(pageResponse?pageResponse:response.c_str())+offset;remain=total-sent;}
        const auto result=sendHttpChunk(client.fd(),data,remain);
        if(result.closed) {closeClient();return;}
        sent+=result.written;
        if(sent==total) closeClient();return;
    }
    for(unsigned processed=0;processed<256 && client.available();++processed) {
        const int c=client.read();if(c<=0) {error(400,"request_bytes");return;}request+=char(c);
        if(!headerEnd && request.endsWith("\r\n\r\n")) {
            headerEnd=request.length()-4;String length,encoding;
            if(readHeader("transfer-encoding",encoding)) {error(400,"transfer_encoding");return;}
            const int firstLine=request.indexOf("\r\n");const bool setup=request.substring(0,firstLine).indexOf(" /api/setup/")>=0;
            if(headerEnd>1024){error(413,"request_limit");return;}
            uint64_t parsed=0;
            if(readHeader("content-length",length)&&!parseUnsigned(length.c_str(),setup?512:256,parsed)){error(413,"body_limit");return;}
            bodyLength=parsed;
        }
        if((!headerEnd && request.length()>1024) || request.length()>1540) {error(413,"request_limit");return;}
        if(headerEnd && request.length()>=headerEnd+4+bodyLength) {route();return;}
    }
}
struct StoredNames {uint32_t version;char names[4][49];uint32_t crc;};
void loadNames() {
    defaultPlants(config.plants);Preferences prefs;
    if(!prefs.begin("plant-names",true))return;
    StoredNames stored{};
    if(prefs.getBytesLength("names")==sizeof(stored)&&prefs.getBytes("names",&stored,sizeof(stored))==sizeof(stored)&&
       stored.version==1&&stored.crc==PlantHistory::crc(reinterpret_cast<const uint8_t*>(&stored),offsetof(StoredNames,crc))){
        for(unsigned i=0;i<4;++i)if(memchr(stored.names[i],0,49)&&validPlantName(stored.names[i]))snprintf(config.plants[i].name,49,"%s",stored.names[i]);
    }
    prefs.end();
}
void saveNames() {
    Preferences prefs;namesFailed=true;
    if(!prefs.begin("plant-names",false))return;
    StoredNames stored{};stored.version=1;
    for(unsigned i=0;i<4;++i)memcpy(stored.names[i],config.plants[i].name,49);
    stored.crc=PlantHistory::crc(reinterpret_cast<const uint8_t*>(&stored),offsetof(StoredNames,crc));
    StoredNames check{};
    const bool saved=prefs.putBytes("names",&stored,sizeof(stored))==sizeof(stored)&&
        prefs.getBytes("names",&check,sizeof(check))==sizeof(check)&&!memcmp(&stored,&check,sizeof(stored));
    prefs.end();
    if(saved){dirtyNames=namesFailed=false;namesSavedRevision=namesRevision;}
}
void serviceStorage(uint32_t now) {
    if(pendingClock){
        pendingClock=false;
        if(history.anchor(bootId,pendingUptime,pendingUtc)){clockUtc=pendingUtc;clockUptime=pendingUptime;clockKnown=true;clockFailed=false;}
        else clockFailed=true;
    }
    HistoryRecord record;
    for(unsigned i=0;i<4&&xQueueReceive(records,&record,0)==pdTRUE;++i){record.utcMs=utcAt(record.uptimeMs);history.append(record);}
    if(dirtyNames&&uint32_t(now-dirtyAt)>=1500){saveNames();dirtyAt=now;}
}
void finishStorage() {
    serviceStorage(millis());HistoryRecord record;
    while(xQueueReceive(records,&record,0)==pdTRUE){record.utcMs=utcAt(record.uptimeMs);history.append(record);}
    if(dirtyNames){saveNames();if(namesFailed)Serial.println("[PLANT_NAMES] persistence=failed");}
}
void worker(void*) {
    loadNames();Preferences prefs;
    xQueueOverwrite(configurations,&config);
    flash.partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
    history.begin();Serial.printf("[PLANT_HISTORY] state=%s\n",PlantHistory::stateName(history.state()));
    WiFi.persistent(true);WiFi.mode(WIFI_STA);loadSavedWifi();setRamStorage();
    String mac=WiFi.macAddress();mac.replace(":","");mac.toLowerCase();hostName="plant-"+mac.substring(6);
    setupSsid="PLANT-"+mac.substring(6);
    char ssid[20];snprintf(ssid,sizeof(ssid),"%s",setupSsid.c_str());for(char* p=ssid;*p;++p)if(*p>='a'&&*p<='z')*p-=32;setupSsid=ssid;
    WiFi.setHostname(hostName.c_str());WiFi.setAutoReconnect(true);WiFi.onEvent(networkEvent);server.begin();connectAt=millis();
    if(hasSavedWifi){WiFi.begin();publishNetwork("Connexion au Wi-Fi enregistre");}else startSetup();
    char command[32]={};unsigned commandLength=0;
    while(!stopping.load()) {
        const uint32_t now=millis();
        xQueuePeek(views,&current,0);
        serviceStorage(now);
        for(unsigned i=0;i<32 && Serial.available();++i) {
            const char c=Serial.read();
            if(c=='\n') {command[commandLength]=0;if(!strcmp(command,"wifi-setup")) setupRequested.store(true);commandLength=0;}
            else if(c!='\r' && commandLength+1<sizeof(command)) command[commandLength++]=c;
        }
        serviceNetwork(now);serviceHttp(now);vTaskDelay(pdMS_TO_TICKS(2));
    }
    bleSetup.stop();closeClient();server.end();if(mdns) MDNS.end();
    if(testingWifi||candidatePending)rollbackCandidate("Configuration suspendue pour la veille.");
    if(provisioning)closeSetup();
    WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
    finishStorage();
    finished.store(true);vTaskDelete(nullptr);
}
}
void PlantDashboard::begin() {
    views=xQueueCreate(1,sizeof(PlantView));records=xQueueCreate(16,sizeof(HistoryRecord));
    configurations=xQueueCreate(1,sizeof(ConfigSet));networks=xQueueCreate(1,sizeof(DashboardNetwork));
    if(!views||!records||!configurations||!networks) {Serial.println("[PLANT_WEB] task=unavailable");return;}
    defaultPlants(config.plants);xQueueOverwrite(configurations,&config);DashboardNetwork initial;xQueueOverwrite(networks,&initial);
    bootId=esp_random();if(!bootId) bootId=1;stopping.store(false);finished.store(false);
    if(xTaskCreatePinnedToCore(worker,"plant-web",12288,nullptr,1,nullptr,0)!=pdPASS) {finished.store(true);Serial.println("[PLANT_WEB] task=unavailable");}
}
void PlantDashboard::publish(const PlantView& view) {
    if(finished.load() || !views) return;
    xQueueOverwrite(views,&view);
    static uint32_t sessions[4]={},sequences[4]={};static bool seen[4]={};
    for(unsigned i=0;i<4;++i) {
        const auto& p=view.plants[i];
        if(!p.seen || (seen[i] && sessions[i]==p.packet.session && sequences[i]==p.packet.sequence)) continue;
        seen[i]=true;sessions[i]=p.packet.session;sequences[i]=p.packet.sequence;
        HistoryRecord record;record.boot=bootId;record.node=i+1;record.session=p.packet.session;record.sequence=p.packet.sequence;
        record.status=static_cast<uint8_t>(p.packet.status);record.raw=p.packet.raw;record.uptimeMs=view.uptimeMs-p.ageMs;
        if(xQueueSend(records,&record,0)!=pdTRUE) ++dropped;
    }
}
void PlantDashboard::configuration(PlantConfig* out) const {
    ConfigSet value;if(configurations && xQueuePeek(configurations,&value,0)==pdTRUE) memcpy(out,value.plants,sizeof(value.plants));
}
DashboardNetwork PlantDashboard::network() const {DashboardNetwork out;if(networks) xQueuePeek(networks,&out,0);return out;}
void PlantDashboard::requestSetup() {setupRequested.store(true);}
void PlantDashboard::requestStop() {stopping.store(true);}
bool PlantDashboard::stopped() const {return finished.load();}
}
#endif
