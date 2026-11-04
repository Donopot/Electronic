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
  const uint32_t setupStarted = millis();
  setup();
  assert(current.node == SOIL_NODE_ID);
  assert(!probePower.powered() && host::modes()[0] == INPUT);
  if (scenario == "banner") {
    constexpr uint32_t firstSeconds[] = {30, 35, 40, 45};
    loop();
    char identity[48], initial[48], state[96];
    snprintf(identity,sizeof(identity),"[CONFIG] node=SOIL-%02u",static_cast<unsigned>(SOIL_NODE_ID));
    snprintf(initial,sizeof(initial),"premier apres %lu s",static_cast<unsigned long>(firstSeconds[SOIL_NODE_ID-1]));
    snprintf(state,sizeof(state),"[SOIL_POWER] mode=AUTO gate=1 node=SOIL-%02u",static_cast<unsigned>(SOIL_NODE_ID));
    assert(host::usb().find(identity)!=std::string::npos && host::usb().find(initial)!=std::string::npos);
    assert(host::usb().find(state)!=std::string::npos && attempts==0 && !probePower.powered());
    assert(host::radio().transmissions.empty());
  } else if (scenario == "acquire") {
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
    host::setNow(lastPowerCycle + plantsoil::kTransmitMs); runPowerLoop(sample);
    assert(!probePower.powered());
    if (scenario == "integration") {
      assert(host::radio().transmissions.back().packet.raw == 20800 && readings == 1);
      failAt = sampleIndex + 2;
      host::setNow(lastPowerCycle + plantsoil::kTransmitMs); runPowerLoop(sample);
      const auto& failed = host::radio().transmissions.back().packet;
      assert(failed.status == plantsoil::AdcStatus::ReadError && failed.raw == 0 && errors == 1);
      failAt = -1;
      host::setNow(lastPowerCycle + plantsoil::kTransmitMs); runPowerLoop(outOfRange);
      assert(host::radio().transmissions.back().packet.status == plantsoil::AdcStatus::RangeError);
      for (size_t i=1;i<host::radio().transmissions.size();++i)
        assert(host::radio().transmissions[i].time - host::radio().transmissions[i-1].time >= plantsoil::kTransmitMs);
    } else {
      assert(host::radio().transmissions.empty());
      char marker[64]; snprintf(marker, sizeof(marker), "[SOIL_TX_ERROR] node=SOIL-%02u", static_cast<unsigned>(SOIL_NODE_ID));
      assert(host::usb().find(marker) != std::string::npos);
      host::radio().beginCode = 0; host::radio().transmitCode = -2;
      host::setNow(lastPowerCycle + plantsoil::kTransmitMs); runPowerLoop(sample);
      assert(!probePower.powered() && !radioReady && !host::radio().transmissions.empty());
      host::radio().transmitCode = 0;
      host::setNow(lastPowerCycle + plantsoil::kTransmitMs); runPowerLoop(sample);
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
    host::setNow(base + plantsoil::kTransmitMs - 100); loop(); assert(attempts == 0);
    for (int i=0;i<3;++i) {
      host::setNow(lastPowerCycle + plantsoil::kTransmitMs); loop();
      assert(!probePower.powered() && host::modes()[0] == INPUT);
    }
    const auto& tx = host::radio().transmissions;
    assert(tx.size() == 3);
    for (size_t i=1;i<tx.size();++i) assert(static_cast<uint32_t>(tx[i].time - tx[i-1].time) >= plantsoil::kTransmitMs);
    assert(readings == 0 && errors == 3); // vrai bus sans ADS : aucun ancien brut reutilise
    for (const auto& t:tx) assert(t.packet.status == plantsoil::AdcStatus::Unavailable && t.packet.raw == 0);
  } else if (scenario == "auto_guard") {
    adsAddress = 0x48;
    const auto requestAuto = []() {
      host::serialInput() += "AUTO\n";
      const uint32_t before = attempts;
      runPowerLoop(sample);
      assert(attempts == before + 1 && !probePower.powered());
    };
    requestAuto();
    assert(host::radio().transmissions.size() == 1);
    const uint32_t firstTx = host::radio().transmissions[0].time;
    host::setNow(firstTx + 30000); requestAuto();
    assert(host::radio().transmissions.size() == 1);
    host::setNow(firstTx + 1800000UL - 2000); requestAuto();
    assert(host::radio().transmissions.size() == 1);
    host::setNow(firstTx + 1800000UL); requestAuto();
    assert(host::radio().transmissions.size() == 2);
    const uint32_t delta = host::radio().transmissions[1].time - firstTx;
    assert(delta >= 1800000UL && delta < 1802000UL && readings == 4);
  } else if (scenario == "cadence") {
    static_assert(plantsoil::kTransmitMs == 1800000UL, "Releves toutes les trente minutes");
    constexpr uint32_t expectedInitial[] = {30000UL, 35000UL, 40000UL, 45000UL};
    static_assert(kInitialSampleDelayMs == expectedInitial[SOIL_NODE_ID - 1], "Delai initial selon le noeud");
    host::connected() = false;
    adsAddress = 0x48;
    const uint32_t firstDue = setupStarted + expectedInitial[SOIL_NODE_ID - 1];
    host::setNow(firstDue - 11); runPowerLoop(sample); assert(attempts == 0);
    host::setNow(firstDue - 10); runPowerLoop(sample);
    assert(attempts == 1 && readings == 1 && host::radio().transmissions.size() == 1);
    assert(host::radio().transmissions[0].packet.node == SOIL_NODE_ID);
    const uint32_t completed = lastPowerCycle;
    const auto gpioCount = host::gpio().size();
    host::setNow(completed + 30000); runPowerLoop(sample);
    host::setNow(completed + 1800000UL - 11); runPowerLoop(sample);
    assert(attempts == 1 && host::gpio().size() == gpioCount && !probePower.powered());
    host::setNow(completed + 1800000UL - 10); runPowerLoop(sample);
    assert(attempts == 2 && readings == 2 && host::radio().transmissions.size() == 2);
    const auto& tx = host::radio().transmissions;
    assert(static_cast<uint32_t>(tx[1].time - tx[0].time) >= 1800000UL);
    // Retard de plusieurs periodes : une seule acquisition, aucun rattrapage.
    host::setNow(lastPowerCycle + 4 * plantsoil::kTransmitMs); runPowerLoop(sample);
    const uint32_t afterPause = attempts;
    for (int i = 0; i < 5; ++i) runPowerLoop(sample);
    assert(attempts == afterPause && afterPause == 3);
    // Une periode automatique traverse aussi le debordement de millis().
    host::setNow(0xFFF00000U);
    lastPowerCycle = millis() - plantsoil::kTransmitMs;
    runPowerLoop(sample);
    const uint32_t beforeWrap = lastPowerCycle;
    const uint32_t beforeAttempts = attempts;
    host::setNow(beforeWrap + 1800000UL - 11); runPowerLoop(sample);
    assert(attempts == beforeAttempts && !probePower.powered());
    host::setNow(beforeWrap + 1800000UL - 10); runPowerLoop(sample);
    assert(attempts == beforeAttempts + 1 && !probePower.powered());
  } else if (scenario == "wrap") {
    host::setNow(0xFFFFFF00U); command("ON\n");
    const uint32_t start = manualStarted;
    host::setNow(start + 120000U); loop(); assert(!probePower.powered());
    command("AUTO\n");
    const uint32_t first = host::radio().transmissions.back().time;
    host::setNow(lastPowerCycle + plantsoil::kTransmitMs); loop();
    assert(static_cast<uint32_t>(host::radio().transmissions.back().time - first) >= plantsoil::kTransmitMs);
  }
  checkSafeGpio();
  puts("PASS P-MOS : grille sans HIGH, OFF sur erreur, mediane, commandes bornees et cadence");
}
