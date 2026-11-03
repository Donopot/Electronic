#include "sensors/StrictWireTransport.h"
#include "diagnostics/Diagnostics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits>
#include <initializer_list>

using namespace plant;
#include "support/FakeWire.h"

static void successAndCalculation() {
    FakeWire wire;
    StrictWireTransport<FakeWire> transport(wire);
    VEML7700Sensor sensor(transport);
    assert(sensor.begin(10) == Status::WarmingUp);
    assert(wire.regs[0] == 0x1000 && wire.regs[3] == 0);
    const int reads = wire.reads;
    assert(sensor.measure(159).status == Status::WarmingUp && wire.reads == reads);
    auto value = sensor.measure(160);
    assert(value.status == Status::Ok && value.raw == 1000 && fabs(value.lux - 537.6) < 1e-8);
    wire.regs[4] = 0;
    value = sensor.measure(1160);
    assert(value.status == Status::Ok && value.lux == 0);
    const double x = 2000 * 0.5376;
    const double expected = 6.0135e-13 * pow(x, 4) - 9.3924e-9 * pow(x, 3) + 8.1488e-5 * x*x + 1.0023*x;
    assert(fabs(VEML7700Sensor::toLux(2000) - expected) < 1e-8);
    wire.regs[4] = 44586;
    value = sensor.measure(2160);
    assert(value.status == Status::Ok && value.lux < 140000 && value.lux > 139990);
    wire.regs[4] = 44587;
    value = sensor.measure(2161);
    assert(value.status == Status::Saturated && value.raw == 44587 && value.lux == 0);
    assert(sensor.status() == Status::Saturated);
    wire.regs[4] = 65534;
    value = sensor.measure(2162);
    assert(value.status == Status::Saturated && value.lux == 0);
    assert(sensor.status() == Status::Saturated);
    wire.regs[4] = 65535;
    value = sensor.measure(3160);
    assert(value.status == Status::Saturated && value.lux == 0);
    assert(sensor.status() == Status::Saturated);
    wire.regs[4] = 5;
    assert(sensor.measure(4160).status == Status::Ok);
    assert(sensor.status() == Status::Ok);
    puts("PASS conversion, obscurite, delai, correction, borne, saturation reversible");
}

static void errorsAndRecovery() {
    for (int failure = 0; failure < 6; ++failure) {
        FakeWire wire;
        StrictWireTransport<FakeWire> transport(wire);
        VEML7700Sensor sensor(transport);
        sensor.begin(0);
        assert(sensor.measure(150).status == Status::Ok);
        Status expected;
        switch (failure) {
        case 0: wire.nack = 2; expected = Status::Nack; break;
        case 1: wire.shortReg = 4; expected = Status::ShortRead; break;
        case 2: wire.busReg = 4; expected = Status::BusError; break;
        case 3: wire.regs[0] = 1; expected = Status::ConfigLost; break;
        case 4: wire.regs[3] = 1; expected = Status::ConfigLost; break;
        default: wire.resetAfterAmbient = true; expected = Status::ConfigLost; break;
        }
        auto bad = sensor.measure(1000);
        assert(bad.status == expected && bad.raw == 0 && bad.lux == 0);
        const int restarts = wire.starts;
        wire.nack = 0; wire.shortReg = -1; wire.busReg = -1; wire.resetAfterAmbient = false;
        sensor.service(3999);
        assert(wire.starts == restarts && sensor.measure(3999).status == expected);
        sensor.service(4000);
        assert(wire.starts == restarts + 1 && wire.ends == wire.starts);
        assert(sensor.measure(4149).status == Status::WarmingUp);
        assert(sensor.measure(4150).status == Status::Ok);
    }
    puts("PASS NACK, lecture courte, bus defaillant, config/perte alimentation, reprise et attente conversion");
}

static void strictTransport() {
    FakeWire wire;
    StrictWireTransport<FakeWire> transport(wire);
    assert(transport.restart() == Status::Ok && transport.configure() == Status::Ok);
    uint16_t raw = 999;
    wire.shortReg = 4;
    const int before = wire.reads;
    assert(transport.ambient(raw) == Status::ShortRead && wire.reads == before + 1);
    wire.shortReg = -1; wire.missingReadByte = true;
    assert(transport.ambient(raw) == Status::ShortRead);
    wire.missingReadByte = false; wire.shortWrite = true;
    assert(transport.configure() == Status::BusError);
    wire.shortWrite = false; wire.nack = 5;
    assert(transport.configure() == Status::BusError);
    wire.nack = 3;
    assert(transport.configure() == Status::Nack);
    wire.nack = 0; wire.beginOk = false;
    assert(transport.restart() == Status::BusError);
    puts("PASS transport reel avec faux Wire : requete unique, read=-1, ecriture courte, timeout, NACK, begin refuse");
}

