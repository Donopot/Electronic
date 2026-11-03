#pragma once
#include <Arduino.h>
inline bool tud_cdc_n_connected(uint8_t instance) { assert(instance == 0); return host::connected(); }
inline uint32_t tud_cdc_n_write_available(uint8_t instance) { assert(instance == 0); return host::stalled() ? 0 : 256; }
inline uint32_t tud_cdc_n_write(uint8_t instance, const void* data, uint32_t length) {
  assert(instance == 0 && length <= tud_cdc_n_write_available(0));
  host::usb().append(static_cast<const char*>(data), length); return length;
}
inline uint32_t tud_cdc_n_write_flush(uint8_t instance) { assert(instance == 0); return 0; }
