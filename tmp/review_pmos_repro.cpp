#include <Arduino.h>
#include <SPI.h>
FakeSerial Serial;
FakeSpi SPI;
#include "C:/Users/Anwender/Documents/ChatGPT/Electronic/firmware/XiaoSoilDiagnostic/src/main.cpp"
int main() {
  setup();
  host::serialInput() = std::string("ON\0bad\n", 7);
  loop();
  printf("NUL_line_accepted_as_ON=%d\n", probeMode == ProbeMode::ManualOn);
  host::setNow(manualStarted + 119980U);
  loop();
  printf("elapsed=%lu powered=%d\n", static_cast<unsigned long>(millis()-manualStarted), probePower.powered());
  loop();
  printf("elapsed=%lu powered=%d\n", static_cast<unsigned long>(millis()-manualStarted), probePower.powered());
}
