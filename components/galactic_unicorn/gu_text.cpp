#include "esphome/core/defines.h"

#if defined(USE_RP2040) && defined(USE_DISPLAY) && defined(USE_TEXT)

#include "gu_text.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace galactic_unicorn {

static const char *const TAG = "galactic_unicorn.text";

void GalacticUnicornText::setup() {
  this->value_ = "";
  this->publish_state(this->value_);
}

void GalacticUnicornText::control(const std::string &value) {
  this->value_ = value;
  // A new message always starts from the beginning.
  this->accumulator_ = 0.0f;
  this->last_draw_ms_ = millis();
  this->publish_state(value);
}

void GalacticUnicornText::draw(display::Display &it) {
  if (this->font_ == nullptr || this->value_.empty())
    return;

  int width = 0, x_offset = 0, baseline = 0, height = 0;
  this->font_->measure(this->value_.c_str(), &width, &x_offset, &baseline, &height);

  const uint32_t now = millis();
  float dt = (now - this->last_draw_ms_) / 1000.0f;
  this->last_draw_ms_ = now;
  // Guard against the first frame and against clock jumps.
  if (dt < 0.0f || dt > 1.0f)
    dt = 0.0f;

  const int panel_width = it.get_width();
  ScrollPositions pos = compute_scroll(width, panel_width, this->scroll_gap_, this->accumulator_);

  if (pos.scrolling) {
    this->accumulator_ =
        advance_accumulator(this->accumulator_, this->scroll_speed_, dt, width, this->scroll_gap_);
  }

  // Vertically centre using the measured glyph height.
  const int y = (it.get_height() - height) / 2;

  it.print(pos.primary, y, this->font_, this->color_, display::TextAlign::TOP_LEFT, this->value_.c_str());
  if (pos.has_secondary) {
    it.print(pos.secondary, y, this->font_, this->color_, display::TextAlign::TOP_LEFT, this->value_.c_str());
  }
}

void GalacticUnicornText::dump_config() {
  LOG_TEXT("", "Galactic Unicorn Text", this);
  ESP_LOGCONFIG(TAG, "  Scroll speed: %.1f px/s", this->scroll_speed_);
  ESP_LOGCONFIG(TAG, "  Scroll gap: %d px", this->scroll_gap_);
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040 && USE_DISPLAY && USE_TEXT
