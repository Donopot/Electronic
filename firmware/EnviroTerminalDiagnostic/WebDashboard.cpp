#include "WebDashboard.h"
#if ENVIRO_ENABLE_WEB
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <atomic>
#include "Sx127xDiagnostics.h"
#include "WebDashboardPage.h"

#if !CONFIG_BLUEDROID_ENABLED
#error "La configuration Bluetooth requiert Bluedroid sur ESP32-C3"
#endif

namespace {
WebServer server(80);
WebRadioDiagnosticProvider radioDiagnosticProvider = nullptr;
bmp180::Measurement sample{false, 0, 0};
uint32_t sampleMs = 0, sampleCount = 0, failedSamples = 0;
int lastRadioRssi = 0;
float lastRadioSnr = 0;
uint32_t connectMs = 0, disconnectedMs = 0, failedProvisionMs = 0, buttonMs = 0;
bool connected = false, mdnsActive = false, provisioning = false;
bool resetFailedProvision = false, buttonHandled = false;
bool restartProvision = false, stoppingProvision = false;
bool forceProvisionOnBoot = false;
uint32_t restartProvisionMs = 0, stopProvisionMs = 0;
String deviceId, hostName, bluetoothName, proofOfPossession, wifiMessage;
std::atomic<bool> provisionEnded{false}, credentialFailure{false};
std::atomic<int> failureReason{0};
constexpr int SETUP_BUTTON = 9;  // Bouton BOOT integre, utilise apres demarrage.

String randomHex() {
  char value[17];
  snprintf(value, sizeof(value), "%08lx%08lx", static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  return String(value);
}

String jsonString(const String &value) {
  String out = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = value[i];
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 0x20) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
    else out += char(c);
  }
  return out + '"';
}

void provisionEvent(arduino_event_t *event) {
  switch (event->event_id) {
    case ARDUINO_EVENT_PROV_START:
      Serial.println("[BLE] Configuration disponible dans ESP BLE Provisioning."); break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
      Serial.println("[BLE] Identifiants Wi-Fi recus (masques)."); break;
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      failureReason.store(event->event_info.prov_fail_reason);
      credentialFailure.store(true); break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("[BLE] Configuration Wi-Fi reussie."); break;
    case ARDUINO_EVENT_PROV_END:
      // Arduino a deja deinitialise le manager avant cet evenement.
      provisionEnded.store(true); break;
    default: break;
  }
}

void startProvisioning() {
  if (provisioning) return;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  provisioning = true;
  provisionEnded.store(false);
  credentialFailure.store(false);
  resetFailedProvision = false;
  wifiMessage = "Configuration Bluetooth en cours dans l’application ESP BLE Provisioning.";
  Serial.printf("[BLE] Appareil=%s ; code=%s\n", bluetoothName.c_str(), proofOfPossession.c_str());
  Serial.printf("[BLE] QR={\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}\n",
                bluetoothName.c_str(), proofOfPossession.c_str());
  // Aucun point d'acces Wi-Fi. Garder la memoire BLE permet de reconfigurer ensuite.
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_NONE,
                          WIFI_PROV_SECURITY_1, proofOfPossession.c_str(),
                          bluetoothName.c_str(), nullptr, nullptr, true);
}

void requestProvisioningRestart() {
  resetFailedProvision = false;
  restartProvision = true;
  if (provisioning) {
    if (stoppingProvision) return;
    stoppingProvision = true;
    stopProvisionMs = millis();
    Serial.println("[BLE] Reinitialisation de la session Bluetooth en cours...");
    // Arret asynchrone : attendre PROV_END et la deinitialisation Arduino
    // avant de recreer les endpoints et la session de securite.
    wifi_prov_mgr_stop_provisioning();
  } else restartProvisionMs = millis();
}

void pollUsbCommands() {
  static char command[32];
  static unsigned length = 0;
  for (unsigned processed = 0; Serial.available() && processed < 64; ++processed) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      command[length] = 0;
      if (!strcmp(command, "wifi-setup")) requestProvisioningRestart();
      else if (!strcmp(command, "wifi-status"))
        Serial.printf("[BLE] Etat : actif=%u, arret=%u, reprise=%u ; Wi-Fi=%u.\n",
                      provisioning, stoppingProvision, restartProvision, WiFi.status() == WL_CONNECTED);
      length = 0;
    } else if (length + 1 < sizeof(command)) command[length++] = c;
    else length = 0;
  }
}

void startDiscovery() {
  mdnsActive = MDNS.begin(hostName.c_str());
  if (!mdnsActive) return;
  MDNS.setInstanceName(("EnviroTerminal " + deviceId.substring(6)).c_str());
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("enviro", "tcp", 80);
  MDNS.addServiceTxt("enviro", "tcp", "id", deviceId.c_str());
  MDNS.addServiceTxt("enviro", "tcp", "api", "1");
  MDNS.addServiceTxt("enviro", "tcp", "path", "/api/device");
  MDNS.addServiceTxt("enviro", "tcp", "project", "environment");
}

