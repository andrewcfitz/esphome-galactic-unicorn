// Port of the Galactic Unicorn display driver from pimoroni/pimoroni-pico
// (MIT, Copyright (c) 2021 Pimoroni Ltd). Audio, buttons, and the light
// sensor are deliberately omitted.
#pragma once

#ifdef USE_RP2040

#include <cstdint>
#include "hardware/pio.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornPanel {
 public:
  static const int WIDTH = 53;
  static const int HEIGHT = 11;

  // Pin assignments are fixed by the hardware.
  static const uint8_t COLUMN_CLOCK = 13;
  static const uint8_t COLUMN_DATA = 14;
  static const uint8_t COLUMN_LATCH = 15;
  static const uint8_t COLUMN_BLANK = 16;
  static const uint8_t ROW_BIT_0 = 17;

  // Returns false if a PIO state machine or the two DMA channels could not
  // be claimed. Never aborts.
  bool init();

  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
  void set_brightness(float value);
  float get_brightness() const;
  void clear();

  // Populated by init() for dump_config().
  int claimed_pio_index{-1};
  int claimed_sm{-1};

 protected:
  static const uint32_t ROW_COUNT = HEIGHT;
  static const uint32_t BCD_FRAME_COUNT = 14;
  static const uint32_t BCD_FRAME_BYTES = 60;
  static const uint32_t ROW_BYTES = BCD_FRAME_COUNT * BCD_FRAME_BYTES;
  static const uint32_t BITSTREAM_LENGTH = ROW_COUNT * ROW_BYTES;  // 9240 bytes

  void configure_shift_registers_();

  PIO pio_{nullptr};
  uint sm_{0};
  uint offset_{0};
  int dma_channel_{-1};
  int dma_ctrl_channel_{-1};
  uint16_t brightness_{128};

  // Must be 4-byte aligned for the 32-bit DMA transfer.
  alignas(4) uint8_t bitstream_[BITSTREAM_LENGTH] = {0};
  const uint32_t bitstream_addr_ = (uint32_t) bitstream_;
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
