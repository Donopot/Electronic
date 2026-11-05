#pragma once
#include "Arduino.h"
#include <deque>
#include <map>
#include <memory>
#include <functional>
#include <assert.h>
constexpr int ESP_OK=0,ESP_FAIL=-1,WL_CONNECTED=3,WIFI_STA=1,WIFI_OFF=0,WIFI_IF_STA=0,WIFI_IF_AP=1;
constexpr int WIFI_MODE_STA=1,WIFI_MODE_APSTA=3,WIFI_AP_STA=3,WIFI_STORAGE_RAM=1,WIFI_STORAGE_FLASH=0;
constexpr int WIFI_AUTH_OPEN=0,WIFI_AUTH_WPA2_PSK=3,WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
struct arduino_event_t {int event_id;};
constexpr int ARDUINO_EVENT_WIFI_STA_GOT_IP=1,ARDUINO_EVENT_WIFI_STA_DISCONNECTED=2;
struct wifi_config_t {
    struct {uint8_t ssid[32]={},password[64]={};}sta;
    struct {uint8_t ssid[32]={},password[64]={};int ssid_len=0,authmode=0,channel=0,max_connection=0,beacon_interval=0;}ap;
};
namespace host {
struct ScanNetwork {std::string ssid;int rssi,auth;ScanNetwork(std::string s,int r,int a):ssid(s),rssi(r),auth(a){}};
struct Net {
    bool autoReconnect=true,started=true,apActive=false,failAp=false,failStorage=false,failCommit=false,failRamStorage=false,failRamAfterCommit=false;
    int status=0,reconnects=0,begins=0,disconnects=0,storage=WIFI_STORAGE_FLASH,mode=WIFI_MODE_STA,flashWrites=0,ramWrites=0,stops=0,starts=0;
    int scanResult=WIFI_SCAN_RUNNING,scanStarts=0,scanStops=0,configReads=0;
    int stationRssi=-57;unsigned char stationChannel=6;
    std::string ip="192.168.1.10",connectedSsid,beginSsid,beginPassword,apIp="192.168.4.1";
    wifi_config_t ram{},flash{},ap{};std::vector<ScanNetwork> scans;
    void(*event)(arduino_event_t*)=nullptr;
};
inline Net& net(){static Net n;return n;}
inline void event(int id){if(net().event){arduino_event_t e{id};net().event(&e);}}
inline std::string ssidOf(const wifi_config_t& c){size_t n=0;while(n<32&&c.sta.ssid[n])++n;return std::string(reinterpret_cast<const char*>(c.sta.ssid),n);}
inline void gotIp(const char* ip="192.168.1.10"){net().status=WL_CONNECTED;net().ip=ip;net().connectedSsid=ssidOf(net().ram);event(ARDUINO_EVENT_WIFI_STA_GOT_IP);}
struct Connection {std::string request,local="192.168.1.10";size_t at=0;bool open=true;};
inline std::shared_ptr<Connection>& accepted(){static std::shared_ptr<Connection> c;return c;}
inline std::map<std::string,std::vector<uint8_t>>& preferences(){static std::map<std::string,std::vector<uint8_t>> p;return p;}
inline bool& failPreferences(){static bool b=false;return b;}
inline std::vector<uint8_t>& partition(){static std::vector<uint8_t> p(0x180000,255);return p;}
inline bool& failFlash(){static bool b=false;return b;}
inline std::vector<uint8_t>& udpQuery(){static std::vector<uint8_t> p;return p;}
inline std::vector<std::vector<uint8_t>>& udpAnswers(){static std::vector<std::vector<uint8_t>> p;return p;}
inline unsigned& udpPolls(){static unsigned n=0;return n;}
}
struct IPAddress {
    std::string value;
    IPAddress():value("0.0.0.0"){}explicit IPAddress(std::string text):value(text){}
    IPAddress(int a,int b,int c,int d):value(std::to_string(a)+"."+std::to_string(b)+"."+std::to_string(c)+"."+std::to_string(d)){}
    String toString()const{return value;}bool operator==(const IPAddress& b)const{return value==b.value;}
};
inline int esp_wifi_get_config(int which,wifi_config_t* out){++host::net().configReads;*out=which==WIFI_IF_AP?host::net().ap:host::net().ram;return 0;}
inline int esp_wifi_set_storage(int storage){if(host::net().failStorage||(storage==WIFI_STORAGE_RAM&&host::net().failRamStorage))return -1;host::net().storage=storage;return 0;}
inline int esp_wifi_set_config(int which,const wifi_config_t* config){
    auto& n=host::net();
    if(which==WIFI_IF_AP){assert(!n.started);if(n.failAp)return -1;n.ap=*config;return 0;}
    if(n.storage==WIFI_STORAGE_FLASH){if(n.failCommit)return -1;++n.flashWrites;n.flash=*config;if(n.failRamAfterCommit)n.failRamStorage=true;}
    else ++n.ramWrites;
    n.ram=*config;return 0;
}
inline int esp_wifi_stop(){auto& n=host::net();++n.stops;n.started=n.apActive=false;n.status=0;host::event(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);return 0;}
inline int esp_wifi_set_mode(int mode){host::net().mode=mode;if(mode==WIFI_MODE_STA)host::net().apActive=false;return 0;}
inline int esp_wifi_start(){auto& n=host::net();++n.starts;if(n.mode==WIFI_MODE_APSTA){
    // AP ouvert : ssid renseigne et aucun mot de passe ; AP ferme : WPA2 + cle.
    if(n.ap.ap.authmode==WIFI_AUTH_OPEN) assert(n.ap.ap.ssid_len>0&&strlen(reinterpret_cast<const char*>(n.ap.ap.password))==0);
    else assert(n.ap.ap.authmode==WIFI_AUTH_WPA2_PSK&&n.ap.ap.ssid_len>0&&strlen(reinterpret_cast<const char*>(n.ap.ap.password))>=8);
    n.apActive=true;}n.started=true;return 0;}
