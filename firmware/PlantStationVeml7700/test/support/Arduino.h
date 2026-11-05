#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>

namespace host {
extern uint32_t now;
inline std::vector<std::string>& gpioCalls() { static std::vector<std::string> calls; return calls; }
inline bool& buttonPressed() { static bool pressed = false; return pressed; }
}
inline uint32_t millis() { return host::now; }
inline void yield() {}
constexpr int INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, LOW = 0, HIGH = 1;
inline int digitalRead(int pin) { if (pin != 0) throw "pin PRG incorrect"; return host::buttonPressed() ? LOW : HIGH; }
inline void pinMode(int pin, int mode) { host::gpioCalls().push_back("mode" + std::to_string(pin) + ":" + std::to_string(mode)); }
inline void digitalWrite(int pin, int value) { host::gpioCalls().push_back("write" + std::to_string(pin) + ":" + std::to_string(value)); }
inline void delay(unsigned long ms) { host::gpioCalls().push_back("delay" + std::to_string(ms)); }
struct FakeSerial {
    bool flushed = false;
    std::vector<std::string> lines;
    std::string input;
    size_t inputAt=0;
    void begin(unsigned long baud) { if (baud != 115200) throw "baud incorrect"; }
    void println(const char* line) { lines.emplace_back(line); }
    void flush() { flushed = true; }
    int available() {return input.size()-inputAt;}
    int read() {return available()?input[inputAt++]:-1;}
    void printf(const char* format,...) {char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);lines.emplace_back(text);}
};
struct FakeEsp {
    const char* getChipModel() { return "ESP32-S3"; }
    unsigned int getChipRevision() { return 2; }
    uint32_t getFlashChipSize() { return 8388608; }
    uint32_t getFreeHeap() { return 182344; }
    uint32_t getMinFreeHeap() { return 151880; }
};
extern FakeSerial Serial;
extern FakeEsp ESP;
