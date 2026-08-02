#pragma once

#include "esphome/core/defines.h"

// Compiled only when the rp2 platform, the display domain, and the text
// domain are all in play. Local external components compile every source
// file in their directory regardless of which sub-platform a given YAML
// uses (see gu_display.h for the full explanation), so without these
// guards a config that lacks `display:` and/or `text:` (e.g. the Task 2/3
// smoke test) fails to build.
#if defined(USE_RP2040) && defined(USE_DISPLAY) && defined(USE_TEXT)

#include <string>

#include "esphome/components/display/display.h"
#include "esphome/components/font/font.h"
#include "esphome/components/text/text.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"

#include "gu_scroll.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornText : public text::Text, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_font(font::Font *font) { this->font_ = font; }
  void set_color(Color color) { this->color_ = color; }
  void set_scroll_speed(float px_per_second) { this->scroll_speed_ = px_per_second; }
  void set_scroll_gap(int gap) { this->scroll_gap_ = gap; }

  // Called from the display's writer lambda once per frame.
  void draw(display::Display &it);

 protected:
  void control(const std::string &value) override;

  font::Font *font_{nullptr};
  Color color_{255, 255, 255};
  float scroll_speed_{20.0f};
  int scroll_gap_{12};

  std::string value_;
  float accumulator_{0.0f};
  uint32_t last_draw_ms_{0};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040 && USE_DISPLAY && USE_TEXT