inline int esp_wifi_scan_stop(){++host::net().scanStops;return 0;}
inline uint32_t esp_random(){static uint32_t n=100;return ++n;}
class WiFiClass {
public:
    void persistent(bool){}bool mode(int mode){host::net().mode=mode;if(mode==WIFI_STA)host::net().apActive=false;if(mode==WIFI_OFF)host::net().started=false;return true;}
    void setHostname(const char*){}void setAutoReconnect(bool v){host::net().autoReconnect=v;}
    void disconnect(bool off=false,bool erase=false){assert(!erase);auto& n=host::net();++n.disconnects;n.status=0;if(off)n.started=false;host::event(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);}
    void begin(){++host::net().begins;}
    void begin(const char* ssid,const char* password){
        auto& n=host::net();assert(strlen(ssid)<=32&&strlen(password)<=64);n.beginSsid=ssid;n.beginPassword=password;++n.begins;
        wifi_config_t config{};memcpy(config.sta.ssid,ssid,strlen(ssid));memcpy(config.sta.password,password,strlen(password));assert(esp_wifi_set_config(WIFI_IF_STA,&config)==0);
    }
    void reconnect(){++host::net().reconnects;}void setMinSecurity(int){}
    void onEvent(void(*f)(arduino_event_t*)){host::net().event=f;}
    int status(){return host::net().status;}IPAddress localIP(){return IPAddress(host::net().ip);}
    IPAddress softAPIP(){return IPAddress(host::net().apIp);}
    bool softAPConfig(IPAddress ip,IPAddress,IPAddress){host::net().apIp=ip.value;return true;}
    String macAddress(){return "3c:0f:02:eb:f5:28";}
    String SSID(){return host::net().connectedSsid;}
    String SSID(int i){return host::net().scans.at(i).ssid;}
    int RSSI(int i){return host::net().scans.at(i).rssi;}int encryptionType(int i){return host::net().scans.at(i).auth;}
    int RSSI(){return host::net().status?host::net().stationRssi:0;}
    unsigned char channel(){return host::net().stationChannel;}
    int scanNetworks(bool async,bool,bool,unsigned){assert(async);++host::net().scanStarts;return WIFI_SCAN_RUNNING;}
    int scanComplete(){return host::net().scanResult;}void scanDelete(){}
};
static WiFiClass WiFi;
class WiFiClient {
    std::shared_ptr<host::Connection> connection;
public:
    WiFiClient(){}explicit WiFiClient(std::shared_ptr<host::Connection> c):connection(c){}
    explicit operator bool()const{return connection&&connection->open;}
    void setTimeout(unsigned){}int fd()const{return *this?7:-1;}
    IPAddress localIP()const{return IPAddress(connection?connection->local:"");}
    int available(){return *this?connection->request.size()-connection->at:0;}
    int read(){return available()?connection->request[connection->at++]:-1;}
    void stop(){if(connection)connection->open=false;connection.reset();}
};
class WiFiServer {
public:explicit WiFiServer(int){}void begin(){}void end(){}
    WiFiClient available(){auto c=host::accepted();host::accepted().reset();return WiFiClient(c);}
};
class WiFiUDP {
    std::vector<uint8_t> outgoing;
public:
    int begin(IPAddress ip,int port){assert(ip.value=="192.168.4.1"&&port==53);return 1;}void stop(){}
    int parsePacket(){++host::udpPolls();return host::udpQuery().size();}
    void flush(){host::udpQuery().clear();}
    int read(uint8_t* out,size_t length){const size_t n=std::min(length,host::udpQuery().size());memcpy(out,host::udpQuery().data(),n);host::udpQuery().clear();return n;}
    IPAddress remoteIP(){return IPAddress(192,168,4,2);}int remotePort(){return 1234;}
    int beginPacket(IPAddress,int){outgoing.clear();return 1;}
    void write(const uint8_t* bytes,size_t n){outgoing.assign(bytes,bytes+n);}
    void endPacket(){host::udpAnswers().push_back(outgoing);}
};
class Preferences {
    std::string prefix;
public:
    bool begin(const char* name,bool){prefix=std::string(name)+"/";return true;}void end(){}
    String getString(const char* key,const char* fallback=""){const auto& p=host::preferences();auto it=p.find(prefix+key);return it==p.end()?String(fallback):String(std::string(it->second.begin(),it->second.end()));}
    size_t putString(const char* key,const String& value){return putBytes(key,value.c_str(),value.length());}
    size_t putBytes(const char* key,const void* bytes,size_t n){if(host::failPreferences())return 0;const auto* p=static_cast<const uint8_t*>(bytes);host::preferences()[prefix+key]=std::vector<uint8_t>(p,p+n);return n;}
    size_t getBytesLength(const char* key){return host::preferences()[prefix+key].size();}
    size_t getBytes(const char* key,void* out,size_t n){const auto& p=host::preferences()[prefix+key];n=std::min(n,p.size());memcpy(out,p.data(),n);return n;}
};
struct Mdns {bool begin(const char*){return true;}void end(){}void addService(const char*,const char*,int){}};
static Mdns MDNS;
struct esp_partition_t {size_t size;};
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2;
inline const esp_partition_t* esp_partition_find_first(int,int,const char*){static esp_partition_t p{0x180000};return &p;}
inline int esp_partition_read(const esp_partition_t*,size_t at,void* out,size_t n){if(at+n>host::partition().size())return -1;memcpy(out,host::partition().data()+at,n);return 0;}
inline int esp_partition_write(const esp_partition_t*,size_t at,const void* in,size_t n){if(host::failFlash())return -1;const auto* p=static_cast<const uint8_t*>(in);for(size_t i=0;i<n;++i)host::partition()[at+i]&=p[i];return 0;}
inline int esp_partition_erase_range(const esp_partition_t*,size_t at,size_t n){if(host::failFlash())return -1;std::fill(host::partition().begin()+at,host::partition().begin()+at+n,255);return 0;}
