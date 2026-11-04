#include <Arduino.h>
#include <SPI.h>
FakeSerial Serial;
FakeSpi SPI;
#include "../src/main.cpp"

static void command(const char* text) { host::serialInput() += text; loop(); }
static int sampleIndex = 0, failAt = -1;
static const int16_t samples[] = {21000, 20000, 22000, 20500, 20800};
static bool sample(int16_t& raw) {
  assert(probePower.powered() && host::modes()[0] == OUTPUT);
  uint32_t poweredAt = 0;
  for (const auto& e:host::gpio())
    if (e.pin == 0 && e.operation == 'm' && e.value == OUTPUT) poweredAt = e.time;
  assert(static_cast<uint32_t>(millis() - poweredAt) >= 1000);
  const int index = sampleIndex++;
  delay(8);
  if (index == failAt) return false;
  raw = samples[index % 5];
  return true;
}
static bool outOfRange(int16_t& raw) { raw = 27000; return true; }
static void checkSafeGpio() {
  const auto& events = host::gpio();
  for (size_t i = 0; i < events.size(); ++i) {
    const auto& e = events[i];
    if (e.pin != 0) continue;
    assert(!(e.operation == 'w' && e.value == HIGH));
    if (e.operation == 'm' && e.value == OUTPUT) {
      assert(i && events[i-1].pin == 0 && events[i-1].operation == 'w' && events[i-1].value == LOW);
    } else if (e.operation == 'm') assert(e.value == INPUT);
  }
}
int main(int argc, char** argv) {
  const std::string scenario = argc > 1 ? argv[1] : "acquire";
  setup();
  assert(!probePower.powered() && host::modes()[0] == INPUT);
  if (scenario == "acquire") {
    int16_t raw = -1;
    assert(probePower.acquire(sample, raw) && raw == 20800 && sampleIndex == 5);
    assert(!probePower.powered() && host::modes()[0] == INPUT);
    for (int fail = 0; fail < 5; ++fail) {
      sampleIndex = 0; failAt = fail; raw = 123;
      assert(!probePower.acquire(sample, raw) && raw == 0);
      assert(!probePower.powered() && host::modes()[0] == INPUT);
    }
  } else if (scenario == "commands") {
    command("OFF\n"); const uint32_t before = attempts;
    for (int i=0;i<10;++i) loop();
    assert(attempts == before && !probePower.powered() && host::radio().transmissions.empty());
    command("ON\r\n");
    assert(probeMode == ProbeMode::ManualOn && probePower.powered());
    host::connected() = false; // fermeture terminal, USB toujours configure
    loop(); assert(probePower.powered());
    host::usbMounted() = false; loop();
    assert(probeMode == ProbeMode::Off && !probePower.powered());
    host::connected() = true; host::usbMounted() = true;
    command("ON\n"); const uint32_t started = manualStarted;
    const uint32_t manualAttempts = attempts;
    host::setNow(started + 119000); loop(); assert(probePower.powered());
    host::setNow(started + 119980); loop();
    assert(static_cast<uint32_t>(millis() - started) < 120000 && attempts == manualAttempts);
    host::setNow(started + 120000); loop(); assert(!probePower.powered() && probeMode == ProbeMode::Off);
    command("AUTO\n");
    assert(probeMode == ProbeMode::Auto && !probePower.powered());
    assert(!host::radio().transmissions.empty());
  } else if (scenario == "gate") {
    host::gateHeldLow() = true;
    command("ON\n"); assert(!probePower.powered() && probeMode == ProbeMode::Off);
    int16_t raw = 123; sampleIndex = 0;
    assert(!probePower.acquire(sample, raw) && sampleIndex == 0 && raw == 0);
  } else if (scenario == "parser") {
    command("OFF\n");
    command("AAAAAAAAAAAAAAAAAAAAAAAAON\n"); assert(probeMode == ProbeMode::Off);
    host::serialInput().append("ON\0bad\n", 7); loop(); assert(probeMode == ProbeMode::Off);
    command("O\rN\n"); assert(probeMode == ProbeMode::Off);
    command("O"); host::connected() = false; loop(); host::connected() = true;
    command("N\n"); assert(probeMode == ProbeMode::Off);
    host::serialInput() = std::string(1000, 'X'); loop();
    assert(host::serialInput().size() == 936); // budget 64 octets par tour
  } else if (scenario == "integration" || scenario == "radio_fault") {
    adsAddress = 0x48;
    if (scenario == "radio_fault") { radioReady = false; host::radio().beginCode = -2; }
    host::setNow(lastPowerCycle + 30000); runPowerLoop(sample);
    assert(!probePower.powered());
    if (scenario == "integration") {
      assert(host::radio().transmissions.back().packet.raw == 20800 && readings == 1);
      failAt = sampleIndex + 2;
      host::setNow(lastPowerCycle + 30000); runPowerLoop(sample);
      const auto& failed = host::radio().transmissions.back().packet;
      assert(failed.status == plantsoil::AdcStatus::ReadError && failed.raw == 0 && errors == 1);
      failAt = -1;
      host::setNow(lastPowerCycle + 30000); runPowerLoop(outOfRange);
      assert(host::radio().transmissions.back().packet.status == plantsoil::AdcStatus::RangeError);
      for (size_t i=1;i<host::radio().transmissions.size();++i)
        assert(host::radio().transmissions[i].time - host::radio().transmissions[i-1].time >= 30000);
    } else {
      assert(host::radio().transmissions.empty());
      host::radio().beginCode = 0; host::radio().transmitCode = -2;
      host::setNow(lastPowerCycle + 30000); runPowerLoop(sample);
      assert(!probePower.powered() && !radioReady && !host::radio().transmissions.empty());
      host::radio().transmitCode = 0;
      host::setNow(lastPowerCycle + 30000); runPowerLoop(sample);
      assert(!probePower.powered() && radioReady && host::radio().transmissions.back().result == 0);
    }
  } else if (scenario == "deferred_error") {
    adsAddress = 0x48; int16_t raw = 123;
    assert(!probePower.acquire(readA0, raw) && deferredAdcError != nullptr && !probePower.powered());
    ++attempts; reportMeasurement(false, raw);
    assert(deferredAdcError == nullptr && host::usb().find("Ecriture configuration ADC") != std::string::npos);
  } else if (scenario == "auto" || scenario == "blocked") {
    host::connected() = scenario == "blocked";
    host::stalled() = scenario == "blocked";
    const uint32_t base = lastPowerCycle;
    host::setNow(base + 29900); loop(); assert(attempts == 0);
    for (int i=0;i<3;++i) {
      host::setNow(lastPowerCycle + 30000); loop();
      assert(!probePower.powered() && host::modes()[0] == INPUT);
    }
    const auto& tx = host::radio().transmissions;
    assert(tx.size() == 3);
    for (size_t i=1;i<tx.size();++i) assert(static_cast<uint32_t>(tx[i].time - tx[i-1].time) >= 30000);
    assert(readings == 0 && errors == 3); // vrai bus sans ADS : aucun ancien brut reutilise
    for (const auto& t:tx) assert(t.packet.status == plantsoil::AdcStatus::Unavailable && t.packet.raw == 0);
  } else if (scenario == "wrap") {
    host::setNow(0xFFFFFF00U); command("ON\n");
    const uint32_t start = manualStarted;
    host::setNow(start + 120000U); loop(); assert(!probePower.powered());
    command("AUTO\n");
    const uint32_t first = host::radio().transmissions.back().time;
    host::setNow(lastPowerCycle + 30000U); loop();
    assert(static_cast<uint32_t>(host::radio().transmissions.back().time - first) >= 30000);
  }
  checkSafeGpio();
  puts("PASS P-MOS : grille sans HIGH, OFF sur erreur, mediane, commandes bornees et cadence");
}