void status() {
  const bool online = WiFi.status() == WL_CONNECTED;
  const bool fresh = sample.valid && sampleCount && uint32_t(millis() - sampleMs) < ENVIRO_SAMPLE_MAX_AGE_MS;
  String json;
  json.reserve(800);
  json = "{\"sensor_ok\":" + String(fresh ? "true" : "false");
  json += ",\"temperature_c\":" + (fresh ? String(sample.temperatureDeciC / 10.0, 1) : String("null"));
  json += ",\"pressure_hpa\":" + (fresh ? String(sample.pressurePa / 100.0, 2) : String("null"));
  json += ",\"sample_age_ms\":" + (sampleCount ? String(uint32_t(millis() - sampleMs)) : String("null"));
  json += ",\"sample_count\":" + String(sampleCount);
  json += ",\"sample_max_age_ms\":" + String(ENVIRO_SAMPLE_MAX_AGE_MS);
  json += ",\"measurement_source\":\"" + String(ENVIRO_REMOTE_SENSOR ? "lora" : "local") + "\"";
  json += ",\"radio_rssi_dbm\":" + (ENVIRO_REMOTE_SENSOR && sampleCount ? String(lastRadioRssi) : String("null"));
  json += ",\"radio_snr_db\":" + (ENVIRO_REMOTE_SENSOR && sampleCount ? String(lastRadioSnr, 2) : String("null"));
  json += ",\"failed_samples\":" + String(failedSamples);
  json += ",\"radio_spi_ok\":" + String(sx127xSpiReady() ? "true" : "false");
  json += ",\"radio_tx_enabled\":" + String(ENVIRO_ENABLE_LORA_TX ? "true" : "false");
  json += ",\"wifi_connected\":" + String(online ? "true" : "false");
  json += ",\"ssid\":" + jsonString(online ? WiFi.SSID() : String(""));
  json += ",\"ip\":" + jsonString(online ? WiFi.localIP().toString() : String(""));
  json += ",\"device_id\":" + jsonString(deviceId);
  json += ",\"hostname\":" + jsonString(hostName + ".local");
  json += ",\"wifi_message\":" + jsonString(wifiMessage) + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void deviceDescription() {
  String json = "{\"protocol\":\"enviroterminal\",\"api_version\":1,\"id\":" + jsonString(deviceId);
  json += ",\"name\":\"EnviroTerminal\",\"project\":\"environment\",\"hostname\":" + jsonString(hostName + ".local");
  json += ",\"capabilities\":[\"temperature\",\"pressure\",\"radio_spi\",\"ble_provisioning\"";
  if (ENVIRO_REMOTE_SENSOR) json += ",\"lora_receive\"";
  if (ENVIRO_ENABLE_LORA_TX) json += ",\"lora_transmit\"";
  json += "],";
  json += "\"status_url\":\"/api/status\",\"provisioning\":\"espressif-ble-security1\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void loadIdentity() {
  deviceId = WiFi.macAddress(); deviceId.replace(":", ""); deviceId.toLowerCase();
  hostName = "enviro-" + deviceId.substring(6);
  bluetoothName = "PROV_Enviro_" + deviceId.substring(6);
  Preferences preferences;
  if (preferences.begin("enviro-ble", false)) {
    forceProvisionOnBoot = preferences.getBool("retry-boot", false);
    if (forceProvisionOnBoot) preferences.remove("retry-boot");
    if (preferences.isKey("pop")) proofOfPossession = preferences.getString("pop");
    if (proofOfPossession.length() != 12) {
      proofOfPossession = randomHex().substring(0, 12);
      if (preferences.putString("pop", proofOfPossession) != proofOfPossession.length())
        Serial.println("[ERREUR] Memorisation du code Bluetooth impossible.");
    }
    preferences.end();
  }
  if (!proofOfPossession.length()) proofOfPossession = randomHex().substring(0, 12);
}

bool connectSavedNetwork() {
  wifi_config_t config{};
  if (esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK && config.sta.ssid[0]) {
    WiFi.begin(); return true;
  }
  // Import unique des identifiants de la premiere version du formulaire.
  Preferences legacy;
  if (!legacy.begin("enviro-wifi", false)) return false;
  const bool migrated = legacy.getBool("ble-migrated", false);
  String previousSsid, previousPassword;
  if (!migrated && legacy.isKey("ssid")) {
    previousSsid = legacy.getString("ssid");
    if (legacy.isKey("password")) previousPassword = legacy.getString("password");
  }
  legacy.putBool("ble-migrated", true);
  legacy.end();
  if (!previousSsid.length()) return false;
  WiFi.begin(previousSsid.c_str(), previousPassword.c_str());
  return true;
}
}

void publishWebMeasurement(const bmp180::Measurement &measurement) {
  sample = measurement;
  if (sample.valid) { sampleMs = millis(); ++sampleCount; }
  else ++failedSamples;
}

