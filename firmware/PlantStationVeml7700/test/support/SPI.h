#pragma once
#include <Arduino.h>
constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings { SPISettings(int, int, int) {} };
struct FakeSpi {
    void begin(int sck, int miso, int mosi, int nss) {
        if (sck != 9 || miso != 11 || mosi != 10 || nss != 8) throw "Broches SPI incorrectes";
        host::gpioCalls().push_back("spi_begin");
    }
    void end() { host::gpioCalls().push_back("spi_end"); }
};
extern FakeSpi SPI;