static void wrapAndRetries() {
    assert(!elapsed(0xFFFFFFF0U, 0xFFFFFF00U, 1000));
    assert(elapsed(744, 0xFFFFFF00U, 1000));
    assert(!elapsed(743, 0xFFFFFF00U, 1000));
    Uptime uptime;
    assert(uptime.update(0xFFFFFFF0U) == 4294967280ULL);
    assert(uptime.update(20) == 4294967316ULL);
    FakeWire wire;
    wire.nack = 2;
    StrictWireTransport<FakeWire> transport(wire);
    VEML7700Sensor sensor(transport);
    const uint32_t start = 0xFFFFFF00U;
    assert(sensor.begin(start) == Status::Nack);
    sensor.service(start + kRetryMs - 1);
    assert(wire.starts == 1);
    sensor.service(start + kRetryMs);
    assert(wire.starts == 2 && sensor.status() == Status::Nack);
    wire.nack = 0;
    sensor.service(start + 2 * kRetryMs);
    assert(wire.starts == 3);
    assert(sensor.measure(start + 2 * kRetryMs + 149).status == Status::WarmingUp);
    assert(sensor.measure(start + 2 * kRetryMs + 150).status == Status::Ok);
    puts("PASS cadence, uptime et reprises au debordement millis, tentatives periodiques bornees");
}
static uint32_t fakeNow = 0;
static uint32_t readClock() { return fakeNow; }
static void conversionStartsAfterConfiguration() {
    struct SlowTransport : SensorTransport {
        Status restart() override { fakeNow += 25; return Status::Ok; }
        Status configure() override { fakeNow += 200; return Status::Ok; }
        Status verify() override { return Status::Ok; }
        Status ambient(uint16_t& raw) override { raw = 42; return Status::Ok; }
    } transport;
    fakeNow = 0;
    VEML7700Sensor sensor(transport, readClock);
    sensor.begin(0);
    assert(fakeNow == 225);
    assert(sensor.measure(0).status == Status::WarmingUp);
    fakeNow = 374;
    assert(sensor.measure(0).status == Status::WarmingUp);
    fakeNow = 375;
    assert(sensor.measure(0).status == Status::Ok);
    puts("PASS attente de conversion mesuree apres fin des transactions de configuration");
}

static void serialization() {
    char line[640];
    for (Status status : {Status::Ok, Status::WarmingUp, Status::Nack, Status::ShortRead,
                           Status::BusError, Status::ConfigLost, Status::Saturated}) {
        Measurement value;
        value.status = status; value.raw = status == Status::Saturated ? 65535 : 1000; value.lux = 537.6;
        assert(Diagnostics::format(line, sizeof(line), true, "mesure", 4294967316ULL, value,
                                   "test \"guillemet\"\\ et\ncontrole", 16));
        puts(line);
    }
    for (double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), -1.0}) {
        Measurement value; value.status = Status::Ok; value.lux = invalid;
        assert(Diagnostics::format(line, sizeof(line), true, "mesure", 1, value)); puts(line);
    }
    Measurement state;
    state.status = Status::Ok;
    assert(Diagnostics::format(line, sizeof(line), true, "scan_ack", 0, state, "ACK sans preuve d'identite", 16)); puts(line);
    state.status = Status::WarmingUp;
    assert(Diagnostics::format(line, sizeof(line), false, "mesure", 0, state));
    assert(strstr(line, "PlantStation") && strstr(line, "lux=null"));
    assert(strstr(line, "adresse=globale") && !strstr(line, "FFFFFFFF"));
    assert(Diagnostics::format(line, sizeof(line), false, "mesure", 0, state, "", 16));
    assert(strstr(line, "adresse=0x10"));
    assert(!Diagnostics::format(line, 10, true, "mesure", 0, state));
    puts("PASS serialisation reelle : schema, erreur, saturation, nombres invalides, echappement et tampon court");
}
static void scannerBounds() {
    struct Probe {
        int calls = 0;
        bool error = false;
        Status probe(uint8_t address) {
            assert(address >= 8 && address <= 119); ++calls;
            return error ? Status::BusError : address == 16 ? Status::Ok : Status::Nack;
        }
    } transport;
    I2cScanner<Probe> scanner(true);
    int acks = 0;
    while (!scanner.done()) if (scanner.step(transport).status == Status::Ok) ++acks;
    assert(transport.calls == 112 && acks == 1);
    scanner.step(transport);
    assert(transport.calls == 112);
    I2cScanner<Probe> disabled(false);
    assert(disabled.done());
    I2cScanner<Probe> broken(true);
    transport.error = true;
    assert(broken.step(transport).done && transport.calls == 113);
    puts("PASS scanner reel : 112 adresses non reservees, ACK, desactivation et arret erreur bus");
}
int main() {
    successAndCalculation(); errorsAndRecovery(); strictTransport(); wrapAndRetries(); conversionStartsAfterConfiguration(); scannerBounds(); serialization();
    puts("PASS tous les tests C++ executes");
}
