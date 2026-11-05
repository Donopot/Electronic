#include "SoilRadio.h"
#if PLANT_SOIL_LORA
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace plant {
namespace {
volatile bool pending = false;
void IRAM_ATTR received() { pending = true; }
}
void SoilRadio::fail(const char* operation, int16_t code) {
    ready_ = false;
    lastAttempt_ = millis();
    char line[160];
    snprintf(line, sizeof(line), "[SOIL_RADIO] state=error operation=%s code=%d authenticated=no", operation, code);
    Serial.println(line);
}
void SoilRadio::begin() {
    SPI.begin(9, 11, 10, 8);
    initialize();
}
void SoilRadio::initialize() {
    lastAttempt_ = millis();
    pending = false;
    ready_ = false;
    initialization_.store(Initialization::Running, std::memory_order_release);
    // begin/findChip peut attendre plus de dix secondes : seul ce travailleur
    // possede alors SPI/radio ; la boucle continue lux et PRG sans attendre.
    if (xTaskCreate(initializeTask, "soil-init", 6144, this, 1, nullptr) != pdPASS) {
        initialization_.store(Initialization::Idle, std::memory_order_release);
        fail("task", -1);
    }
}
void SoilRadio::initializeTask(void* context) {
    auto& self = *static_cast<SoilRadio*>(context);
    int16_t code = self.radio_.begin(kSoilFrequencyMhz, kSoilBandwidthKhz, kSoilSpreadingFactor,
                                    kSoilCodingRate, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 0,
                                    kSoilPowerDbm, 1.8f, false);
    if (code == RADIOLIB_ERR_NONE) code = self.radio_.setDio2AsRfSwitch(true);
    if (code == RADIOLIB_ERR_NONE) code = self.radio_.setCRC(2);
    if (code == RADIOLIB_ERR_NONE) {
        self.radio_.setDio1Action(received);
        code = self.radio_.startReceive();
    }
    self.initializationCode_ = code;
    // Aucun acces radio/SPI apres cette publication acquire/release.
    self.initialization_.store(Initialization::Finished, std::memory_order_release);
    vTaskDelete(nullptr);
}
void SoilRadio::service(uint32_t now) {
    readings_.update(now); // acquerir les peremptions meme si l'OLED est absent
    const auto initialization = initialization_.load(std::memory_order_acquire);
    if (initialization == Initialization::Running) return;
    if (initialization == Initialization::Finished) {
        initialization_.store(Initialization::Idle, std::memory_order_release);
        if (initializationCode_ != RADIOLIB_ERR_NONE) { fail("begin", initializationCode_); return; }
        ready_ = true;
        Serial.println("[SOIL_RADIO] state=listening nodes=SOIL-01,SOIL-02,SOIL-03,SOIL-04 authenticated=no frequency=868100000");
    }
    if (!ready_) {
        if (elapsed(now, lastAttempt_, 10000)) initialize();
        return;
    }
    if (!pending) return;
    pending = false;
    uint8_t bytes[255];
    // Figer RX avant toute lecture de longueur, en-tete ou charge utile.
    int16_t code = radio_.standby();
    bool hasCrc = false;
    const char* rejected = "standby";
    size_t length = 0;
    if (code == RADIOLIB_ERR_NONE) {
        length = radio_.getPacketLength();
        rejected = "length";
        if (length > sizeof(bytes)) code = RADIOLIB_ERR_PACKET_TOO_LONG;
    }
    if (code == RADIOLIB_ERR_NONE) {
        code = radio_.getLoRaRxHeaderInfo(nullptr, &hasCrc);
        rejected = "header_crc";
    }
    if (code == RADIOLIB_ERR_NONE && hasCrc) {
        code = radio_.readData(bytes, length);
        rejected = "radio_read";
    }
    if (code == RADIOLIB_ERR_NONE && hasCrc) {
        const auto result = readings_.accept(bytes, length, now);
        char line[240];
        if (result == plantsoil::Result::Accepted) {
            // RSSI/SNR sont mesures par le SX1262 a la reception : PS v1 ne les
            // transporte pas et ils ne sont jamais ecrits dans le journal.
            const auto rssi = static_cast<int16_t>(lroundf(radio_.getRSSI()));
            const auto snr = static_cast<int16_t>(lroundf(radio_.getSNR()));
            readings_.noteSignal(bytes[3], rssi, snr, now);
            if (plantsoil::formatPacket(line, sizeof(line), "RX", readings_.node(bytes[3]).packet())) Serial.println(line);
            snprintf(line, sizeof(line), "[SOIL_SIGNAL] node=SOIL-%02u rssi_dbm=%d snr_db=%d authenticated=no",
                     static_cast<unsigned>(bytes[3]), static_cast<int>(rssi), static_cast<int>(snr));
            Serial.println(line);
        } else {
            if (result == plantsoil::Result::Duplicate || result == plantsoil::Result::OldSequence)
                snprintf(line, sizeof(line), "[SOIL_REJECT] node=SOIL-%02u reason=%s authenticated=no",
                         static_cast<unsigned>(bytes[3]), plantsoil::resultName(result));
            else snprintf(line, sizeof(line), "[SOIL_REJECT] reason=%s authenticated=no", plantsoil::resultName(result));
            Serial.println(line);
        }
    } else {
        char line[120];
        snprintf(line, sizeof(line), "[SOIL_REJECT] reason=%s code=%d authenticated=no", rejected, code);
        Serial.println(line);
    }
    const int16_t restart = radio_.startReceive();
    if (restart != RADIOLIB_ERR_NONE) fail("restart", restart);
}
bool SoilRadio::sleep() {
    // La boucle attend la fin du proprietaire prive avant de preparer les GPIO.
    if (initializing()) return false;
    radio_.clearDio1Action();
    pending = false;
    const int16_t code = radio_.sleep(false);
    ready_ = false;
    SPI.end();
    char line[120];
    snprintf(line, sizeof(line), "[SOIL_RADIO] state=sleep code=%d reset_fallback=%s", code,
             code == RADIOLIB_ERR_NONE ? "no" : "yes");
    Serial.println(line);
    return code == RADIOLIB_ERR_NONE;
}
}
#endif
