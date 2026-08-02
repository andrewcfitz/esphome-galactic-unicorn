#pragma once

#ifdef USE_RP2040

#include "esphome/core/component.h"
#include "gu_panel.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornHub : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_initial_brightness(float b) { this->initial_brightness_ = b; }

  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    this->panel_.set_pixel(x, y, r, g, b);
  }
  void set_brightness(float b) { this->panel_.set_brightness(b); }
  float get_brightness() const { return this->panel_.get_brightness(); }
  void clear() { this->panel_.clear(); }

 protected:
  GalacticUnicornPanel panel_;
  float initial_brightness_{0.5f};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
