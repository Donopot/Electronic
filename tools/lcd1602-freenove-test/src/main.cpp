#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#ifndef LCD_TEST_SDA
#define LCD_TEST_SDA 3
#endif
#ifndef LCD_TEST_SCL
#define LCD_TEST_SCL 2
#endif

// Test LCD seul, adapte de la sequence d'utilisation publique Freenove :
// https://docs.freenove.com/projects/fnk0079/en/latest/fnk0079/codes/ESP32/ESP32_C/1_LCD1602.html
// Avant essai : SDA/SCL compatibles 3,3 V cote ESP32-C3.
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);
LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C *lcd = nullptr;

bool probe(uint8_t address) {
  Wire.beginTransmission(address);
  const uint8_t status = Wire.endTransmission(true);
  Serial.printf("[LCD TEST] Adresse 0x%02X : %s (code %u).\n", address,
                status == 0 ? "ACK" : status == 2 ? "NACK" :
                status == 5 ? "TIMEOUT" : "ERREUR", status);
  return status == 0;
}

uint8_t scanOtherAddresses() {
  Serial.println("[SCAN LCD] Recherche complete 0x08..0x77 (0x27 et 0x3F deja sondes).");
  unsigned found = 0;
  unsigned errors = 0;
  unsigned consecutiveErrors = 0;
  uint8_t candidate = 0;
  bool aborted = false;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    if (address == 0x27 || address == 0x3F) continue;
    Wire.beginTransmission(address);
    const uint8_t status = Wire.endTransmission(true);
    if (status == 0) {
      ++found;
      consecutiveErrors = 0;
      Serial.printf("[SCAN LCD] ACK a 0x%02X.\n", address);
      if (!candidate && ((address >= 0x20 && address <= 0x27) ||
                         (address >= 0x38 && address <= 0x3F))) candidate = address;
    } else if (status == 2) {
      consecutiveErrors = 0;
    } else {
      ++errors;
      ++consecutiveErrors;
      Serial.printf("[SCAN LCD] Erreur I2C a 0x%02X : code %u.\n", address, status);
      if (consecutiveErrors >= 3) {
        aborted = true;
        break;
      }
    }
    delay(1);
  }
  Serial.printf("[SCAN LCD] %s : %u peripherique(s), %u erreur(s) de bus.\n",
                aborted ? "Incomplet" : "Termine", found, errors);
  return candidate;
}

void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && uint32_t(millis() - started) < 3000) delay(10);
  Serial.printf("\nTest LCD seul : SDA=GPIO%d, SCL=GPIO%d, LiquidCrystal_I2C.\n", LCD_TEST_SDA, LCD_TEST_SCL);
  if (!Wire.begin(LCD_TEST_SDA, LCD_TEST_SCL, 100000)) {
    Serial.println("[ERREUR] Initialisation I2C impossible.");
    return;
  }
  Wire.setTimeOut(50);
  if (probe(0x3F)) lcd = &lcd3f;
  else if (probe(0x27)) lcd = &lcd27;
  if (!lcd) {
    const uint8_t address = scanOtherAddresses();
    if (address) {
      lcd27 = LiquidCrystal_I2C(address, 16, 2);
      lcd = &lcd27;
      Serial.printf("[LCD TEST] Essai de l'adaptateur detecte a 0x%02X.\n", address);
    }
  }
  if (!lcd) {
    Serial.println("[ERREUR] Aucun adaptateur LCD detecte. Initialisation abandonnee.");
    return;
  }
  lcd->init();
  lcd->backlight();
  lcd->setCursor(0, 0);
  lcd->print("Bonjour !       ");
  // La bibliotheque ne retourne pas le resultat des ecritures LCD.
  Serial.println("[LCD TEST] Commandes init(), backlight() et print() envoyees ; verifier l'ecran.");
}

void loop() {
  if (lcd) {
    char line[17];
    snprintf(line, sizeof(line), "Temps:%9lu s", static_cast<unsigned long>(millis() / 1000));
    lcd->setCursor(0, 1);
    lcd->print(line);
  }
  delay(1000);
}
