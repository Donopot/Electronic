#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <assert.h>
constexpr int SSD1306_SWITCHCAPVCC = 2, SSD1306_WHITE = 1;
constexpr int SSD1306_DISPLAYOFF = 0xAE;
namespace host {
inline std::vector<std::string>& oledFrames() { static std::vector<std::string> frames; return frames; }
}
class Adafruit_SSD1306 {
public:
    Adafruit_SSD1306(int width, int height, TwoWire* wire, int reset, unsigned long, unsigned long) {
        assert(width == 128 && height == 64 && wire->controller == 1 && reset == -1);
    }
    bool begin(int, int address, bool reset, bool periphBegin) {
        assert(address == 0x3C && !reset && !periphBegin); return true;
    }
    void setTextWrap(bool) {}
    void setTextColor(int) {}
    void setTextSize(int) {}
    void setCursor(int, int) {}
    void clearDisplay() { text_.clear(); }
    void print(const char* text) { text_ += text; text_ += '\n'; }
    void display() { host::oledFrames().push_back(text_); }
    void ssd1306_command(int command) { assert(command == SSD1306_DISPLAYOFF); host::gpioCalls().push_back("oled_off"); }
private:
    std::string text_;
};
