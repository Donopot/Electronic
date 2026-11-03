#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <assert.h>
#define F(value) value
constexpr int INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, LOW = 0, HIGH = 1, HEX = 16;
namespace host {
inline uint64_t& clockUs() { static uint64_t clock = 0; return clock; }
inline bool& connected() { static bool value = true; return value; }
inline bool& stalled() { static bool value = false; return value; }
inline bool& rngReady() { static bool value = true; return value; }
inline std::string& usb() { static std::string value; return value; }
inline void setNow(uint32_t now) { clockUs() = static_cast<uint64_t>(now) * 1000; }
inline std::vector<std::string>& pins() { static std::vector<std::string> value; return value; }
}
struct FakeRng { uint32_t CONFIG = 0, EVENTS_VALRDY = 0, TASKS_START = 0, TASKS_STOP = 0, VALUE = 17; };
inline FakeRng& fakeRng() { static FakeRng value; return value; }
#define NRF_RNG (&fakeRng())
constexpr int RNG_CONFIG_DERCEN_Enabled = 1, RNG_CONFIG_DERCEN_Pos = 0;
inline uint32_t millis() { return static_cast<uint32_t>(host::clockUs() / 1000); }
inline uint32_t micros() { return static_cast<uint32_t>(host::clockUs()); }
inline void yield() {
  host::clockUs() += 1000;
  if (host::rngReady() && NRF_RNG->TASKS_START) { NRF_RNG->EVENTS_VALRDY = 1; NRF_RNG->VALUE++; }
}
inline void delay(unsigned long value) { host::clockUs() += value * 1000ULL; }
inline void delayMicroseconds(unsigned value) { host::clockUs() += value; }
inline void pinMode(int pin, int mode) { host::pins().push_back(std::to_string(pin) + ":" + std::to_string(mode)); }
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; } // bus libre sans peripherique : NACK borne
static const uint32_t g_ADigitalPinMap[] = {0, 0, 0, 0, 0, 0, 43, 44};
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t value) = 0;
  virtual size_t write(const uint8_t* values, size_t length) { for (size_t i=0;i<length;++i) write(values[i]); return length; }
  void print(const char* text) { write(reinterpret_cast<const uint8_t*>(text), strlen(text)); }
  void print(long value, int base = 10) { char out[40]; snprintf(out, sizeof(out), base == HEX ? "%lX" : "%ld", value); print(out); }
  void print(unsigned long value, int base = 10) { char out[40]; snprintf(out, sizeof(out), base == HEX ? "%lX" : "%lu", value); print(out); }
  void print(int value, int base = 10) { print(static_cast<long>(value), base); }
  void print(unsigned value, int base = 10) { print(static_cast<unsigned long>(value), base); }
  void print(float value, int digits) { char out[40]; snprintf(out, sizeof(out), "%.*f", digits, static_cast<double>(value)); print(out); }
  void println() { print("\r\n"); }
  template<class T> void println(T value) { print(value); println(); }
  template<class T> void println(T value, int base) { print(value, base); println(); }
};
struct FakeSerial : Print {
  void begin(unsigned long value) { assert(value == 115200); }
  explicit operator bool() const { return host::connected(); }
  size_t write(uint8_t) override { assert(false && "Serial.write interdit pour le profil TX"); return 0; }
};
extern FakeSerial Serial;
