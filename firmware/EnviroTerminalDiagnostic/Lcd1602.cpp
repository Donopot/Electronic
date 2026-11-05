#include "Lcd1602.h"

#if ENVIRO_ENABLE_LCD
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <memory>
#include <new>
#include <stdio.h>
#include <string.h>

namespace {
std::unique_ptr<hd44780_I2Cexp> lcd;
bool initialized = false;
bool attempted = false;
uint32_t lastAttemptMs = 0;
constexpr uint32_t RETRY_INTERVAL_MS = 10000;
constexpr uint8_t LCD_ADDRESS = 0x27;

int findExpander() {
  Serial.printf("[LCD] Test cible a 0x%02X : SDA=GPIO3, SCL=GPIO2.\n", LCD_ADDRESS);
  Wire.beginTransmission(LCD_ADDRESS);
  const uint8_t status = Wire.endTransmission(true);
  if (status == 0) {
    Serial.printf("[OK] LCD : adaptateur I2C present a 0x%02X (ACK).\n", LCD_ADDRESS);
    return LCD_ADDRESS;
  }
  if (status != 2) {
    Serial.printf("[ERREUR] LCD 0x%02X : %s (code %u).\n", LCD_ADDRESS,
                  status == 5 ? "delai I2C depasse" : "erreur I2C", status);
    return -1;
  }
  Serial.printf("[LCD] Aucun ACK a 0x%02X ; recherche des autres adresses 0x20..0x27 / 0x38..0x3F.\n", LCD_ADDRESS);
  for (uint8_t address = 0x20; address <= 0x3F; ++address) {
    if (address == LCD_ADDRESS) continue;
    if (address > 0x27 && address < 0x38) continue;
    Wire.beginTransmission(address);
    const uint8_t probeStatus = Wire.endTransmission(true);
    if (probeStatus == 0) {
      Serial.printf("[OK] LCD : adaptateur detecte a 0x%02X (ACK).\n", address);
      return address;
    }
    if (probeStatus != 2) {
      Serial.printf("[ERREUR] LCD scan 0x%02X : erreur I2C code %u.\n", address, probeStatus);
      return -1;
    }
  }
  Serial.println("[ERREUR] LCD : aucun adaptateur aux adresses 0x20..0x27 / 0x38..0x3F.");
  return -1;
}

bool initializeLcd() {
  const int address = findExpander();
  if (address < 0) return false;
  // Adresse explicite pour rendre les tentatives suivantes independantes du
  // compteur interne d'auto-detection de la bibliotheque.
  lcd.reset(new (std::nothrow) hd44780_I2Cexp(address));
  if (!lcd) {
    Serial.println("[ERREUR] LCD : memoire insuffisante.");
    return false;
  }
  const int status = lcd->begin(16, 2);
  if (status != 0) {
    Serial.printf("[ERREUR] LCD 16x2 : initialisation/autoconfiguration impossible (code %d).\n", status);
    return false;
  }
  if (lcd->backlight() != 0) {
    Serial.println("[ERREUR] LCD : commande du retroeclairage impossible.");
    return false;
  }
  Serial.println("[OK] LCD 16x2 initialise ; temperature et pression toutes les 2 s.");
  return true;
}

bool writeLine(uint8_t row, const char *text) {
  char padded[17];
  memset(padded, ' ', 16);
  const size_t count = strlen(text);
  memcpy(padded, text, count < 16 ? count : 16);
  padded[16] = '\0';
  if (lcd->setCursor(0, row) != 0) return false;
  // Les 16 caracteres ecrasent les anciennes valeurs sans clear()/clignotement.
  return lcd->write(reinterpret_cast<const uint8_t *>(padded), 16) == 16;
}

void render(const bmp180::Measurement &measurement) {
  if (!initialized) {
    attempted = true;
    lastAttemptMs = millis();
    initialized = initializeLcd();
    if (!initialized) {
      Serial.println("[INFO] LCD indisponible ; nouvelle tentative dans 10 s. Lecture BMP180 maintenue.");
      return;
    }
  }
  char firstLine[17];
  char secondLine[17];
  if (measurement.valid) {
    snprintf(firstLine, sizeof(firstLine), "Temp: %5.1f C", measurement.temperatureDeciC / 10.0);
    snprintf(secondLine, sizeof(secondLine), "P: %7.2f hPa", measurement.pressurePa / 100.0);
  } else {
    snprintf(firstLine, sizeof(firstLine), "ERREUR BMP180");
    snprintf(secondLine, sizeof(secondLine), "Mesure invalide");
  }
  if (!writeLine(0, firstLine) || !writeLine(1, secondLine)) {
    initialized = false;
    attempted = true;
    lastAttemptMs = millis();
    Serial.println("[ERREUR] LCD : ecriture echouee ; nouvelle tentative dans 10 s.");
  } else {
    Serial.printf("[LCD] %s | %s\n", firstLine, secondLine);
  }
}
}  // namespace

void updateLcd1602(I2cBusRouter &bus, const bmp180::Measurement &measurement) {
  if (!initialized && attempted &&
      uint32_t(millis() - lastAttemptMs) < RETRY_INTERVAL_MS) return;
  if (bus.select(I2cTarget::Display)) {
    render(measurement);
  } else {
    initialized = false;
    attempted = true;
    lastAttemptMs = millis();
  }
  // Y compris apres un echec d'initialisation ou d'ecriture LCD.
  bus.select(I2cTarget::Sensor);
}
#else
void updateLcd1602(I2cBusRouter &, const bmp180::Measurement &) {}
#endif
