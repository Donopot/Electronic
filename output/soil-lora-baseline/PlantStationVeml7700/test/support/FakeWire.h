#pragma once
#include "config.h"
#include <assert.h>
#include <stddef.h>
struct FakeWire {
    explicit FakeWire(uint8_t controller = 0) : controller(controller) {}
    uint8_t controller;
    static bool& oledPresent() { static bool present = true; return present; }
    uint16_t regs[8] = {1, 0, 0, 0, 1000, 0, 0, 0};
    uint8_t buffer[8] = {};
    size_t used = 0, remaining = 0, offset = 0;
    uint8_t selected = 0, nack = 0, targetAddress = 0;
    int starts = 0, ends = 0, reads = 0;
    int shortReg = -1, busReg = -1;
    bool beginOk = true, shortWrite = false, resetAfterAmbient = false;
    bool missingReadByte = false;
    bool ignoreShutdownWrite = false;
    void end() { ++ends; }
    bool begin(int sda, int scl, uint32_t hz) {
        assert(hz == 100000);
        assert(controller == 0 ? (sda == 4 && scl == 5) : (controller == 1 && sda == 17 && scl == 18));
        ++starts; return beginOk;
    }
    void setTimeOut(uint16_t timeout) { assert(timeout == plant::kTimeoutMs); }
    void beginTransmission(uint8_t address) { assert(address < 127); targetAddress = address; used = 0; }
    size_t write(const uint8_t* data, size_t length) {
        if (shortWrite) return 0;
        for (size_t i = 0; i < length; ++i) buffer[used++] = data[i];
        return length;
    }
    uint8_t endTransmission(bool stop) {
        if (controller == 1) return targetAddress == 0x3C && oledPresent() ? 0 : 2;
        if (!beginOk) return 4;
        if (nack) return nack;
        if (targetAddress != plant::kAddress) return 2;
        if (!used) return 0;
        selected = buffer[0];
        assert(selected < 8);
        if (used == 3) {
            assert(stop); // toute ecriture de registre termine la transaction
            regs[selected] = buffer[1] | (buffer[2] << 8);
            if (selected == 0 && ignoreShutdownWrite) regs[0] &= ~1;
        } else assert(used == 1 && !stop);
        return 0;
    }
    size_t requestFrom(uint8_t address, size_t length, bool stop) {
        assert(address == plant::kAddress && length == 2 && stop);
        ++reads; offset = 0;
        remaining = selected == busReg ? 0 : selected == shortReg ? 1 : 2;
        return remaining;
    }
    int available() { return static_cast<int>(remaining); }
    int read() {
        if (!remaining) return -1;
        --remaining;
        const int value = (regs[selected] >> (8 * offset++)) & 255;
        if (missingReadByte && selected == 4) return -1;
        if (resetAfterAmbient && selected == 4 && !remaining) regs[0] = 1;
        return value;
    }
};
