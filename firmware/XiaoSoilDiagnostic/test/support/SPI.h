#pragma once
constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings { SPISettings(int, int, int) {} };
struct FakeSpi { void begin() {} };
extern FakeSpi SPI;
