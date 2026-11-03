#include <Arduino.h>
#include <SPI.h>
FakeSerial Serial;
FakeSpi SPI;
// Inclure le vrai code permet d'exercer ses fonctions privees sans API de test.
#include "../src/main.cpp"

static const host::Transmission& latest() { return host::radio().transmissions.back(); }
int main(int argc, char** argv) {
  const std::string scenario = argc > 1 ? argv[1] : "cadence";
  if (scenario == "rng") {
    host::rngReady() = false;
    setup(); assert(!sessionReady && millis() >= 25 && millis() < 40);
    serviceTransmit(false, 0); assert(current.sequence == 1 && host::radio().transmissions.empty());
    host::setNow(10000); host::rngReady() = true;
    serviceTransmit(false, 0); assert(sessionReady && host::radio().transmissions.empty());
    host::setNow(30025); serviceTransmit(false, 0);
    assert(latest().packet.session != 0 && latest().packet.sequence == 2);
  } else if (scenario == "usb" || scenario == "disconnected") {
    host::connected() = scenario == "usb";
    host::stalled() = scenario == "usb";
    setup();
    const uint32_t start = millis();
    for (unsigned i=0; i<70; ++i) {
      const uint32_t before = millis(); loop();
      assert(static_cast<uint32_t>(millis() - before) < 1500);
    }
    assert(host::radio().transmissions.size() >= 3);
    for (size_t i=1; i<host::radio().transmissions.size(); ++i)
      assert(static_cast<uint32_t>(host::radio().transmissions[i].time - host::radio().transmissions[i-1].time) >= 30000);
    assert(static_cast<uint32_t>(millis() - start) < 100000);
    host::connected() = true; host::stalled() = false;
    loop();
    assert((host::usb().find("[SOIL_LOG] dropped_lines=") != std::string::npos) == (scenario == "usb"));
    assert(host::pins()[0] == "0:0");
  } else {
    setup(); adsAddress = 0x48;
    const uint32_t base = scenario == "wrap" ? 0xFFFFF000U : 100U;
    host::setNow(base); serviceTransmit(true, 21565);
    assert(latest().packet.raw == 21565 && latest().packet.sequence == 1);
    host::setNow(base + 29999); serviceTransmit(true, 8000); assert(host::radio().transmissions.size() == 1);
    host::setNow(base + 30000); serviceTransmit(false, 0);
    assert(latest().packet.status == plantsoil::AdcStatus::ReadError && latest().packet.raw == 0);
    host::setNow(base + 200000); serviceTransmit(true, 26401);
    assert(latest().packet.status == plantsoil::AdcStatus::RangeError);
    for (int i=0;i<20;++i) serviceTransmit(true, 8000);
    assert(host::radio().transmissions.size() == 3); // pas de rattrapage
    host::setNow(base + 230000); adsAddress = 0; serviceTransmit(false, 0);
    assert(latest().packet.status == plantsoil::AdcStatus::Unavailable && latest().packet.address == 0);
    host::setNow(base + 260000); adsAddress = 0x49; host::radio().transmitCode = -2; serviceTransmit(true, -8);
    assert(current.sequence == 5 && latest().packet.raw == -8);
    host::radio().transmitCode = 0;
    host::setNow(base + 270000); serviceTransmit(true, 1000); assert(current.sequence == 5);
    host::setNow(base + 289999); serviceTransmit(true, 1000); assert(current.sequence == 5);
    host::setNow(base + 290000); serviceTransmit(true, 1000);
    assert(current.sequence == 6 && latest().packet.sequence == 6);
  }
  puts("PASS vrai XIAO : cadence, panne/reprise, statuts ADC, RNG borne, USB absent/bloque et pertes explicites");
}
