#pragma once

#include <cmath>

namespace esphome {
namespace galactic_unicorn {

struct ScrollPositions {
  bool scrolling;      // false when the text fits and should sit still
  int primary;         // x of the first copy
  int secondary;       // x of the wrap-around copy, valid only if has_secondary
  bool has_secondary;
};

// One scroll period: the text plus the blank gap that follows it.
inline int scroll_period(int text_width, int gap) { return text_width + gap; }

// Where to draw the text, given how far the accumulator has advanced.
// Text that fits the panel is centred and ignores the accumulator entirely.
inline ScrollPositions compute_scroll(int text_width, int panel_width, int gap, float accumulator) {
  ScrollPositions out{};
  if (text_width <= panel_width) {
    out.scrolling = false;
    out.primary = (panel_width - text_width) / 2;
    out.has_secondary = false;
    out.secondary = 0;
    return out;
  }
  const int period = scroll_period(text_width, gap);
  out.scrolling = true;
  out.primary = -(int) std::fmod(accumulator, (float) period);
  out.secondary = out.primary + period;
  out.has_secondary = true;
  return out;
}

// Advance by speed * dt, folded back into a single period so the value
// cannot grow without bound over a long uptime.
inline float advance_accumulator(float accumulator, float speed_px_s, float dt_s,
                                 int text_width, int gap) {
  const float period = (float) scroll_period(text_width, gap);
  float next = accumulator + speed_px_s * dt_s;
  if (period > 0.0f) next = std::fmod(next, period);
  if (next < 0.0f) next += period;
  return next;
}

}  // namespace galactic_unicorn
}  // namespace esphome
