// Port of the Galactic Unicorn display driver from pimoroni/pimoroni-pico
// (MIT, Copyright (c) 2021 Pimoroni Ltd).
#ifdef USE_RP2040

#include "gu_panel.h"
#include "gu_gamma.h"
#include "gu_pio.h"

#include <cmath>
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace esphome {
namespace galactic_unicorn {

bool GalacticUnicornPanel::init() {
  // Seed the per-frame constants: pixel count, row select, and BCD tick count.
  for (uint8_t row = 0; row < HEIGHT; row++) {
    for (uint8_t frame = 0; frame < BCD_FRAME_COUNT; frame++) {
      uint8_t *p = &this->bitstream_[row * ROW_BYTES + (BCD_FRAME_BYTES * frame)];
      p[0] = WIDTH - 1;
      p[1] = row;
      uint32_t bcd_ticks = (1u << frame);
      p[56] = (bcd_ticks & 0xff) >> 0;
      p[57] = (bcd_ticks & 0xff00) >> 8;
      p[58] = (bcd_ticks & 0xff0000) >> 16;
      p[59] = (bcd_ticks & 0xff000000) >> 24;
    }
  }

  gpio_init(COLUMN_CLOCK); gpio_set_dir(COLUMN_CLOCK, GPIO_OUT); gpio_put(COLUMN_CLOCK, false);
  gpio_init(COLUMN_DATA); gpio_set_dir(COLUMN_DATA, GPIO_OUT); gpio_put(COLUMN_DATA, false);
  gpio_init(COLUMN_LATCH); gpio_set_dir(COLUMN_LATCH, GPIO_OUT); gpio_put(COLUMN_LATCH, false);
  gpio_init(COLUMN_BLANK); gpio_set_dir(COLUMN_BLANK, GPIO_OUT); gpio_put(COLUMN_BLANK, true);

  // Park the row select on a non-visible row so setup does not flash.
  for (uint8_t pin = ROW_BIT_0; pin < ROW_BIT_0 + 4; pin++) {
    gpio_init(pin); gpio_set_dir(pin, GPIO_OUT); gpio_put(pin, true);
  }

  sleep_ms(100);
  this->configure_shift_registers_();

  // Claim a state machine on either PIO block. This is the single most
  // important deviation from Pimoroni's driver, which hardcodes pio0.
  //
  // pico-sdk 1.5.1 (what arduino-pico ships) has no
  // pio_claim_free_sm_and_add_program (that call was added in SDK 2.0), so
  // search both PIO blocks by hand. Hardcoding pio0 the way Pimoroni do
  // risks losing the race against the CYW43439 WiFi driver, which is itself
  // driven over PIO. NUM_PIOS is not defined in this SDK, so 2 is used
  // directly.
  bool claimed = false;
  for (uint i = 0; i < 2 && !claimed; i++) {
    PIO candidate = (i == 0) ? pio0 : pio1;
    if (!pio_can_add_program(candidate, &galactic_unicorn_program)) continue;
    int sm = pio_claim_unused_sm(candidate, false);
    if (sm < 0) continue;
    this->pio_ = candidate;
    this->sm_ = (uint) sm;
    this->offset_ = pio_add_program(candidate, &galactic_unicorn_program);
    claimed = true;
  }
  if (!claimed) return false;
  this->claimed_pio_index = pio_get_index(this->pio_);
  this->claimed_sm = (int) this->sm_;

  for (uint8_t pin = COLUMN_CLOCK; pin < COLUMN_CLOCK + 8; pin++) {
    pio_gpio_init(this->pio_, pin);
  }

  // Raise blank and the row bits before enabling outputs, to avoid a flash.
  const uint pins_to_set = 1 << COLUMN_BLANK | 0b1111 << ROW_BIT_0;
  pio_sm_set_pins_with_mask(this->pio_, this->sm_, pins_to_set, pins_to_set);
  pio_sm_set_consecutive_pindirs(this->pio_, this->sm_, COLUMN_CLOCK, 8, true);

  pio_sm_config c = galactic_unicorn_program_get_default_config(this->offset_);
  sm_config_set_out_shift(&c, true, true, 32);  // shift right, autopull, threshold 32
  sm_config_set_out_pins(&c, ROW_BIT_0, 4);
  sm_config_set_set_pins(&c, COLUMN_DATA, 3);
  sm_config_set_sideset_pins(&c, COLUMN_CLOCK);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  this->dma_channel_ = dma_claim_unused_channel(false);
  this->dma_ctrl_channel_ = dma_claim_unused_channel(false);
  if (this->dma_channel_ < 0 || this->dma_ctrl_channel_ < 0) {
    if (this->dma_channel_ >= 0) dma_channel_unclaim(this->dma_channel_);
    if (this->dma_ctrl_channel_ >= 0) dma_channel_unclaim(this->dma_ctrl_channel_);
    pio_sm_unclaim(this->pio_, this->sm_);
    return false;
  }

  // The control channel rewinds the data channel's read address, so the two
  // chain into each other and replay the bitstream forever.
  dma_channel_config ctrl_config = dma_channel_get_default_config(this->dma_ctrl_channel_);
  channel_config_set_transfer_data_size(&ctrl_config, DMA_SIZE_32);
  channel_config_set_read_increment(&ctrl_config, false);
  channel_config_set_write_increment(&ctrl_config, false);
  channel_config_set_chain_to(&ctrl_config, this->dma_channel_);
  dma_channel_configure(this->dma_ctrl_channel_, &ctrl_config,
                        &dma_hw->ch[this->dma_channel_].read_addr,
                        &this->bitstream_addr_, 1, false);

  dma_channel_config config = dma_channel_get_default_config(this->dma_channel_);
  channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
  channel_config_set_bswap(&config, false);
  channel_config_set_dreq(&config, pio_get_dreq(this->pio_, this->sm_, true));
  channel_config_set_chain_to(&config, this->dma_ctrl_channel_);
  dma_channel_configure(this->dma_channel_, &config, &this->pio_->txf[this->sm_],
                        nullptr, BITSTREAM_LENGTH / 4, false);

  pio_sm_init(this->pio_, this->sm_, this->offset_, &c);
  pio_sm_set_enabled(this->pio_, this->sm_, true);
  dma_start_channel_mask(1u << this->dma_ctrl_channel_);
  return true;
}

void GalacticUnicornPanel::configure_shift_registers_() {
  // Select full output current in the column driver chips' register 2.
  const uint16_t reg1 = 0b1111111111001110;
  for (int j = 0; j < 9; j++) {
    for (int i = 0; i < 16; i++) {
      gpio_put(COLUMN_DATA, (reg1 & (1U << (15 - i))) != 0);
      sleep_us(10);
      gpio_put(COLUMN_CLOCK, true);
      sleep_us(10);
      gpio_put(COLUMN_CLOCK, false);
    }
  }
  // The tenth chip latches partway through, which is what commits the value.
  for (int i = 0; i < 16; i++) {
    gpio_put(COLUMN_DATA, (reg1 & (1U << (15 - i))) != 0);
    sleep_us(10);
    gpio_put(COLUMN_CLOCK, true);
    sleep_us(10);
    gpio_put(COLUMN_CLOCK, false);
    if (i == 4) gpio_put(COLUMN_LATCH, true);
  }
  gpio_put(COLUMN_LATCH, false);

  // Reassert blank; the sequence above leaves a faint glow otherwise.
  gpio_put(COLUMN_BLANK, false);
  sleep_us(10);
  gpio_put(COLUMN_BLANK, true);
}

void GalacticUnicornPanel::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;

  // The panel is wired with both axes reversed.
  x = (WIDTH - 1) - x;
  y = (HEIGHT - 1) - y;

  r = (r * this->brightness_) >> 8;
  g = (g * this->brightness_) >> 8;
  b = (b * this->brightness_) >> 8;

  uint16_t gamma_r = GALACTIC_UNICORN_GAMMA_14BIT[r];
  uint16_t gamma_g = GALACTIC_UNICORN_GAMMA_14BIT[g];
  uint16_t gamma_b = GALACTIC_UNICORN_GAMMA_14BIT[b];

  // One bit of each channel lands in each of the 14 BCD frames.
  for (uint8_t frame = 0; frame < BCD_FRAME_COUNT; frame++) {
    uint8_t *p = &this->bitstream_[y * ROW_BYTES + (BCD_FRAME_BYTES * frame) + 2 + x];
    *p = ((gamma_b & 0b1) << 0) | ((gamma_g & 0b1) << 1) | ((gamma_r & 0b1) << 2);
    gamma_r >>= 1;
    gamma_g >>= 1;
    gamma_b >>= 1;
  }
}

void GalacticUnicornPanel::set_brightness(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  // 256 is full scale because it is applied as (channel * brightness) >> 8.
  this->brightness_ = (uint16_t) floorf(value * 256.0f);
}

float GalacticUnicornPanel::get_brightness() const { return this->brightness_ / 256.0f; }

void GalacticUnicornPanel::clear() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) this->set_pixel(x, y, 0, 0, 0);
  }
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
