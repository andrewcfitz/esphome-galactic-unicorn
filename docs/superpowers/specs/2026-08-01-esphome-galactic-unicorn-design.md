# ESPHome Galactic Unicorn Component

**Date:** 2026-08-01
**Status:** Approved, ready for implementation planning

## Goal

Let a Home Assistant user type into a text box and have that text appear, scrolling
if it does not fit, on a Pimoroni Galactic Unicorn LED matrix.

Delivered as a public ESPHome external component so the device is a first-class
ESPHome node: native API, OTA, logs, and Home Assistant auto-discovery.

## Background

### The hardware

The Galactic Unicorn is a 53x11 RGB LED matrix with a Raspberry Pi Pico W (RP2040)
soldered to the back. It is neither HUB75 nor an addressable strip. Pimoroni drive
it with a hand-written PIO program plus DMA:

| Function | GPIO |
|---|---|
| Column clock (PIO sideset) | 13 |
| Column data | 14 |
| Latch | 15 |
| Blank | 16 |
| Row select, bits 0 to 3 | 17, 18, 19, 20 |

One PIO state machine shifts column data and drives row select and blanking. Two
chained DMA channels feed pixel data from memory into the PIO FIFO, so refresh
costs the CPU almost nothing. Colour depth comes from binary-coded modulation: the
frame is stored as separate bit planes, each displayed for a power-of-two duration.
Pimoroni apply 14-bit gamma correction per channel.

The board also carries buttons, an I2S speaker, and a light sensor. All are out of
scope for v1.

### Why this needs new code

ESPHome has no driver for this panel and nobody appears to have published one. The
`rp2` platform builds on the arduino-pico framework, which sits on the pico-sdk, so
`hardware/pio.h` and `hardware/dma.h` are reachable. The built-in
`rp2040_pio_led_strip` component already includes both, which proves PIO programs
compile inside ESPHome on this platform.

Pimoroni's driver is MIT licensed, so porting it and republishing is permitted with
attribution.

## Architecture

Three layers with hard boundaries. The hardware layer is the risky part; isolating
it keeps everything above it ordinary code.

```
components/galactic_unicorn/
  __init__.py              # shared config schema and constants
  display.py               # display platform codegen
  text.py                  # text platform codegen
  galactic_unicorn.h/.cpp  # GalacticUnicornPanel: hardware only
  gu_pio.h                 # pre-assembled PIO program
  gu_display.h/.cpp        # GalacticUnicornDisplay: ESPHome DisplayBuffer
  gu_text.h/.cpp           # GalacticUnicornText: scroll state and rendering
  gu_scroll.h              # pure scroll math, host-testable
example/galactic-unicorn.yaml
tests/test_scroll.cpp
.github/workflows/ci.yml
README.md
LICENSE
```

### Layer 1: GalacticUnicornPanel

Pure hardware. Claims a PIO state machine and two DMA channels, drives GPIO 13 to
20, performs binary-coded modulation. A direct port of Pimoroni's driver with
copyright retained.

Public API:

- `bool init()` returns false if PIO or DMA resources cannot be claimed
- `void set_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)`
- `void set_brightness(float b)` clamped to 0.0 to 1.0
- `void clear()`
- `void update()` publishes the staged frame to the DMA bitstream

Knows nothing about ESPHome. Depends only on the pico-sdk.

The PIO program is assembled ahead of time with `pioasm` and the generated
instruction array committed as `gu_pio.h`. This is standard practice for shipped
components and avoids requiring `pioasm` in the ESPHome build environment.

Resource claiming must use `pio_claim_free_sm_and_add_program()` rather than
hardcoding `pio0`. See Risks.

### Layer 2: GalacticUnicornDisplay

Subclasses ESPHome's `display::DisplayBuffer` at 53x11.

- `draw_absolute_pixel_internal()` writes into a 53 * 11 * 3 = 1749 byte RGB888 buffer
- `update()` blits the buffer to the panel via `set_pixel()`, then calls `panel.update()`
- `get_width_internal()` returns 53, `get_height_internal()` returns 11

Subclassing DisplayBuffer rather than writing a bespoke text renderer means ESPHome's
own rendering stack supplies fonts, `it.printf()`, images, shapes, and lambda support
at no additional cost. For a public component this is the surface users will expect.

### Layer 3: GalacticUnicornText

An ESPHome `text` component, which Home Assistant discovers as a text entity.

Holds the current string and the scroll accumulator. On each display update it
measures the string with the configured font and draws it at the computed x position.

If the user supplies no display lambda, codegen emits one that calls
`id(text_id)->draw(it)`. If the user supplies their own lambda, they call
`id(text_id)->draw(it)` themselves wherever they want it. This keeps the common case
zero-effort without locking out custom layouts.

### Scroll behaviour

Specified precisely because it is the one piece with real logic, and it gets a unit
test.

Let `W` = 53 (panel width), `tw` = measured text width in pixels, `gap` =
`scroll_gap` in pixels, `v` = `scroll_speed` in pixels per second.

- If `tw <= W`: static. Draw one copy at `x = (W - tw) / 2`. No animation.
- If `tw > W`: let period `P = tw + gap`. An accumulator advances by `v * dt` each
  update, where `dt` is the measured seconds since the previous update, so apparent
  speed is independent of `update_interval`. Then `x0 = -fmod(accumulator, P)`.
  Draw a copy at `x0` and a second copy at `x0 + P`, so the wrap is seamless with no
  blank frame between repeats.
