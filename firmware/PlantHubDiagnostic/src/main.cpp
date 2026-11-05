#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

// Heltec WiFi LoRa 32 V3: ESP32-S3 + SX1262, EU868 project variant.
// UART0 is routed to the onboard CP2102; Serial must not use native USB CDC.
namespace {
constexpr uint8_t kNss = 8, kSck = 9, kMosi = 10, kMiso = 11;
constexpr uint8_t kReset = 12, kBusy = 13, kDio1 = 14;
constexpr uint8_t kOledSda = 17, kOledScl = 18, kOledReset = 21;
constexpr uint8_t kVext = 36;
constexpr uint8_t kPrg = 0, kLed = 35;
constexpr uint32_t kButtonDebounceMs = 40, kLongPressMs = 3000;
constexpr gpio_num_t kHeldPins[] = {
    GPIO_NUM_8, GPIO_NUM_12, GPIO_NUM_21, GPIO_NUM_35, GPIO_NUM_36};
constexpr float kFrequencyMHz = 868.1f;

Adafruit_SSD1306 display(128, 64, &Wire, kOledReset);
SX1262 radio(new Module(kNss, kDio1, kReset, kBusy, SPI,
                       SPISettings(2000000, MSBFIRST, SPI_MODE0)));
volatile bool packetPending = false;
bool displayReady = false;
bool radioReady = false;
int16_t lastError = RADIOLIB_ERR_NONE;
uint32_t received = 0, crcErrors = 0, lastStatusMs = 0, lastAttemptMs = 0;
bool prgRawPressed = false, prgPressed = false, prgArmed = false;
bool sleepRequested = false;
uint32_t prgRawChangedMs = 0, prgPressedMs = 0;

void IRAM_ATTR packetInterrupt() {
  packetPending = true;
}

void showStatus() {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("PLANT STATION"));
  display.println(F("Heltec V3 - test"));
  display.println();
  if (radioReady) {
    display.println(F("RADIO OK / ECOUTE"));
    display.println(F("868.1 MHz / SF7"));
    display.print(F("Paquets: "));
    display.println(received);
    display.println(received ? F("Reception observee") : F("Attente emetteur"));
  } else {
    display.println(F("ERREUR RADIO"));
    display.print(F("Code: "));
    display.println(lastError);
    display.println(F("Voir journal USB"));
  }
  display.display();
}

void failRadio(const char* operation, int16_t code) {
  radioReady = false;
  lastError = code;
  lastAttemptMs = millis();
  Serial.printf("[ERREUR] %s: code=%d\n", operation, code);
  showStatus();
}

void initializeRadio() {
  lastAttemptMs = millis();
  packetPending = false;
  Serial.println(F("[RADIO] Initialisation SX1262, TCXO 1.8 V, SPI 2 MHz..."));
  // Configure the modem only. This diagnostic never calls transmit()/send().
  int16_t code = radio.begin(kFrequencyMHz, 125.0f, 7, 5,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                            10, 8, 1.8f, false);
  if (code != RADIOLIB_ERR_NONE) {
    failRadio("SX1262 begin", code);
    return;
  }
  code = radio.setDio2AsRfSwitch(true);
  if (code != RADIOLIB_ERR_NONE) {
    failRadio("Commutateur RF DIO2", code);
    return;
  }
  code = radio.setCRC(2);
  if (code != RADIOLIB_ERR_NONE) {
    failRadio("CRC LoRa", code);
    return;
  }
  radio.setDio1Action(packetInterrupt);
  code = radio.startReceive();
  if (code != RADIOLIB_ERR_NONE) {
    failRadio("Demarrage reception", code);
    return;
  }
  lastError = RADIOLIB_ERR_NONE;
  radioReady = true;
  Serial.println(F("[OK] SX1262 initialise ; reception continue active."));
  Serial.println(F("[CONFIG] 868100000 Hz / BW125 kHz / SF7 / CR4/5 / sync 0x12 / CRC / preambule 8"));
  Serial.println(F("[INFO] Aucune emission. Sans emetteur compatible, zero paquet est normal."));
  showStatus();
}

void readPacket() {
  packetPending = false;
  uint8_t buffer[255];
  const size_t length = radio.getPacketLength();
  if (length > sizeof(buffer)) {
    failRadio("Longueur paquet invalide", RADIOLIB_ERR_PACKET_TOO_LONG);
    return;
  }
  const int16_t code = radio.readData(buffer, length);
  if (code == RADIOLIB_ERR_NONE) {
    ++received;
    Serial.printf("[RX] paquet=%lu octets=%u RSSI=%.1f dBm SNR=%.2f dB hex=",
                  static_cast<unsigned long>(received),
                  static_cast<unsigned>(length), radio.getRSSI(), radio.getSNR());
    for (size_t i = 0; i < length; ++i) Serial.printf("%02X", buffer[i]);
    Serial.println();
    // Raw radio frames are observations, not authenticated plant measurements.
  } else if (code == RADIOLIB_ERR_CRC_MISMATCH) {
    ++crcErrors;
    Serial.println(F("[RX] CRC incorrect : paquet ignore."));
  } else {
    Serial.printf("[RX] Lecture echouee : code=%d\n", code);
  }
  const int16_t restartCode = radio.startReceive();
  if (restartCode != RADIOLIB_ERR_NONE) {
    failRadio("Reprise reception", restartCode);
    return;
  }
  showStatus();
}

