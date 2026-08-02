#include "gu_display.h"

#if defined(USE_RP2040) && defined(USE_DISPLAY)

#include "esphome/core/log.h"

namespace esphome {
namespace galactic_unicorn {

static const char *const TAG = "galactic_unicorn.display";

void GalacticUnicornDisplay::setup() {
  memset(this->rgb_, 0, BUFFER_LENGTH);
}

void GalacticUnicornDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->get_width_internal() || y < 0 || y >= this->get_height_internal()) return;
  size_t i = (y * GalacticUnicornPanel::WIDTH + x) * 3;
  this->rgb_[i + 0] = color.r;
  this->rgb_[i + 1] = color.g;
  this->rgb_[i + 2] = color.b;
}

void GalacticUnicornDisplay::update() {
  if (this->hub_ == nullptr || this->hub_->is_failed()) return;

  // Clear (unless the user disabled auto_clear_enabled), run the user's (or
  // generated) drawing lambda, then blit. The panel has no staging buffer:
  // set_pixel writes straight into the live DMA bitstream, so a torn frame
  // is the worst case, never a blank one.
  if (this->auto_clear_enabled_) {
    memset(this->rgb_, 0, BUFFER_LENGTH);
  }
  this->do_update_();

  for (int y = 0; y < GalacticUnicornPanel::HEIGHT; y++) {
    for (int x = 0; x < GalacticUnicornPanel::WIDTH; x++) {
      size_t i = (y * GalacticUnicornPanel::WIDTH + x) * 3;
      this->hub_->set_pixel(x, y, this->rgb_[i + 0], this->rgb_[i + 1], this->rgb_[i + 2]);
    }
  }
}

void GalacticUnicornDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "Galactic Unicorn Display:");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", this->get_width_internal(), this->get_height_internal());
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040 && USE_DISPLAY