- The accumulator resets to 0 whenever the text changes, so a new message always
  starts from the beginning.

### Data flow

```
Home Assistant text box
  -> ESPHome native API
  -> GalacticUnicornText::control()   (store string, reset accumulator)
  -> GalacticUnicornDisplay::update() (polled, renders glyphs at current offset)
  -> GalacticUnicornPanel::update()   (stage frame)
  -> PIO + DMA                        (refresh, asynchronous, no CPU cost)
```

## Configuration surface

```yaml
external_components:
  - source: github://andrewcfitz/esphome-galactic-unicorn

font:
  - file: "gfonts://Roboto"
    id: sign_font
    size: 8

display:
  - platform: galactic_unicorn
    id: unicorn
    brightness: 0.5
    update_interval: 33ms

text:
  - platform: galactic_unicorn
    name: "Sign Text"
    display_id: unicorn
    font: sign_font
    color: [255, 180, 0]
    scroll_speed: 20    # pixels per second
    scroll_gap: 12      # pixels of blank before the message repeats
```

Options and defaults:

| Option | Platform | Default | Notes |
|---|---|---|---|
| `brightness` | display | `0.5` | 0.0 to 1.0, validated at config time |
| `update_interval` | display | `33ms` | roughly 30fps |
| `display_id` | text | required | which display to draw on |
| `font` | text | required | any ESPHome font |
| `color` | text | `[255, 255, 255]` | RGB |
| `scroll_speed` | text | `20` | pixels per second |
| `scroll_gap` | text | `12` | pixels |
| `mode` | text | `text` | standard ESPHome text option |

The GPIO pins are fixed by the hardware and are not configurable.

## Error handling

| Condition | Behaviour |
|---|---|
| PIO or DMA claim fails at setup | `ESP_LOGE` naming the resource, then `mark_failed()`. Do not hang, do not silently show nothing. |
| Brightness outside 0.0 to 1.0 | Rejected at config validation time, clamped at runtime |
| Empty string | Panel cleared, no animation |
| Glyph missing from the font | ESPHome's font handles it; must not crash or corrupt the width measurement |
| Text longer than `max_length` | Enforced by the stock ESPHome text component |
| `font` or `display_id` omitted | Config validation error at codegen time, not a runtime surprise |

## Testing

Hardware-in-the-loop CI is not realistic, so verification splits three ways.

1. **Compile CI.** GitHub Actions runs `esphome config` and `esphome compile` against
   `example/galactic-unicorn.yaml` on every push and pull request. This catches
   codegen errors, schema mistakes, and C++ compile failures, which is the bulk of
   the pain in an external component.
2. **Unit test.** `gu_scroll.h` holds the scroll position calculation as a pure
   function with no ESPHome and no pico-sdk dependency. `tests/test_scroll.cpp`
   compiles with host `g++` in CI and covers: text narrower than the panel centres
   and does not move; text wider than the panel advances leftward; the second copy
   sits exactly one period right of the first; the accumulator wraps without a
   position discontinuity; a text change resets the offset to zero.
3. **Manual hardware smoke test.** A checklist in the README covering first flash,
   panel lights up, WiFi still connects, Home Assistant discovers the text entity,
   short text is static and centred, long text scrolls and wraps seamlessly, and
   brightness responds.

## Risks

**PIO contention is the go/no-go risk.** The Pico W's CYW43439 wireless chip is
driven over PIO by the arduino-pico core. Pimoroni's driver hardcodes `pio0`. If
WiFi has already claimed the resources we need, the component cannot work as
designed. Mitigation is to claim resources dynamically with
`pio_claim_free_sm_and_add_program()`, and to verify this first, before any other
work. If both PIO blocks turn out to be contested, the project stops and we
reconsider the approach rather than discovering the problem late.

**ESPHome rp2040 display and API interaction.** There is a reported issue where
enabling a display on rp2040 disrupts the Home Assistant API connection. Our refresh
is DMA-asynchronous rather than a blocking bus transfer, so we are better positioned
than an SPI display, but this cannot be confirmed without hardware.

**Memory and CPU.** The framebuffer is 1749 bytes and the BCM bitstream a few
kilobytes, against 264KB of RAM. Not a concern, recorded only to note it was
considered.

## Out of scope for v1

The buttons, the light sensor, and the I2S speaker. Each is a natural follow-up and
each becomes straightforward once the panel works. None is on the path to a working
text box, so none is in v1.

Also excluded: configurable GPIO pins (fixed by the hardware), the Cosmic and Stellar
Unicorn variants, and the newer Pico 2 W board revision.

## Licensing

MIT, matching Pimoroni. `LICENSE` retains the original Pimoroni Ltd copyright
alongside ours, and every ported source file carries a header crediting
`pimoroni/pimoroni-pico` as its origin.

## Implementation phases

1. Panel driver and PIO contention verification. The go/no-go gate.
2. DisplayBuffer integration.
3. Text platform and scroll logic.
4. Repo polish: README, CI, example YAML, license, hardware smoke checklist.
