#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <assert.h>
constexpr int SSD1306_SWITCHCAPVCC = 2, SSD1306_WHITE = 1, SSD1306_BLACK = 0;
constexpr int SSD1306_DISPLAYOFF = 0xAE;
namespace host {
inline std::vector<std::string>& oledFrames() { static std::vector<std::string> frames; return frames; }
struct OledDraw { int x, y, size; std::string text; };
inline std::vector<std::vector<OledDraw>>& oledLayouts() { static std::vector<std::vector<OledDraw>> frames; return frames; }
struct OledRect { int x,y,width,height,color; };
inline std::vector<OledRect>& oledRectangles() {static std::vector<OledRect> result;return result;}
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
    void setTextSize(int value) { size_ = value; }
    void setCursor(int x, int y) { x_ = x; y_ = y; }
    void clearDisplay() { text_.clear(); draws_.clear(); }
    void print(const char* text) { text_ += text; text_ += '\n'; draws_.push_back({x_, y_, size_, text}); }
    void display() { host::oledFrames().push_back(text_); host::oledLayouts().push_back(draws_); }
    void ssd1306_command(int command) { assert(command == SSD1306_DISPLAYOFF); host::gpioCalls().push_back("oled_off"); }
    void fillRect(int x,int y,int w,int h,int color) {assert(x>=0&&y>=0&&x+w<=128&&y+h<=64);host::oledRectangles().push_back({x,y,w,h,color});}
    void drawRoundRect(int,int,int,int,int,int) {}
    void drawLine(int,int,int,int,int) {}
    void fillCircle(int,int,int,int) {}
private:
    std::string text_;
    int x_ = 0, y_ = 0, size_ = 1;
    std::vector<host::OledDraw> draws_;
};
