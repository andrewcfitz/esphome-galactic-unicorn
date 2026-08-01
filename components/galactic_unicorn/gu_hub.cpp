#ifdef USE_RP2040

#include "gu_hub.h"
#include "esphome/core/log.h"

namespace esphome {
namespace galactic_unicorn {

static const char *const TAG = "galactic_unicorn";

void GalacticUnicornHub::setup() {
  if (!this->panel_.init()) {
    ESP_LOGE(TAG, "Could not claim a PIO state machine or two DMA channels. "
                  "The WiFi driver may be holding them.");
    this->mark_failed();
    return;
  }
  this->panel_.set_brightness(this->initial_brightness_);
  this->panel_.clear();
}

void GalacticUnicornHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Galactic Unicorn:");
  ESP_LOGCONFIG(TAG, "  Size: %dx%d", GalacticUnicornPanel::WIDTH, GalacticUnicornPanel::HEIGHT);
  ESP_LOGCONFIG(TAG, "  Brightness: %.2f", this->panel_.get_brightness());
  ESP_LOGCONFIG(TAG, "  PIO: pio%d sm%d", this->panel_.claimed_pio_index, this->panel_.claimed_sm);
  if (this->is_failed()) ESP_LOGE(TAG, "  Setup FAILED");
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
