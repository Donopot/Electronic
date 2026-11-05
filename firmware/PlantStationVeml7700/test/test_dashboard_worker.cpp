// Compile the real worker's production boundaries in this translation unit.
// Only SDK I/O is replaced: sockets, Wi-Fi events, queues, flash and Preferences.
#include "../src/web/PlantBleSetup.cpp"
#include "../src/web/PlantDashboard.cpp"
#include <assert.h>
#include <stdio.h>
#include <string>
namespace host {uint32_t now=0;}
FakeSerial Serial;FakeEsp ESP;
using namespace plant;
static std::shared_ptr<host::Connection> queueHttp(const std::string& method,const std::string& path,const std::string& body="",bool marker=true,const std::string& hostname="plant-ebf528.local",bool ap=false,const std::string& origin="") {
    auto connection=std::make_shared<host::Connection>();
    connection->local=ap?"192.168.4.1":"192.168.1.10";
    connection->request=method+" "+path+" HTTP/1.1\r\nHost: "+hostname+"\r\n";
    if(method=="POST")connection->request+="Content-Type: application/x-www-form-urlencoded\r\n"+std::string(marker?"X-Plant-Request: 1\r\n":"")+"Content-Length: "+std::to_string(body.size())+"\r\n";
    if(method=="POST")connection->request+="Origin: "+(origin.empty()?"http://"+hostname:origin)+"\r\n";
    connection->request+="\r\n"+body;host::accepted()=connection;host::socketSend().bytes.clear();return connection;
}
static const std::string& completeHttp(){
    const auto& bytes=host::socketSend().bytes;const auto split=bytes.find("\r\n\r\n");assert(split!=std::string::npos);
    const auto lengthAt=bytes.find("Content-Length: ");assert(lengthAt!=std::string::npos);
    const auto length=std::stoul(bytes.substr(lengthAt+16));assert(bytes.size()-split-4==length);return bytes;
}
static std::string requestHttp(const std::string& method,const std::string& path,const std::string& body="",bool marker=true,const std::string& hostname="plant-ebf528.local",bool ap=false,const std::string& origin="") {
    const auto connection=queueHttp(method,path,body,marker,hostname,ap,origin);auto& socket=host::socketSend();
    // 512 octets par envoi reussi : une page de 31 ko tient dans la fenetre de
    // 4000 ticks, tout en gardant un EAGAIN sur trois pour les envois partiels.
    for(unsigned tick=0;connection->open&&tick<40000;++tick){
        socket.result=tick%3?-1:512;socket.error=EAGAIN;
        serviceHttp(host::now++);
    }
    assert(!connection->open);
    return completeHttp();
}
static void seedNetwork(const char* ssid,const char* password) {
    wifi_config_t config{};memcpy(config.sta.ssid,ssid,strlen(ssid));memcpy(config.sta.password,password,strlen(password));
    host::net().ram=host::net().flash=config;loadSavedWifi();assert(setRamStorage());
}
static std::string setupHttp(const std::string& method,const std::string& path,const std::string& body="") {
    return requestHttp(method,path,body,true,"192.168.4.1",true);
}
static size_t occurrences(const std::string& text,const std::string& token){size_t count=0,at=0;while((at=text.find(token,at))!=std::string::npos){++count;at+=token.size();}return count;}
static void temporarySecretCleanupTests(){
    alignas(FormFields) unsigned char memory[sizeof(FormFields)];
    for(const char* input:{"ssid=Home&password=TemporaryPassword","ssid=Home&password=TemporaryPassword&bad=%00"}){
        FormFields* fields=new(memory)FormFields;
        parseForm(input,*fields);assert(strstr(fields->get("password"),"TemporaryPassword"));
        fields->~FormFields();for(unsigned char value:memory)assert(value==0);
    }
    request="POST /api/setup/connect HTTP/1.1\r\n\r\npassword=TemporaryPassword";
    error(400,"setup_fields");assert(request.length()==0);closeClient();
    request="password=IncompleteTemporaryPassword";closeClient();assert(request.length()==0);
    puts("PASS bounded temporary cleanup: FormFields valid/early-failure copies explicitly erased; HTTP reply/error and incomplete-client close erase request");
}
static void bleApFailureWorkerTests(PlantDashboard& dashboard){
    unsigned stage=0;
    host::runTask(100,[&](unsigned tick){
        if(!tick)return;
        if(stage==0){
            assert(!provisioning&&!host::net().apActive&&bleSetup.active());
            assert(host::bleRequest(communication,"plant-config","v=1&id=801&ssid=BleOnly&password=GoodPassword").find("queued")!=std::string::npos);stage=1;
        }else if(stage==1){assert(testingWifi&&bleAttempt==801);host::gotIp("192.168.10.56");stage=2;}
        else if(stage==2){
            assert(!provisioning&&bleSetup.active()&&bleState=="connected"&&host::net().flashWrites==1);
            host::now+=kSetupSuccessOpenMs;stage=3;
        }else if(stage==3){assert(!bleSetup.active()&&!host::bleRunning()&&!networkState.bleSetup);const auto stops=host::bleStops();host::now+=10;assert(stops==1);dashboard.requestStop();stage=4;}
    });
    assert(stage==4&&dashboard.stopped());
    puts("PASS real worker: AP start failure retains BLE; BLE commit succeeds and closes at its own deadline without AP provisioning");
}
static void bleHttpHandoffWorkerTests(PlantDashboard& dashboard){
    unsigned stage=0;std::shared_ptr<host::Connection> connection;
    host::runTask(5000,[&](unsigned tick){
        auto& socket=host::socketSend();socket.result=tick%3?-1:23;socket.error=EAGAIN;
        if(!tick)return;
        if(stage==0){
            assert(provisioning&&bleSetup.active());
            assert(host::bleRequest(communication,"plant-config","v=1&id=901&ssid=Wrong&password=WrongPassword").find("queued")!=std::string::npos);stage=1;
        }else if(stage==1){assert(testingWifi&&bleAttempt==901);host::now+=kWifiAttemptMs;stage=2;}
        else if(stage==2){
            assert(!testingWifi&&bleState=="failed"&&bleAttempt==901);
            connection=queueHttp("POST","/api/setup/connect","ssid=HttpHome&password=HttpPassword",true,"192.168.4.1",true);stage=3;
        }else if(stage==3&&!connection->open){
            assert(completeHttp().find("202 Accepted")!=std::string::npos&&testingWifi&&bleAttempt==0&&bleRejected==0&&bleState=="idle"&&request.length()==0);
            host::gotIp("192.168.10.57");stage=4;
        }else if(stage==4){
            assert(setupState=="connected"&&host::net().flashWrites==1);
            // No BLE poll occurred between the failed attempt and this HTTP commit.
            const auto snapshot=host::bleRequest(communication,"plant-status","1");
            assert(snapshot.find("\"id\":0")!=std::string::npos&&snapshot.find("\"state\":\"connected\"")!=std::string::npos&&snapshot.find("901")==std::string::npos);
            dashboard.requestStop();stage=5;
        }
    });
    assert(stage==5&&dashboard.stopped());
    puts("PASS real worker: failed BLE attempt followed by HTTP commit cannot be attributed to prior BLE id before its next poll");
}
static void assertReadOnlySavedConnection(){
    assert(online&&bleSetup.active()&&bleAttempt==0&&!candidatePending&&!testingWifi);
    const auto snapshot=host::bleRequest(communication,"plant-status","1");
    assert(snapshot.find("\"id\":0")!=std::string::npos&&snapshot.find("\"state\":\"connected\"")!=std::string::npos&&snapshot.find("http://192.168.10.58/")!=std::string::npos);
    assert(bleState=="idle"&&setupState=="idle"&&!networkState.setupSuccess); // Read-only state leaves the form/OLED unchanged.
}
static void savedConnectionRecoveryWorkerTests(PlantDashboard& dashboard){
    unsigned stage=0;int reads=0;
    host::runTask(100,[&](unsigned tick){
        if(!tick)return;
        if(stage==0){assert(host::bleRequest(communication,"plant-config","v=1&id=1001&ssid=SavedHome&password=SavedPassword").find("queued")!=std::string::npos);stage=1;}
        else if(stage==1){assert(testingWifi);host::gotIp("192.168.10.58");stage=2;}
        else if(stage==2){assert(bleAttempt==1001&&bleState=="connected"&&hasSavedWifi);host::now+=kSetupSuccessOpenMs;stage=3;}
        else if(stage==3){assert(!bleSetup.active()&&!provisioning);reads=host::net().configReads;stage=4;}
        else if(stage==4){assert(host::net().configReads==reads);dashboard.requestSetup();stage=5;}
        else if(stage==5){assert(bleSetup.active()&&provisioning&&bleAttempt==0);host::gotIp("192.168.10.58");stage=6;}
        else if(stage==6){assertReadOnlySavedConnection();dashboard.requestStop();stage=7;}
    });
    assert(stage==7&&dashboard.stopped());
    // Simulated reboot: production begin reloads durable SDK credentials; no BLE until PRG.
    host::net().ram=host::net().flash;host::net().status=0;online=mdns=provisioning=false;
    bleAttempt=bleRejected=0;bleState="idle";setupState="idle";stage=0;dashboard.begin();
    std::shared_ptr<host::Connection> connection;
    host::runTask(5000,[&](unsigned tick){
        auto& socket=host::socketSend();socket.result=tick%3?-1:23;socket.error=EAGAIN;
        if(!tick)return;
        if(stage==0){assert(hasSavedWifi&&!bleSetup.active()&&host::ssidOf(savedWifi)=="SavedHome");reads=host::net().configReads;host::gotIp("192.168.10.58");stage=1;}
        else if(stage==1){assert(online&&host::net().configReads==reads);dashboard.requestSetup();stage=2;}
        else if(stage==2){assert(bleSetup.active());host::gotIp("192.168.10.58");stage=3;}
        else if(stage==3){
            assertReadOnlySavedConnection();
            memset(host::net().ram.sta.password,0,64);memcpy(host::net().ram.sta.password,"DifferentPassword",17);host::gotIp("192.168.10.58");stage=4;
        }else if(stage==4){
            auto snapshot=host::bleRequest(communication,"plant-status","1");
            assert(online&&host::ssidOf(host::net().ram)=="SavedHome"&&snapshot.find("\"state\":\"idle\"")!=std::string::npos&&snapshot.find("\"url\":\"\"")!=std::string::npos);
            assert(!memcmp(host::net().flash.sta.password,"SavedPassword",13));
            host::net().ram=savedWifi;memset(host::net().ram.sta.ssid,0,32);memcpy(host::net().ram.sta.ssid,"DifferentHome",13);host::gotIp("192.168.10.58");stage=5;
        }else if(stage==5){
            assert(host::bleRequest(communication,"plant-status","1").find("\"state\":\"idle\"")!=std::string::npos);
            host::net().ram=savedWifi;hasSavedWifi=false;host::gotIp("192.168.10.58");stage=6;
        }else if(stage==6){
            assert(host::bleRequest(communication,"plant-status","1").find("\"state\":\"idle\"")!=std::string::npos);
            hasSavedWifi=true;connection=queueHttp("POST","/api/setup/connect","ssid=SavedHome&password=SavedPassword",true,"192.168.4.1",true);stage=7;
        }else if(stage==7&&!connection->open){
            assert(completeHttp().find("202 Accepted")!=std::string::npos&&testingWifi&&bleAttempt==0);
            assert(host::bleRequest(communication,"plant-status","1").find("\"state\":\"idle\"")!=std::string::npos);
            dashboard.requestStop();stage=8;
        }
    });
    assert(stage==8&&dashboard.stopped());
    puts("PASS real worker saved recovery: five-minute close/PRG reopen and saved reboot produce read-only connected id0; current password/SSID mismatch, unsaved config and active HTTP candidate excluded; no SDK config polling with BLE inactive; form/OLED unchanged");
}
static void bleWorkerLoopTests(PlantDashboard& dashboard){
    unsigned stage=0;std::shared_ptr<host::Connection> connection;
    host::runTask(5000,[&](unsigned tick){
        auto& socket=host::socketSend();socket.result=tick%3?-1:23;socket.error=EAGAIN;
        if(!tick)return;
        if(stage==0){
            assert(bleSetup.active()&&host::bleRunning()&&communication->security);
            assert(communication->version.find("\"sec_ver\":1")!=std::string::npos);
            seedNetwork("OldHome","OldHomePassword");
            assert(host::bleRequest(communication,"plant-config","v=1&id=101&ssid=Wrong&password=WrongPassword",false)=="SDK_AUTH_REFUSED");
            assert(!candidatePending&&!testingWifi&&host::net().flashWrites==0);
            assert(host::bleRequest(communication,"plant-config",std::string(513,'x')).find("invalid")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=0&id=1&ssid=x&password=password").find("invalid")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=1&id=0&ssid=x&password=password").find("invalid")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=1&id=1&ssid=x%00&password=password").find("invalid")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=1&id=101&ssid=Wrong&password=WrongPassword").find("queued")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=1&id=102&ssid=Second&password=OtherPassword").find("busy")!=std::string::npos);
            stage=1;
        }else if(stage==1){
            assert(testingWifi&&bleAttempt==101&&host::ssidOf(host::net().ram)=="Wrong"&&host::net().flashWrites==0);
            assert(host::bleRequest(communication,"plant-status","1").find("\"state\":\"testing\"")!=std::string::npos);
            assert(host::bleRequest(communication,"plant-config","v=1&id=102&ssid=Second&password=OtherPassword").find("queued")!=std::string::npos);
            connection=queueHttp("POST","/api/setup/connect","ssid=HttpCompetitor&password=OtherPassword",true,"192.168.4.1",true);stage=2;
        }else if(stage==2&&!connection->open){
            assert(completeHttp().find("409 Conflict")!=std::string::npos);
            assert(bleAttempt==101&&bleRejected==102&&host::ssidOf(host::net().ram)=="Wrong");
            host::now+=kWifiAttemptMs;stage=3;
        }else if(stage==3){
            assert(!testingWifi&&bleState=="failed"&&host::net().flashWrites==0&&host::ssidOf(host::net().ram)=="OldHome");
            assert(host::bleRequest(communication,"plant-config","v=1&id=103&ssid=NewHome&password=CorrectPassword").find("queued")!=std::string::npos);stage=4;
        }else if(stage==4){
            assert(testingWifi&&bleAttempt==103&&host::net().flashWrites==0);host::gotIp("192.168.10.55");stage=5;
        }else if(stage==5){
            assert(!testingWifi&&bleState=="connected"&&host::net().flashWrites==1&&host::ssidOf(host::net().flash)=="NewHome");
            const auto reply=host::bleRequest(communication,"plant-status","1");
            assert(reply.find("\"id\":103")!=std::string::npos&&reply.find("\"state\":\"connected\"")!=std::string::npos&&reply.find("http://192.168.10.55/")!=std::string::npos);
            assert(reply.find("CorrectPassword")==std::string::npos&&reply.find(setupPassword.c_str())==std::string::npos);
            dashboard.requestStop();stage=6;
        }
    });
    assert(stage==6&&dashboard.stopped()&&!host::bleRunning()&&!bleSetup.active());
    assert(proof[0]==0&&possession.len==0);
    const auto starts=host::bleStarts();assert(bleSetup.start("PLANT-EBF528",setupPassword.c_str()));
    assert(host::bleStarts()==starts+1);bleSetup.stop();
    host::failBleStart()=true;assert(!bleSetup.start("PLANT-EBF528",setupPassword.c_str())&&!bleSetup.active()&&proof[0]==0);host::failBleStart()=false;
    puts("PASS real scheduled worker + production protocomm adapter (SDK authentication simulated): Security1/PoP binding, unauthorized/invalid/oversized refusal, queue full, HTTP/BLE contention, correlation, timeout->old FLASH preserved->correct retry->GOT_IP->commit, no secrets, stop/reopen/failure");
}
static void workerLoopTests(PlantDashboard& dashboard){
    unsigned stage=0;std::shared_ptr<host::Connection> connection;
    host::runTask(5000,[&](unsigned tick){
        auto& socket=host::socketSend();socket.result=tick%3?-1:23;socket.error=EAGAIN;
        if(!tick)return;
        if(stage==0){
            assert(provisioning&&host::net().apActive);
            PlantView view;view.uptimeMs=1000;auto& p=view.plants[0];p.seen=true;p.ageMs=10;p.packet.session=712;p.packet.sequence=3;p.packet.raw=19000;
            dashboard.publish(view);dashboard.publish(view);
            connection=queueHttp("POST","/api/setup/connect","ssid=WorkerHome&password=WorkerPassword",true,"192.168.4.1",true);stage=1;
        }else if(stage==1&&!connection->open){
            assert(completeHttp().find("202 Accepted")!=std::string::npos);
            assert(testingWifi&&host::net().flashWrites==0&&host::ssidOf(host::net().ram)=="WorkerHome");
            host::gotIp("192.168.10.55");stage=2;
        }else if(stage==2){
            assert(setupState=="connected"&&host::net().flashWrites==1&&host::ssidOf(host::net().flash)=="WorkerHome");
            HistoryRecord values[4];uint64_t next;bool more;
            assert(history.page(1,0,0,kHistoryMs,values,4,next,more)==1&&values[0].session==712&&values[0].sequence==3&&values[0].raw==19000&&values[0].uptimeMs==990);
            connection=queueHttp("POST","/api/plants","id=1&name=NomDuWorker");stage=3;
        }else if(stage==3&&!connection->open){
            assert(completeHttp().find("202 Accepted")!=std::string::npos&&dirtyNames);host::now+=1500;stage=4;
        }else if(stage==4){assert(!dirtyNames&&!namesFailed&&host::preferences().count("plant-names/names"));dashboard.requestStop();stage=5;}
    });
    assert(stage==5&&dashboard.stopped()&&!host::net().apActive);
    // Simulated reboot: only SDK durable state survives, and the real worker reloads it.
    host::net().ram=host::net().flash;online=mdns=false;setupState="idle";defaultPlants(config.plants);dashboard.begin();
    host::runTask(10,[&](unsigned tick){if(tick){PlantConfig restored[4];dashboard.configuration(restored);assert(!strcmp(restored[0].name,"NomDuWorker")&&hasSavedWifi&&host::ssidOf(savedWifi)=="WorkerHome");dashboard.requestStop();}});
    assert(dashboard.stopped());
    puts("PASS scheduled production worker: bounded real task iterations, HTTP candidate -> RAM test -> injected GOT_IP -> durable FLASH commit, queued ingestion exactly once, HTTP name save and real worker reload after reboot (no manual serviceNetwork)");
}
static void networkTests() {
    WiFi.onEvent(networkEvent);setupSsid="PLANT-EBF528";hostName="plant-ebf528";
    host::failPreferences()=true;assert(!startSetup()&&!host::net().apActive);host::failPreferences()=false;
    assert(startSetup()&&provisioning&&host::net().apActive&&setupPassword.length()==16);
    const uint8_t dns[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'k','i','t',0,0,1,0,1};
    host::udpQuery().assign(dns,dns+sizeof(dns));const auto polls=host::udpPolls();serviceNetwork(++host::now);
    assert(host::udpPolls()==polls+1&&host::udpAnswers().size()==1);
    host::udpQuery()={0x12};serviceNetwork(++host::now);assert(host::udpAnswers().size()==1);
    host::udpQuery().assign(513,0);serviceNetwork(++host::now);assert(host::udpQuery().empty()&&host::udpAnswers().size()==1);
    const String password=setupPassword;setupPassword="";assert(loadSetupPassword()&&setupPassword==password);
    assert(host::net().ap.ap.authmode==WIFI_AUTH_WPA2_PSK&&host::net().ap.ap.max_connection==2&&host::net().flashWrites==0);
    closeSetup();assert(!provisioning&&!host::net().apActive);
    seedNetwork("OldHome","OldHomePassword");const auto old=host::net().flash;
    host::net().failAp=true;assert(!startSetup()&&!host::net().apActive&&host::net().autoReconnect);host::net().failAp=false;
    online=false;connectAt=0;apRetryAt=0;host::now=31000;serviceNetwork(host::now);
    assert(provisioning&&host::net().autoReconnect&&host::net().reconnects==1&&!memcmp(&old,&host::net().flash,sizeof(old)));
    host::gotIp();serviceNetwork(++host::now);assert(networkState.online);
    host::net().ip="192.168.2.22";serviceNetwork(++host::now);assert(strstr(networkState.url,"192.168.2.22"));
    assert(setupHttp("GET","/").find(PLANT_SETUP_PAGE)!=std::string::npos);
    assert(requestHttp("GET","/generate_204","",true,"connectivitycheck.gstatic.com",true).find("Location: http://192.168.4.1/")!=std::string::npos);
    assert(requestHttp("POST","/api/setup/connect","ssid=x&password=password",true,"192.168.4.1",false).find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("POST","/api/setup/connect","ssid=x&password=password",true,"evil.invalid",true).find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("POST","/api/setup/connect","ssid=x&password=password",true,"192.168.4.1",true,"http://evil.invalid").find("403 Forbidden")!=std::string::npos);
    assert(setupHttp("POST","/api/setup/connect","ssid=home&password=short").find("400 Bad Request")!=std::string::npos);
    host::net().scans={{std::string("Invalid\xC0\xAF"),-20,WIFI_AUTH_OPEN},{"Ma \"box\"\\ <script>&",-40,WIFI_AUTH_WPA2_PSK},{"Bébé 🌿",-70,WIFI_AUTH_OPEN},{std::string("Truncated\xE2\x82"),-80,WIFI_AUTH_OPEN},{std::string("Surrogate\xED\xA0\x80"),-90,WIFI_AUTH_OPEN}};
    assert(setupHttp("GET","/api/setup/networks").find("\"scanning\":true")!=std::string::npos);
    host::net().scanResult=5;serviceNetwork(++host::now);
    const auto scan=setupHttp("GET","/api/setup/networks");
    assert(scan.find("\\\"box\\\"")!=std::string::npos&&scan.find("Bébé 🌿")!=std::string::npos&&scan.find("Invalid")==std::string::npos&&scan.find("Truncated")==std::string::npos&&scan.find("Surrogate")==std::string::npos&&scan.find("\"networks\":[{\"ssid\":")!=std::string::npos&&occurrences(scan,"\"ssid\":")==2);
    assert(setupHttp("POST","/api/setup/connect","ssid=NewHome&password=WrongPassword").find("202 Accepted")!=std::string::npos);
    assert(candidatePending&&!memcmp(&old,&host::net().flash,sizeof(old)));
    serviceNetwork(++host::now);assert(testingWifi&&host::net().storage==WIFI_STORAGE_RAM&&host::net().flashWrites==0);
    host::now+=kWifiAttemptMs;serviceNetwork(host::now);
    assert(!testingWifi&&setupState=="failed"&&host::ssidOf(host::net().ram)=="OldHome"&&!memcmp(&old,&host::net().flash,sizeof(old)));
    host::now+=600000;serviceNetwork(host::now);assert(provisioning); // Never close an unfinished form.
    setupHttp("POST","/api/setup/connect","ssid=Cancelled&password=AnotherPassword");serviceNetwork(++host::now);
    setupHttp("POST","/api/setup/cancel");serviceNetwork(++host::now);
    assert(!testingWifi&&host::ssidOf(host::net().ram)=="OldHome"&&host::net().flashWrites==0);
    setupHttp("POST","/api/setup/connect","ssid=NotSaved&password=AnotherPassword");serviceNetwork(++host::now);
    host::net().failCommit=true;host::gotIp();serviceNetwork(++host::now);host::net().failCommit=false;
    assert(setupState=="failed"&&!memcmp(&old,&host::net().flash,sizeof(old))&&host::net().storage==WIFI_STORAGE_RAM);
    const std::string ssid32(32,'S'),psk64(64,'A');
    setupHttp("POST","/api/setup/connect","ssid="+ssid32+"&password="+psk64);serviceNetwork(++host::now);
    assert(host::net().beginSsid==ssid32&&host::net().beginPassword==psk64&&host::net().flashWrites==0);
    host::net().failRamAfterCommit=true;host::gotIp("192.168.2.99");serviceNetwork(++host::now);
    assert(setupState=="connected"&&host::net().flashWrites==1&&host::ssidOf(host::net().flash)==ssid32&&hasSavedWifi&&host::ssidOf(savedWifi)==ssid32&&provisioning&&!ramStorageReady);
    const int disconnects=host::net().disconnects;
    assert(setupHttp("POST","/api/setup/cancel").find("409 Conflict")!=std::string::npos);serviceNetwork(++host::now);
    assert(host::net().disconnects==disconnects&&setupState=="connected");
    const auto publicStatus=requestHttp("GET","/api/status");
    assert(publicStatus.find(setupPassword.c_str())==std::string::npos&&publicStatus.find(psk64)==std::string::npos);
    for(const auto& line:Serial.lines)assert(line.find(setupPassword.c_str())==std::string::npos&&line.find(psk64)==std::string::npos);
    assert(setupHttp("GET","/api/setup/status").find("http://192.168.2.99/")!=std::string::npos);
    assert(requestHttp("GET","/api/status","",true,"evil.invalid").find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("POST","/api/time","utc_ms=1793700000000",true,"evil.invalid",false,"http://evil.invalid").find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("POST","/api/time","utc_ms=1793700000000",true,"plant-ebf528.local",false,"http://evil.invalid").find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("GET","/api/status","",true,"PLANT-EBF528.LOCAL:80").find("200 OK")!=std::string::npos);
    assert(requestHttp("POST","/api/plants","id=0&name=x",true,"PLANT-EBF528.LOCAL:80",false,"http://plant-ebf528.local").find("400 Bad Request")!=std::string::npos);
    assert(requestHttp("GET","/api/status","",true,"192.168.2.99:80").find("200 OK")!=std::string::npos);
    assert(requestHttp("GET","/api/status","",true,"192.168.1.10").find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("GET","/api/status","",true,"plant-ebf528.local:81").find("403 Forbidden")!=std::string::npos);
    // Explicit PRG setup reopens the form even during the successful handoff grace period.
    closeApRequested=true;closeApAt=successAt=host::now;setupRequested.store(true);serviceNetwork(++host::now);
    assert(provisioning&&setupState=="idle"&&!closeApRequested&&!successAt&&!closeApAt&&host::ssidOf(host::net().flash)==ssid32);
    const int begins=host::net().begins;
    setupHttp("POST","/api/setup/connect","ssid=MustStayTemporary&password=AnotherPassword");serviceNetwork(++host::now);
    assert(setupState=="failed"&&!testingWifi&&host::net().begins==begins&&host::net().flashWrites==1&&host::ssidOf(host::net().flash)==ssid32);
    host::net().failRamAfterCommit=host::net().failRamStorage=false;assert(setRamStorage());
    // A later success/disconnect restores an editable form and cancels both closing deadlines.
    setupHttp("POST","/api/setup/connect","ssid=Recovered&password=AnotherPassword");serviceNetwork(++host::now);host::gotIp("192.168.2.99");serviceNetwork(++host::now);assert(setupState=="connected");
    closeApRequested=true;closeApAt=host::now;WiFi.disconnect();serviceNetwork(++host::now);
    assert(provisioning&&setupState=="idle"&&!successAt&&!closeApAt&&!closeApRequested&&!networkState.setupSuccess);
    host::now+=kSetupSuccessOpenMs;serviceNetwork(host::now);assert(provisioning&&host::net().autoReconnect);
    assert(setupHttp("POST","/api/setup/connect","ssid=RecoveredAgain&password=AnotherPassword").find("202 Accepted")!=std::string::npos);
    serviceNetwork(++host::now);host::gotIp("192.168.2.99");serviceNetwork(++host::now);assert(setupState=="connected");
    setupHttp("POST","/api/setup/close");assert(provisioning);host::now+=2000;serviceNetwork(host::now);assert(!provisioning&&!host::net().apActive);
    assert(startSetup());host::gotIp();serviceNetwork(++host::now);setupState="connected";successAt=host::now;online=true;host::now+=kSetupSuccessOpenMs;serviceNetwork(host::now);assert(!provisioning);
    setupRequested.store(true);serviceNetwork(++host::now);assert(provisioning);closeSetup();
    puts("PASS production setup: protected AP, outage/reopen recovery, late cancel, durable commit despite RAM-selection failure and next-candidate guard, LAN Host/origin restrictions, invalid UTF8 scan filtering, timeout/rollback, SSID32/PSK64, DHCP/log, no secrets, bounded close");
}
static void storageAndHttp(PlantDashboard& dashboard) {
    flash.partition=esp_partition_find_first(1,2,"spiffs");assert(history.begin());
    PlantView view;view.uptimeMs=100000;view.radioReady=true;view.luxValid=true;view.lux=50;
    for(unsigned i=0;i<4;++i){auto& p=view.plants[i];p.seen=true;p.id=i+1;p.ageMs=100+i;p.packet.node=i+1;p.packet.session=123+i;p.packet.sequence=42+i;p.packet.raw=i==1?0:16000+i;p.packet.address=0x48;p.packet.status=i==1?plantsoil::AdcStatus::ReadError:plantsoil::AdcStatus::Ok;}
    dashboard.publish(view);dashboard.publish(view);xQueuePeek(views,&current,0);serviceStorage(host::now);
    for(unsigned id=1;id<=4;++id){HistoryRecord values[4];uint64_t next;bool more;const auto count=history.page(id,0,0,kHistoryMs,values,4,next,more);assert(count==1&&!more);const auto& r=values[0];assert(r.boot==bootId&&r.node==id&&r.session==122+id&&r.sequence==41+id&&r.uptimeMs==100000-(99+id));assert(r.raw==(id==2?0:int(15999+id))&&r.status==(id==2?2:0));}
    ++view.plants[3].packet.sequence;view.plants[3].packet.raw=20000;dashboard.publish(view);dashboard.publish(view);serviceStorage(host::now);
    {HistoryRecord values[4];uint64_t next;bool more;assert(history.page(4,0,0,kHistoryMs,values,4,next,more)==2);assert(values[1].sequence==46&&values[1].raw==20000);}
    auto response=requestHttp("GET","/api/status");assert(response.find("200 OK")!=std::string::npos&&response.find("battery_percent\":null")!=std::string::npos);
    response=requestHttp("GET","/api/history?node=1&days=30&after=0");
    assert(response.find("200 OK")!=std::string::npos&&response.find("\"utc_ms\":null")!=std::string::npos&&response.find("\"microvolts\":2000000,\"adc_status\":\"ok\"")!=std::string::npos);
    response=requestHttp("GET","/api/history?node=2&days=30&after=0");
    assert(response.find("\"utc_ms\":null")!=std::string::npos&&response.find("\"microvolts\":null,\"adc_status\":\"adc_error\"")!=std::string::npos);
    response=requestHttp("GET","/");assert(response.find(PLANT_DASHBOARD_PAGE)!=std::string::npos);
    assert(response.find("data-tab=\"radio\"")!=std::string::npos&&response.find("data-tab=\"system\"")!=std::string::npos);
    {
        // API v2 : details par noeud, radio/systeme, curseur multi-noeuds et
        // persistance NVS des dernieres mesures (relue apres effacement RAM).
        const auto json=statusJson();
        assert(json.indexOf("\"api_version\":2")>=0&&json.indexOf("\"lux_available\":")>=0);
        assert(json.indexOf("\"last_known\":{\"present\":true")>=0&&json.indexOf("\"raw\":16000")>=0&&json.indexOf("\"ads_address\":72")>=0);
        assert(json.indexOf("\"rssi_dbm\":null")>=0&&json.indexOf("\"radio\":null")>=0&&json.indexOf("\"system\":{\"chip\":\"ESP32-S3\"")>=0);
        const auto mixed=requestHttp("GET","/api/history?node=0&days=30&after=0");
        assert(mixed.find("200 OK")!=std::string::npos&&occurrences(mixed,"\"node\":1")>=1&&occurrences(mixed,"\"node\":4")>=1);
        host::now+=61000;serviceStorage(host::now);
        lastKnown=StoredLast{};loadLastKnown();
        assert(lastKnown.nodes[0].node==1&&lastKnown.nodes[0].sequence==42&&lastKnown.nodes[3].node==4&&lastKnown.nodes[3].raw==20000&&lastKnown.nodes[3].sequence==46);
    }
    assert(requestHttp("GET","/api/history?node=5&days=30").find("400 Bad Request")!=std::string::npos);
    assert(requestHttp("POST","/api/plants","id=1&name=Basilic",false).find("403 Forbidden")!=std::string::npos);
    assert(requestHttp("POST","/api/plants","id=1&id=2&name=x").find("400 Bad Request")!=std::string::npos);
    assert(requestHttp("POST","/api/plants",std::string(257,'x')).find("413 Payload Too Large")!=std::string::npos);
    assert(requestHttp("GET","/unknown").find("404 Not Found")!=std::string::npos);
    response=requestHttp("POST","/api/time","utc_ms=1793700000000");assert(response.find("202 Accepted")!=std::string::npos&&!clockKnown&&pendingClock);
    serviceStorage(host::now);assert(clockKnown&&!clockFailed);
    response=requestHttp("GET","/api/history?node=1&days=1&after=0");
    assert(response.find("\"utc_ms\":1793699999900")!=std::string::npos&&response.find("\"microvolts\":2000000")!=std::string::npos);
    response=requestHttp("GET","/api/history?node=2&days=7&after=0");
    assert(response.find("\"utc_ms\":1793699999899")!=std::string::npos&&response.find("\"microvolts\":null")!=std::string::npos);
    for(unsigned i=0;i<97;++i){HistoryRecord r;r.boot=999;r.node=3;r.uptimeMs=70000+i;r.raw=i==96?23456:18000;r.status=i==1?2:0;r.utcMs=i%2?0:clockUtc-1000-i;assert(history.append(r));}
    response=requestHttp("GET","/api/history?node=3&days=30&after=0");
    assert(response.find("\"more\":true")!=std::string::npos&&occurrences(response,"\"uptime_ms\":")==96&&response.find("\"utc_ms\":null")!=std::string::npos&&response.find("\"microvolts\":null")!=std::string::npos&&response.find("\"microvolts\":2250000")!=std::string::npos&&response.find("\"uptime_ms\":70096")==std::string::npos);
    const size_t cursorAt=response.find("\"next\":\"")+8;const std::string cursor=response.substr(cursorAt,response.find('"',cursorAt)-cursorAt);assert(cursor!="0");
    response=requestHttp("GET","/api/history?node=3&days=30&after="+cursor);
    assert(response.find("\"more\":false")!=std::string::npos&&occurrences(response,"\"uptime_ms\":")==2&&response.find("\"uptime_ms\":70096,\"microvolts\":2932000")!=std::string::npos);
    HistoryRecord before[4],after[4];uint64_t next;bool more;
    assert(history.page(1,0,clockUtc,kHistoryMs,before,4,next,more)==1&&before[0].utcMs);
    assert(history.begin());assert(history.page(1,0,clockUtc,kHistoryMs,after,4,next,more)==1&&before[0].utcMs==after[0].utcMs);
    response=requestHttp("POST","/api/plants","id=1&name=Basilic");assert(dirtyNames&&response.find("pending")!=std::string::npos);
    host::now+=1500;serviceStorage(host::now);assert(!dirtyNames&&!namesFailed&&namesSavedRevision==namesRevision);
    defaultPlants(config.plants);loadNames();assert(!strcmp(config.plants[0].name,"Basilic"));
    requestHttp("POST","/api/plants","id=2&name=Menthe");host::failPreferences()=true;host::now+=1500;serviceStorage(host::now);
    assert(dirtyNames&&namesFailed&&statusJson().indexOf("\"names_persistence\":\"failed\"")>=0);
    finishStorage();assert(namesFailed);host::failPreferences()=false;finishStorage();assert(!dirtyNames&&!namesFailed);
    loadNames();assert(!strcmp(config.plants[1].name,"Menthe"));
    clockKnown=false;host::failFlash()=true;requestHttp("POST","/api/time","utc_ms=1793700000100");serviceStorage(host::now);
    assert(!clockKnown&&clockFailed&&!pendingClock);host::failFlash()=false;
    puts("PASS production HTTP/queues/storage/Preferences: complete Content-Length bytes through partial sends+EAGAIN, routing validation, exactly-once ingestion/all fields, durable clock reboot, names pending/failure/save/load/shutdown");
}
int main(int argc,char** argv){temporarySecretCleanupTests();if(argc>1&&!strcmp(argv[1],"ble-ap-failure"))host::net().failAp=true;PlantDashboard dashboard;dashboard.begin();if(argc>1&&!strcmp(argv[1],"saved-connection-recovery")){savedConnectionRecoveryWorkerTests(dashboard);return 0;}if(argc>1&&!strcmp(argv[1],"ble-ap-failure")){bleApFailureWorkerTests(dashboard);return 0;}if(argc>1&&!strcmp(argv[1],"ble-http-handoff")){bleHttpHandoffWorkerTests(dashboard);return 0;}if(argc>1&&!strcmp(argv[1],"ble-worker-loop")){bleWorkerLoopTests(dashboard);return 0;}if(argc>1&&!strcmp(argv[1],"worker-loop")){workerLoopTests(dashboard);return 0;}networkTests();storageAndHttp(dashboard);}