void publishWebRadioSignal(int rssi, float snr) {
  lastRadioRssi = rssi;
  lastRadioSnr = snr;
}

void setWebRadioDiagnosticProvider(WebRadioDiagnosticProvider provider) {
  radioDiagnosticProvider = provider;
}

void beginWebDashboard() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  loadIdentity();
  WiFi.setHostname(hostName.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(provisionEvent);
  pinMode(SETUP_BUTTON, INPUT_PULLUP);
  server.on("/", HTTP_GET, [] { server.sendHeader("Cache-Control", "no-store"); server.send_P(200, "text/html; charset=utf-8", DASHBOARD_PAGE); });
  server.on("/api/status", HTTP_GET, status);
  server.on("/api/device", HTTP_GET, deviceDescription);
  if (radioDiagnosticProvider) {
    server.on("/api/radio", HTTP_GET, [] {
      server.sendHeader("Cache-Control", "no-store");
      server.send(200, "application/json; charset=utf-8", radioDiagnosticProvider());
    });
  }
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "Page introuvable"); });
  server.begin();
  connectMs = millis();
  if (forceProvisionOnBoot) startProvisioning();
  else if (connectSavedNetwork()) {
    wifiMessage = "Connexion automatique au Wi-Fi enregistré.";
    Serial.println("[WEB] Connexion au Wi-Fi enregistre (identifiants masques).");
  } else startProvisioning();
}

void pollWebDashboard() {
  pollUsbCommands();
  server.handleClient();
  const uint32_t now = millis();
  if (digitalRead(SETUP_BUTTON) == LOW) {
    if (!buttonMs) buttonMs = now;
    if (!buttonHandled && uint32_t(now - buttonMs) >= 3000) {
      buttonHandled = true; requestProvisioningRestart();
    }
  } else { buttonMs = 0; buttonHandled = false; }
  if (provisionEnded.exchange(false)) {
    provisioning = false;
    stoppingProvision = false;
    resetFailedProvision = false;
    WiFi.setAutoReconnect(!restartProvision);
    connectMs = now;
    restartProvisionMs = now;
    Serial.println(restartProvision
        ? "[BLE] Session fermee ; preparation d'une nouvelle connexion."
        : "[BLE] Configuration terminee ; retour au fonctionnement Wi-Fi normal.");
  }
  if (credentialFailure.exchange(false)) {
    failedProvisionMs = now; resetFailedProvision = true;
    Serial.println(failureReason.load() == WIFI_PROV_STA_AUTH_ERROR
        ? "[BLE] Mot de passe Wi-Fi refuse ; nouvelle session Bluetooth dans 8 secondes."
        : "[BLE] Reseau Wi-Fi introuvable ; nouvelle session Bluetooth dans 8 secondes.");
  }
  if (resetFailedProvision && provisioning && uint32_t(now - failedProvisionMs) > 8000) {
    requestProvisioningRestart();
  }
  if (restartProvision && !provisioning && uint32_t(now - restartProvisionMs) >= 500) {
    restartProvision = false;
    startProvisioning();
  }
  if (stoppingProvision && uint32_t(now - stopProvisionMs) > 10000) {
    // Dernier recours si le transport refuse de terminer son arret.
    // Ne redemarrer qu'apres avoir memorise une reprise immediate.
    Preferences recovery;
    if (recovery.begin("enviro-ble", false)) {
      const bool saved = recovery.putBool("retry-boot", true) > 0;
      recovery.end();
      if (saved) { Serial.println("[BLE] Arret bloque ; redemarrage automatique pour recuperer la connexion."); Serial.flush(); ESP.restart(); }
    }
    stopProvisionMs = now;
  }
  const bool online = WiFi.status() == WL_CONNECTED;
  if (online && !connected) {
    wifiMessage = "Connecté à votre box. Votre téléphone reste sur son Wi-Fi habituel.";
    Serial.printf("[WEB] Site : http://%s/ ; http://%s.local/\n", WiFi.localIP().toString().c_str(), hostName.c_str());
    startDiscovery();
  } else if (!online && connected) {
    disconnectedMs = now;
    wifiMessage = "Wi-Fi interrompu ; reconnexion automatique.";
    Serial.println("[WEB] Wi-Fi interrompu ; reconnexion automatique.");
    if (mdnsActive) { MDNS.end(); mdnsActive = false; }
  }
  connected = online;
  if (!online && !provisioning && !restartProvision && uint32_t(now - connectMs) > 30000 &&
      (!disconnectedMs || uint32_t(now - disconnectedMs) > 30000)) startProvisioning();
}
#else
void beginWebDashboard() {}
void pollWebDashboard() {}
void publishWebMeasurement(const bmp180::Measurement &) {}
void publishWebRadioSignal(int, float) {}
void setWebRadioDiagnosticProvider(WebRadioDiagnosticProvider) {}
#endif
