#include "PlantDashboard.h"
#if PLANT_DASHBOARD
#include "PlantDashboardApi.h"
#include "PlantDashboardPage.h"
#include "storage/PlantHistory.h"
#include <WiFi.h>
#include <WiFiProv.h>
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
std::atomic<bool> stopping{false}, finished{true}, provisionEnded{false}, credentialFailed{false};
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
ConfigSet config;
PlantView current;
uint64_t clockUtc=0, clockUptime=0;
bool clockKnown=false, provisioning=false, mdns=false, online=false, dirtyNames=false;
bool restartProvision=false, endingProvision=false;
uint32_t connectAt=0, failedAt=0, endedAt=0, stopAt=0, dirtyAt=0;
String hostName, bleName, proof;

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
void provisionEvent(arduino_event_t* event) {
    if(event->event_id==ARDUINO_EVENT_PROV_END) provisionEnded.store(true);
    if(event->event_id==ARDUINO_EVENT_PROV_CRED_FAIL) credentialFailed.store(true);
}
void publishNetwork(const char* message) {
    networkState.online=online;networkState.provisioning=provisioning;
    snprintf(networkState.message,sizeof(networkState.message),"%s",message);
    snprintf(networkState.url,sizeof(networkState.url),"%s",online ? ("http://"+WiFi.localIP().toString()+"/").c_str() : "");
    snprintf(networkState.hostname,sizeof(networkState.hostname),"%s.local",hostName.c_str());
    snprintf(networkState.bluetoothName,sizeof(networkState.bluetoothName),"%s",bleName.c_str());
    snprintf(networkState.proof,sizeof(networkState.proof),"%s",proof.c_str());
    xQueueOverwrite(networks,&networkState);
}
void startProvision() {
    if(provisioning) return;
    WiFi.setAutoReconnect(false); WiFi.disconnect(false,false);
    provisioning=true; provisionEnded.store(false); credentialFailed.store(false);
    publishNetwork("Configurer avec ESP BLE Provisioning");
    // The proof is shown locally on the OLED only, never in API or serial logs.
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE,WIFI_PROV_SCHEME_HANDLER_NONE,
                           WIFI_PROV_SECURITY_1,proof.c_str(),bleName.c_str(),nullptr,nullptr,true);
}
void requestProvision() {
    restartProvision=true;
    if(provisioning && !endingProvision) {endingProvision=true;stopAt=millis();wifi_prov_mgr_stop_provisioning();}
    else if(!provisioning) endedAt=millis();
}
void serviceNetwork(uint32_t now) {
    if(provisionEnded.exchange(false)) {provisioning=false;endingProvision=false;endedAt=now;connectAt=now;WiFi.setAutoReconnect(!restartProvision);}
    if(credentialFailed.exchange(false)) {failedAt=now;publishNetwork("Wi-Fi refuse ; reprise BLE dans 8 s");}
    if(failedAt && uint32_t(now-failedAt)>=8000) {failedAt=0;requestProvision();}
    if(restartProvision && !provisioning && uint32_t(now-endedAt)>=500) {restartProvision=false;startProvision();}
    if(endingProvision && uint32_t(now-stopAt)>=10000) {
        // Do not reboot the receiver or touch the radio when a BLE stack stalls.
        publishNetwork("BLE bloque ; RST pour reconfigurer");
        stopAt=now;
    }
    const bool connected=WiFi.status()==WL_CONNECTED;
    if(connected!=online) {
        online=connected;
        if(online) {
            mdns=MDNS.begin(hostName.c_str());
            if(mdns) {MDNS.addService("http","tcp",80);MDNS.addService("plant","tcp",80);}
            publishNetwork("Connecte au Wi-Fi domestique");
            Serial.printf("[PLANT_WEB] url=%s hostname=%s\n",networkState.url,networkState.hostname);
        } else {if(mdns) MDNS.end();mdns=false;connectAt=now;publishNetwork("Wi-Fi absent ; reconnexion automatique");}
    }
    if(!online && !provisioning && !restartProvision && uint32_t(now-connectAt)>=30000) startProvision();
}
String statusJson() {
    String out;out.reserve(2200);
    out="{\"api_version\":1,\"source\":\"live\",\"mood\":"+quote(moodName(current.mood));
    out+=",\"dry_mask\":"+String(current.dryMask)+",\"warning_mask\":"+String(current.warningMask);
    out+=",\"radio_ready\":"+String(current.radioReady?"true":"false")+",\"uptime_ms\":"+number(current.uptimeMs);
    out+=",\"utc_ms\":"+(clockKnown?number(utcAt(current.uptimeMs)):String("null"));
    out+=",\"lux\":"+(current.luxValid?String(current.lux,1):String("null"));
    out+=",\"wifi_connected\":"+String(online?"true":"false")+",\"url\":"+quote(networkState.url);
    out+=",\"hostname\":"+quote(networkState.hostname)+",\"network_message\":"+quote(networkState.message);
    out+=",\"history_state\":"+quote(PlantHistory::stateName(history.state()))+",\"history_dropped\":"+String(dropped.load());
    out+=",\"history_corrupt\":"+String(history.corruptRecords())+",\"plants\":[";
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
void reply(unsigned code,const char* type,const String& body) {
    response=body;pageResponse=nullptr;sent=0;
    const char* reason=code==200?"OK":code==202?"Accepted":code==400?"Bad Request":code==403?"Forbidden":code==404?"Not Found":code==413?"Payload Too Large":"Service Unavailable";
    header="HTTP/1.1 "+String(code)+" "+reason+"\r\nContent-Type: "+type+"\r\nContent-Length: "+String(response.length())+
        "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; frame-ancestors 'none'\r\n\r\n";
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
void route() {
    const int first=request.indexOf(' '),second=request.indexOf(' ',first+1);
    if(first<0 || second<0 || request.substring(second+1,request.indexOf("\r\n"))!="HTTP/1.1") {error(400,"request");return;}
    const String method=request.substring(0,first),target=request.substring(first+1,second);
    const int query=target.indexOf('?');const String path=query<0?target:target.substring(0,query);
    String host;if(!readHeader("host",host)) {error(400,"host");return;}
    if(method=="GET" && path=="/") {
        reply(200,"text/html; charset=utf-8","");pageResponse=PLANT_DASHBOARD_PAGE;
        header.replace("Content-Length: 0","Content-Length: "+String(strlen(PLANT_DASHBOARD_PAGE)));return;
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
            const uint64_t utc=r.utcMs?r.utcMs:r.boot==bootId?utcAt(r.uptimeMs):0;
            out+="{\"utc_ms\":"+(utc?number(utc):String("null"))+",\"boot\":"+String(r.boot)+",\"uptime_ms\":"+number(r.uptimeMs);
            out+=",\"microvolts\":"+(r.status==0?String(plantsoil::microvolts(r.raw)):String("null"))+",\"adc_status\":"+quote(plantsoil::statusName(static_cast<plantsoil::AdcStatus>(r.status)))+"}";
        }
        reply(200,"application/json; charset=utf-8",out+"]}");return;
    }
    if(method=="POST" && query<0 && (path=="/api/time" || path=="/api/plants")) {
        String marker,type,origin;
        if(!readHeader("x-plant-request",marker) || marker!="1" || !readHeader("content-type",type) || type!="application/x-www-form-urlencoded" ||
           (readHeader("origin",origin) && origin!="http://"+host)) {error(403,"request_origin_or_type");return;}
        FormFields fields;if(!parseForm(request.c_str()+headerEnd+4,fields)) {error(400,"form");return;}
        if(path=="/api/time") {
            uint64_t utc;
            if(fields.count!=1 || !parseUnsigned(fields.get("utc_ms"),4102444800000ULL,utc) || utc<1577836800000ULL) {error(400,"utc_ms");return;}
            clockUtc=utc;clockUptime=current.uptimeMs;clockKnown=true;
            reply(200,"application/json","{\"ok\":true}");return;
        }
        uint64_t id;const char* name=fields.get("name");
        if(fields.count!=2 || !parseUnsigned(fields.get("id"),4,id) || !id || !validPlantName(name)) {error(400,"plant_name");return;}
        snprintf(config.plants[id-1].name,sizeof(config.plants[id-1].name),"%s",name);
        xQueueOverwrite(configurations,&config);dirtyNames=true;dirtyAt=millis();
        reply(202,"application/json","{\"ok\":true,\"persistence\":\"pending\"}");return;
    }
    error(404,"not_found");
}
void closeClient() {client.stop();request="";response="";header="";pageResponse=nullptr;sent=0;headerEnd=0;bodyLength=0;}
void serviceHttp(uint32_t now) {
    if(!client) {
        client=server.available();if(!client) return;
        client.setTimeout(20);clientAt=now;request="";header="";response="";pageResponse=nullptr;sent=0;headerEnd=0;bodyLength=0;
    }
    if(uint32_t(now-clientAt)>4000) {closeClient();return;}
    if(header.length()) {
        const size_t total=header.length()+(pageResponse?strlen(pageResponse):response.length());
        const size_t available=client.availableForWrite();if(!available) return;
        const char* data;size_t remain;
        if(sent<header.length()) {data=header.c_str()+sent;remain=header.length()-sent;}
        else {const size_t offset=sent-header.length();data=(pageResponse?pageResponse:response.c_str())+offset;remain=total-sent;}
        const size_t amount=std::min(std::min(remain,available),size_t(512));
        sent+=client.write(reinterpret_cast<const uint8_t*>(data),amount);
        if(sent==total) closeClient();return;
    }
    for(unsigned processed=0;processed<256 && client.available();++processed) {
        const int c=client.read();if(c<=0) {error(400,"request_bytes");return;}request+=char(c);
        if(!headerEnd && request.endsWith("\r\n\r\n")) {
            headerEnd=request.length()-4;String length,encoding;
            if(readHeader("transfer-encoding",encoding)) {error(400,"transfer_encoding");return;}
            uint64_t parsed=0;
            if(readHeader("content-length",length) && !parseUnsigned(length.c_str(),256,parsed)) {error(413,"body_limit");return;}
            bodyLength=parsed;
        }
        if((!headerEnd && request.length()>1024) || request.length()>1284) {error(413,"request_limit");return;}
        if(headerEnd && request.length()>=headerEnd+4+bodyLength) {route();return;}
    }
}
void saveNames() {
    Preferences prefs;
    if(!prefs.begin("plant-names",false)) return;
    bool saved=true;
    for(unsigned i=0;i<4;++i) {char key[8];snprintf(key,sizeof(key),"name%u",i+1);saved &= prefs.putString(key,config.plants[i].name)==strlen(config.plants[i].name);}
    prefs.end();if(saved) dirtyNames=false;
}
void worker(void*) {
    defaultPlants(config.plants);
    Preferences prefs;
    if(prefs.begin("plant-names",true)) {
        for(unsigned i=0;i<4;++i) {char key[8];snprintf(key,sizeof(key),"name%u",i+1);String name=prefs.getString(key,config.plants[i].name);if(validPlantName(name.c_str())) snprintf(config.plants[i].name,49,"%s",name.c_str());}
        prefs.end();
    }
    xQueueOverwrite(configurations,&config);
    flash.partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
    history.begin();Serial.printf("[PLANT_HISTORY] state=%s\n",PlantHistory::stateName(history.state()));
    WiFi.persistent(true);WiFi.mode(WIFI_STA);
    String mac=WiFi.macAddress();mac.replace(":","");mac.toLowerCase();
    hostName="plant-"+mac.substring(6);bleName="PROV_Plant_"+mac.substring(6);
    if(prefs.begin("plant-ble",false)) {
        proof=prefs.getString("pop","");
        if(proof.length()!=12) {char code[13];snprintf(code,sizeof(code),"%08lx%04lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()&65535));proof=code;prefs.putString("pop",proof);}
        prefs.end();
    }
    if(proof.length()!=12) {char code[13];snprintf(code,sizeof(code),"%08lx%04lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()&65535));proof=code;}
    WiFi.setHostname(hostName.c_str());WiFi.setAutoReconnect(true);WiFi.onEvent(provisionEvent);server.begin();connectAt=millis();
    wifi_config_t saved{};
    if(esp_wifi_get_config(WIFI_IF_STA,&saved)==ESP_OK && saved.sta.ssid[0]) {WiFi.begin();publishNetwork("Connexion au Wi-Fi enregistre");}
    else startProvision();
    char command[32]={};unsigned commandLength=0;
    while(!stopping.load()) {
        const uint32_t now=millis();
        xQueuePeek(views,&current,0);
        HistoryRecord record;
        for(unsigned i=0;i<4 && xQueueReceive(records,&record,0)==pdTRUE;++i) {record.utcMs=utcAt(record.uptimeMs);history.append(record);}
        if(dirtyNames && uint32_t(now-dirtyAt)>=1500) {saveNames();dirtyAt=now;}
        for(unsigned i=0;i<32 && Serial.available();++i) {
            const char c=Serial.read();
            if(c=='\n') {command[commandLength]=0;if(!strcmp(command,"wifi-setup")) requestProvision();commandLength=0;}
            else if(c!='\r' && commandLength+1<sizeof(command)) command[commandLength++]=c;
        }
        serviceNetwork(now);serviceHttp(now);vTaskDelay(pdMS_TO_TICKS(2));
    }
    closeClient();server.end();if(mdns) MDNS.end();
    if(provisioning) {
        wifi_prov_mgr_stop_provisioning();
        // Arduino deinitializes the manager in PROV_END; do not race it.
        const uint32_t stopStarted=millis();
        while(!provisionEnded.exchange(false) && uint32_t(millis()-stopStarted)<3000)
            vTaskDelay(pdMS_TO_TICKS(10));
    }
    WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
    HistoryRecord record;while(xQueueReceive(records,&record,0)==pdTRUE) {record.utcMs=utcAt(record.uptimeMs);history.append(record);}
    if(dirtyNames) saveNames();
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
void PlantDashboard::requestStop() {stopping.store(true);}
bool PlantDashboard::stopped() const {return finished.load();}
}
#endif
