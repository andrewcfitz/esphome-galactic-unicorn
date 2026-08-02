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
  // now and last_draw_ms_ are both uint32_t, so unsigned subtraction handles
  // millis() rollover (~49.7 days) correctly on its own. The upper bound
  // below guards against a stalled or very slow first frame.
  float dt = (now - this->last_draw_ms_) / 1000.0f;
  this->last_draw_ms_ = now;
  if (dt > 1.0f)
    dt = 0.0f;

  const int panel_width = it.get_width();
  ScrollPositions pos = compute_scroll(width, panel_width, this->scroll_gap_, this->accumulator_);

  if (pos.scrolling) {
    this->accumulator_ =
        advance_accumulator(this->accumulator_, this->scroll_speed_, dt, width, this->scroll_gap_);
  }

#ifdef USE_LIGHT
  // When a light drives the colour and it's off (or dimmed to black), blank
  // the sign. The dt measurement and accumulator advance above still ran,
  // so the scroll position keeps ticking along underneath; turning the
  // light back on resumes the scroll where it would have been rather than
  // restarting it.
  if (this->light_ != nullptr && this->light_->is_dark())
    return;
  const Color &color = (this->light_ != nullptr) ? this->light_->current_color() : this->color_;
#else
  const Color &color = this->color_;
#endif

  // Vertically centre using the measured glyph height.
  const int y = (it.get_height() - height) / 2;

  // TOP_LEFT places the glyph origin at x, but width is the tight bounding
  // box; the visible ink actually starts at x + x_offset (the font's left
  // bearing), so subtract it to land the ink where compute_scroll intended.
  it.print(pos.primary - x_offset, y, this->font_, color, display::TextAlign::TOP_LEFT, this->value_.c_str());
  // The wrap-around copy only needs drawing once it could actually be
  // visible; skip the rasterisation work entirely while it is still off
  // the right edge of the panel.
  if (pos.has_secondary && pos.secondary < panel_width) {
    it.print(pos.secondary - x_offset, y, this->font_, color, display::TextAlign::TOP_LEFT, this->value_.c_str());
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