void enterDeepSleep() {
  Serial.println(F("[VEILLE] Arret de la reception et extinction de l'ecran."));
  const int16_t sleepCode = radio.sleep(false);
  // If radio communication has failed, hold its hardware reset asserted
  // rather than leaving a possibly active receiver while the MCU sleeps.
  if (sleepCode != RADIOLIB_ERR_NONE) {
    Serial.printf("[VEILLE] Sommeil SX1262 code=%d ; RESET radio maintenu bas.\n",
                  sleepCode);
  } else {
    Serial.println(F("[VEILLE] SX1262 en sommeil (code=0)."));
  }
  radio.clearDio1Action();
  radioReady = false;
  packetPending = false;
  if (displayReady) display.ssd1306_command(SSD1306_DISPLAYOFF);
  Wire.end();
  pinMode(kOledSda, INPUT);
  pinMode(kOledScl, INPUT);
  SPI.end();

  pinMode(kNss, OUTPUT);
  digitalWrite(kNss, HIGH);
  pinMode(kReset, OUTPUT);
  digitalWrite(kReset, sleepCode == RADIOLIB_ERR_NONE ? HIGH : LOW);
  pinMode(kOledReset, OUTPUT);
  digitalWrite(kOledReset, LOW);
  pinMode(kLed, OUTPUT);
  digitalWrite(kLed, LOW);
  digitalWrite(kVext, HIGH);  // OLED supply off, including during deep sleep.
  for (const gpio_num_t pin : kHeldPins) gpio_hold_en(pin);
  gpio_deep_sleep_hold_en();

  // Remain asleep until hardware reset (RST) or a power cycle. PRG alone
  // is deliberately not a wake source; no timer or radio wake is enabled.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  Serial.println(F("[VEILLE] Entree en veille profonde ; RST pour redemarrer."));
  Serial.flush();
  esp_deep_sleep_start();
}

void handlePrgButton(uint32_t now) {
  const bool rawPressed = digitalRead(kPrg) == LOW;
  if (rawPressed != prgRawPressed) {
    prgRawPressed = rawPressed;
    prgRawChangedMs = now;
  }
  if (now - prgRawChangedMs < kButtonDebounceMs) return;
  if (prgPressed != prgRawPressed) {
    prgPressed = prgRawPressed;
    if (prgPressed) {
      prgPressedMs = now;
      if (prgArmed) Serial.println(F("[PRG] Appui detecte."));
    } else if (sleepRequested) {
      enterDeepSleep();
    }
  }
  // Require a released button after boot before accepting a long press.
  if (!prgPressed) {
    prgArmed = true;
    return;
  }
  if (prgArmed && !sleepRequested && now - prgPressedMs >= kLongPressMs) {
    sleepRequested = true;
    Serial.println(F("[PRG] Appui long valide ; relacher PRG pour la veille."));
    if (displayReady) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("MISE EN VEILLE"));
      display.println();
      display.println(F("Relache PRG"));
      display.println(F("RST pour reprendre"));
      display.display();
    }
  }
}
}  // namespace

void setup() {
  // Restore normal GPIO control after a previous sleep before initialising
  // the radio and powering the display again.
  gpio_deep_sleep_hold_dis();
  for (const gpio_num_t pin : kHeldPins) gpio_hold_dis(pin);
  pinMode(kPrg, INPUT_PULLUP);
  pinMode(kLed, OUTPUT);
  digitalWrite(kLed, LOW);
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\nPLANT STATION - HELTEC V3 - DIAGNOSTIC RX v2"));
  Serial.println(F("[BOUTONS] PRG 3 secondes puis relacher = veille ; RST = redemarrage."));
  Serial.printf("[CARTE] %s revision=%u flash=%lu octets\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  pinMode(kVext, OUTPUT);
  digitalWrite(kVext, LOW);
  delay(100);
  Wire.begin(kOledSda, kOledScl, 400000);
  Wire.setTimeOut(50);
  // Verify an I2C acknowledgement as begin() alone only checks allocation.
  pinMode(kOledReset, OUTPUT);
  digitalWrite(kOledReset, LOW);
  delay(10);
  digitalWrite(kOledReset, HIGH);
  delay(20);
  Wire.beginTransmission(0x3C);
  displayReady = Wire.endTransmission() == 0;
  if (displayReady) {
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  }
  Serial.println(displayReady ? F("[OK] OLED detecte a 0x3C et initialise.")
                              : F("[INFO] OLED indisponible ; diagnostic sur USB."));
  SPI.begin(kSck, kMiso, kMosi, kNss);
  initializeRadio();
}

void loop() {
  handlePrgButton(millis());
  if (sleepRequested) {
    delay(2);
    return;
  }
  if (radioReady && packetPending) readPacket();
  const uint32_t now = millis();
  if (!radioReady && now - lastAttemptMs >= 10000) initializeRadio();
  if (now - lastStatusMs >= 5000) {
    lastStatusMs = now;
    Serial.printf("[ETAT] uptime_s=%lu radio_rx=%s paquets=%lu crc_erreurs=%lu code=%d\n",
                  static_cast<unsigned long>(now / 1000), radioReady ? "OK" : "ERREUR",
                  static_cast<unsigned long>(received),
                  static_cast<unsigned long>(crcErrors), lastError);
  }
  delay(2);
}
