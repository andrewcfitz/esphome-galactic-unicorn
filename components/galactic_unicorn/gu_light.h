#pragma once

#include "esphome/core/defines.h"

// Compiled only when the rp2 platform and the light domain are both in
// play. Local external components compile every source file in their
// directory regardless of which sub-platform a given YAML uses (see
// gu_display.h for the full explanation), so without the USE_LIGHT guard a
// config that lacks `light:` fails to build.
#if defined(USE_RP2040) && defined(USE_LIGHT)

#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"

namespace esphome {
namespace galactic_unicorn {

// Drives no physical output; it only holds the colour/brightness/on-off
// state Home Assistant's light entity computes, so GalacticUnicornText can
// read it back out as a Color for drawing.
class GalacticUnicornLight : public light::LightOutput, public Component {
 public:
  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void write_state(light::LightState *state) override {
    state->current_values_as_rgb(&this->red_, &this->green_, &this->blue_);
  }

  Color current_color() const {
    return Color(static_cast<uint8_t>(this->red_ * 255), static_cast<uint8_t>(this->green_ * 255),
                 static_cast<uint8_t>(this->blue_ * 255));
  }

  // True when off or dimmed all the way down. Floats rarely land on exactly
  // 0.0 after the brightness/gamma math in current_values_as_rgb, so this
  // compares against a small epsilon rather than ==.
  bool is_dark() const {
    const float epsilon = 1e-3f;
    return this->red_ < epsilon && this->green_ < epsilon && this->blue_ < epsilon;
  }

 protected:
  float red_{0.0f};
  float green_{0.0f};
  float blue_{0.0f};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040 && USE_LIGHT
