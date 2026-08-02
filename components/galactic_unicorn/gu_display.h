#pragma once

#include "esphome/core/defines.h"

// Compiled only when both the rp2 platform and the display domain are in
// play. Local external components compile every source file in their
// directory regardless of which sub-platform a given YAML uses, so without
// the USE_DISPLAY guard a config with `galactic_unicorn:` but no `display:`
// (e.g. the Task 2 smoke test) fails to build: esphome/components/display
// is never copied into the build tree because nothing in that config
// references it.
#if defined(USE_RP2040) && defined(USE_DISPLAY)

#include "esphome/components/display/display_buffer.h"
#include "gu_hub.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornDisplay : public display::DisplayBuffer {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_hub(GalacticUnicornHub *hub) { this->hub_ = hub; }

  int get_width_internal() override { return GalacticUnicornPanel::WIDTH; }
  int get_height_internal() override { return GalacticUnicornPanel::HEIGHT; }
  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  static const size_t BUFFER_LENGTH =
      GalacticUnicornPanel::WIDTH * GalacticUnicornPanel::HEIGHT * 3;

  GalacticUnicornHub *hub_{nullptr};
  uint8_t rgb_[BUFFER_LENGTH] = {0};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040 && USE_DISPLAY
