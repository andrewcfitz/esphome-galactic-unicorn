// Host-compiled unit tests for the scroll position maths.
#include "../components/galactic_unicorn/gu_scroll.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace esphome::galactic_unicorn;

static void test_short_text_is_centred_and_static() {
  // 20px of text on a 53px panel leaves 33px, so 16px of margin.
  ScrollPositions p = compute_scroll(20, 53, 12, 0.0f);
  assert(!p.scrolling);
  assert(p.primary == 16);
  assert(!p.has_secondary);

  // A non-zero accumulator must not move static text.
  ScrollPositions q = compute_scroll(20, 53, 12, 987.6f);
  assert(q.primary == 16);
  assert(!q.scrolling);
}

static void test_text_exactly_panel_width_is_static() {
  ScrollPositions p = compute_scroll(53, 53, 12, 0.0f);
  assert(!p.scrolling);
  assert(p.primary == 0);
}

static void test_long_text_starts_at_zero_and_moves_left() {
  ScrollPositions a = compute_scroll(100, 53, 12, 0.0f);
  assert(a.scrolling);
  assert(a.primary == 0);

  ScrollPositions b = compute_scroll(100, 53, 12, 25.0f);
  assert(b.primary == -25);
}

static void test_second_copy_is_exactly_one_period_right() {
  // period = text_width + gap = 112
  ScrollPositions p = compute_scroll(100, 53, 12, 40.0f);
  assert(p.has_secondary);
  assert(p.secondary - p.primary == 112);
}

static void test_wrap_has_no_discontinuity() {
  // Just before the wrap the primary copy sits at -111.
  ScrollPositions before = compute_scroll(100, 53, 12, 111.0f);
  assert(before.primary == -111);
  // At exactly one period the primary snaps back to 0, and the secondary
  // occupies where the primary just was plus one period, so the visible
  // pixels are unchanged.
  ScrollPositions after = compute_scroll(100, 53, 12, 112.0f);
  assert(after.primary == 0);
  assert(after.secondary == 112);
}

static void test_accumulator_advances_by_speed_times_dt() {
  float a = advance_accumulator(0.0f, 20.0f, 0.5f, 100, 12);
  assert(std::fabs(a - 10.0f) < 1e-4f);
}

static void test_accumulator_wraps_within_one_period() {
  // period is 112; advancing past it must fold back, not grow without bound.
  float a = advance_accumulator(110.0f, 20.0f, 0.5f, 100, 12);
  assert(a >= 0.0f && a < 112.0f);
  assert(std::fabs(a - 8.0f) < 1e-4f);
}

int main() {
  test_short_text_is_centred_and_static();
  test_text_exactly_panel_width_is_static();
  test_long_text_starts_at_zero_and_moves_left();
  test_second_copy_is_exactly_one_period_right();
  test_wrap_has_no_discontinuity();
  test_accumulator_advances_by_speed_times_dt();
  test_accumulator_wraps_within_one_period();
  printf("all scroll tests passed\n");
  return 0;
}
