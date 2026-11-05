# LoRa169

Local copy of [Sandeep Mistry's Arduino-LoRa 0.8.0](https://github.com/sandeepmistry/arduino-LoRa/tree/0.8.0)
with the original MIT license preserved in LICENSE.

Changes for this project's SX1278 radios (137..525 MHz only):

The historical library name is retained; the shared application channel is
now 433.92 MHz. LowFrequencyModeOn also applies at 433 MHz (below 525 MHz).

- Keep RegOpMode.LowFrequencyModeOn (0x08) with the LoRa bit (0x80), for sleep,
  standby, transmission and reception. The upstream constant uses only 0x80.
- Configure SPI before asserting chip select, and release chip select before
  ending the transaction, matching the project's original SPI diagnostic.
- Expose the IRQ flags observed by the last parsePacket() before acknowledgement,
  for receive/error counters without an extra SPI read or receive command.

Reference: [Semtech's LF/HF register definitions](https://github.com/Lora-net/LoRaMac-node/blob/master/src/radio/sx1276/sx1276Regs-LoRa.h).
The application also enters FSK sleep before selecting LoRa and verifies
registers after configuration. This library is not intended for 868/915 MHz.
