# ESPHome Galactic Unicorn Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A public ESPHome external component that drives the Pimoroni Galactic Unicorn 53x11 LED matrix and exposes a Home Assistant text entity whose contents scroll across the panel.

**Architecture:** Three layers with hard boundaries. `GalacticUnicornPanel` is pure pico-sdk (PIO + DMA), ported from Pimoroni and knowing nothing about ESPHome. A hub component owns the panel and is what YAML instantiates. On top, a `display` platform subclasses ESPHome's `DisplayBuffer`, and a `text` platform owns the string and scroll accumulator.

**Tech Stack:** ESPHome 2026.7.3, arduino-pico framework, pico-sdk (`hardware/pio.h`, `hardware/dma.h`), RP2040 on a Raspberry Pi Pico W. Python 3 for build-time asset generation. C++17.

## Global Constraints

Every task's requirements implicitly include this section.

- **ESPHome version is pinned to `2026.7.3`.** CI installs exactly this. Before writing any code against an ESPHome API (`DisplayBuffer`, `text::Text`, `font::Font`, codegen helpers), read the actual signatures from the installed package under `.venv/lib/python3*/site-packages/esphome/components/`. Do not write signatures from memory; ESPHome's internal APIs change between releases.
- **License is MIT.** Every file ported or derived from Pimoroni carries a header naming `pimoroni/pimoroni-pico` as its origin and retaining the Pimoroni Ltd copyright. `LICENSE` carries both copyrights.
- **No em dashes** in any file: code, comments, docs, commit messages. Use a colon, a period, a comma, or parentheses.
- **Panel geometry is fixed:** `WIDTH = 53`, `HEIGHT = 11`. GPIO pins are fixed by the hardware and are never configurable.
- **Target board** is `rpipicow` (RP2040). The Pico 2 W / RP2350 revision is explicitly out of scope.
- **Commit after every task.** Each task ends with a green check and a commit.

## Deviations from the spec

Recorded here so the implementer does not treat them as mistakes. All three were discovered by reading Pimoroni's actual source and are corrections to assumptions in `docs/superpowers/specs/2026-08-01-esphome-galactic-unicorn-design.md`.

1. **There is no frame staging.** The spec described `Panel::update()` as publishing a staged frame. In reality `set_pixel()` writes directly into the live DMA bitstream, and DMA replays that buffer continuously. There is no `update()` on the panel at all. The display platform's `update()` simply blits its RGB buffer through `set_pixel()`.
2. **A hub component is introduced.** The spec had the `display` platform own the panel. Instead a top-level `galactic_unicorn:` block owns it. This lets the PIO contention gate (Task 2) be verified on hardware before any DisplayBuffer code exists, and it matches how ESPHome models hardware shared by several platforms.
3. **Brightness is baked in at `set_pixel()` time,** not applied globally at refresh. Changing brightness therefore has no visible effect until every pixel is written again. Because the display redraws all 583 pixels every frame, this is harmless, but a brightness setter must not be expected to act on its own.

## File Structure

| Path | Responsibility |
|---|---|
| `components/galactic_unicorn/__init__.py` | Hub config schema and codegen |
| `components/galactic_unicorn/display.py` | `display` platform codegen |
| `components/galactic_unicorn/text.py` | `text` platform codegen |
| `components/galactic_unicorn/gu_gamma.h` | Generated 256-entry 14-bit gamma table |
| `components/galactic_unicorn/gu_pio.h` | Generated PIO program and default config |
| `components/galactic_unicorn/gu_panel.h/.cpp` | `GalacticUnicornPanel`: pico-sdk only |
| `components/galactic_unicorn/gu_hub.h/.cpp` | `GalacticUnicornHub`: ESPHome `Component` owning the panel |
| `components/galactic_unicorn/gu_display.h/.cpp` | `GalacticUnicornDisplay`: `DisplayBuffer` subclass |
| `components/galactic_unicorn/gu_scroll.h` | Pure scroll math, no ESPHome, no pico-sdk |
| `components/galactic_unicorn/gu_text.h/.cpp` | `GalacticUnicornText`: `text::Text` subclass |
| `tools/generate_assets.py` | Regenerates `gu_gamma.h` and `gu_pio.h` |
| `tools/galactic_unicorn.pio` | Vendored PIO source (Pimoroni, MIT) |
| `tests/test_assets.py` | Proves the generated headers are correct |
| `tests/test_scroll.cpp` | Host-compiled unit tests for scroll math |
| `example/galactic-unicorn.yaml` | The documented example, compiled by CI |
| `example/pio-smoke-test.yaml` | Minimal hub-only config for the contention gate |
| `.github/workflows/ci.yml` | Compile CI plus host unit tests |
| `README.md`, `LICENSE` | Docs and dual copyright |

---

### Task 1: Repo skeleton and generated hardware assets

The gamma table and the PIO program are both derived artifacts. Generating them from a script with a test that proves equivalence to Pimoroni's originals is safer than pasting 256 magic numbers and hoping.

**Files:**
- Create: `LICENSE`, `.gitignore`, `tools/galactic_unicorn.pio`, `tools/generate_assets.py`
- Create: `components/galactic_unicorn/gu_gamma.h`, `components/galactic_unicorn/gu_pio.h` (generated)
- Test: `tests/test_assets.py`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `gu_gamma.h` defines `static const uint16_t GALACTIC_UNICORN_GAMMA_14BIT[256];`
  - `gu_pio.h` defines `static const uint16_t galactic_unicorn_program_instructions[24];`, `static const struct pio_program galactic_unicorn_program;`, and `static inline pio_sm_config galactic_unicorn_program_get_default_config(uint offset);`

- [ ] **Step 1: Create the repo skeleton**

```bash
mkdir -p components/galactic_unicorn tools tests example .github/workflows
printf '__pycache__/\n*.pyc\n.venv/\n.esphome/\ntests/build/\n' > .gitignore
```

- [ ] **Step 2: Write LICENSE with both copyrights**

```
MIT License

Copyright (c) 2021 Pimoroni Ltd
Copyright (c) 2026 Andrew Fitz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 3: Vendor the PIO source**

Download Pimoroni's PIO program verbatim and keep it as the generator's input:

```bash
curl -sL -o tools/galactic_unicorn.pio \
  https://raw.githubusercontent.com/pimoroni/pimoroni-pico/main/libraries/galactic_unicorn/galactic_unicorn.pio
```

Verify it starts with `.program galactic_unicorn` and contains `.side_set 1 opt`.

- [ ] **Step 4: Write the failing test**

Create `tests/test_assets.py`. The expected PIO words below were produced by assembling Pimoroni's `.pio` with `adafruit_pioasm` and are the ground truth for this component.

```python
"""Prove the generated headers match their upstream sources exactly."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAMMA_H = ROOT / "components" / "galactic_unicorn" / "gu_gamma.h"
PIO_H = ROOT / "components" / "galactic_unicorn" / "gu_pio.h"

# Assembled from tools/galactic_unicorn.pio by adafruit_pioasm.
EXPECTED_PIO = [
    0x6048, 0x6008, 0x7121, 0xE004, 0x0026, 0xE005, 0xBA42, 0x7121,
    0xE004, 0x002B, 0xE005, 0xBA42, 0x7121, 0xE004, 0x0030, 0xE005,
    0x7A65, 0x0082, 0x6068, 0xE506, 0xE000, 0x6040, 0x0096, 0xE004,
]


def _ints(path, symbol):
    src = path.read_text()
    body = re.search(symbol + r"\[[^\]]*\]\s*=\s*\{(.*?)\}", src, re.S).group(1)
    return [int(tok, 0) for tok in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]


def regenerate():
    subprocess.run([sys.executable, str(ROOT / "tools" / "generate_assets.py")], check=True)


def test_gamma_matches_pimoroni_formula():
    regenerate()
    table = _ints(GAMMA_H, "GALACTIC_UNICORN_GAMMA_14BIT")
    assert len(table) == 256
    expected = [round(pow(i / 255, 2.2) * 16383) for i in range(256)]
    assert table == expected


def test_gamma_endpoints():
    table = _ints(GAMMA_H, "GALACTIC_UNICORN_GAMMA_14BIT")
    assert table[0] == 0
    assert table[255] == 16383
    # Spot values copied from pimoroni-pico common/pimoroni_common.hpp.
    assert table[:16] == [0, 0, 0, 1, 2, 3, 4, 6, 8, 10, 13, 16, 20, 23, 28, 32]


def test_pio_program_matches_upstream():
    regenerate()
    assert _ints(PIO_H, "galactic_unicorn_program_instructions") == EXPECTED_PIO


def test_pio_config_constants_present():
    src = PIO_H.read_text()
    # .side_set 1 opt assembles to a 2-bit sideset field with optional=true.
    assert "sm_config_set_sideset(&c, 2, true, false)" in src
    assert "sm_config_set_wrap(&c, offset + 0, offset + 23)" in src
```

- [ ] **Step 5: Run the test to verify it fails**

```bash
python3 -m venv .venv && .venv/bin/pip install -q pytest adafruit-circuitpython-pioasm
.venv/bin/python -m pytest tests/test_assets.py -v
```

Expected: FAIL. `tools/generate_assets.py` does not exist, so `regenerate()` raises `FileNotFoundError`.

- [ ] **Step 6: Write the generator**

Create `tools/generate_assets.py`:

```python
#!/usr/bin/env python3
"""Regenerate the gamma table and PIO program headers.

The gamma curve and the PIO program both originate in pimoroni/pimoroni-pico
(MIT, Copyright (c) 2021 Pimoroni Ltd). The gamma table is reproduced by
formula rather than copied; tests/test_assets.py proves the result is
identical to the upstream table.
"""
from pathlib import Path

import adafruit_pioasm

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "components" / "galactic_unicorn"

BANNER = """// Generated by tools/generate_assets.py. Do not edit by hand.
//
// Derived from pimoroni/pimoroni-pico (MIT, Copyright (c) 2021 Pimoroni Ltd).
"""


def write_gamma():
    values = [round(pow(i / 255, 2.2) * 16383) for i in range(256)]
    rows = []
    for i in range(0, 256, 16):
        rows.append("    " + ", ".join(str(v) for v in values[i:i + 16]) + ",")
    body = "\n".join(rows)
    (OUT / "gu_gamma.h").write_text(
        f"""{BANNER}
#pragma once

#include <cstdint>

// 14-bit gamma correction, curve exponent 2.2, output range 0 to 16383.
static const uint16_t GALACTIC_UNICORN_GAMMA_14BIT[256] = {{
{body}
}};
"""
    )


def write_pio():
    program = adafruit_pioasm.Program((ROOT / "tools" / "galactic_unicorn.pio").read_text())
    words = list(program.assembled)
    kwargs = program.pio_kwargs
    assert kwargs["sideset_pin_count"] == 1 and kwargs["sideset_enable"] is True
    wrap, wrap_target = kwargs["wrap"], kwargs["wrap_target"]

    rows = []
    for i in range(0, len(words), 8):
        rows.append("    " + ", ".join(f"0x{w:04X}" for w in words[i:i + 8]) + ",")
    body = "\n".join(rows)

    (OUT / "gu_pio.h").write_text(
        f"""{BANNER}
#pragma once

#include "hardware/pio.h"

static const uint16_t galactic_unicorn_program_instructions[] = {{
{body}
}};

static const struct pio_program galactic_unicorn_program = {{
    .instructions = galactic_unicorn_program_instructions,
    .length = {len(words)},
    .origin = -1,
}};

static inline pio_sm_config galactic_unicorn_program_get_default_config(uint offset) {{
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset + {wrap_target}, offset + {wrap});
  // ".side_set 1 opt" means a 2-bit sideset field, optional, not pindirs.
  sm_config_set_sideset(&c, 2, true, false);
  return c;
}}
"""
    )


if __name__ == "__main__":
    write_gamma()
    write_pio()
    print(f"wrote {OUT / 'gu_gamma.h'} and {OUT / 'gu_pio.h'}")
```

- [ ] **Step 7: Run the tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_assets.py -v
```

Expected: 4 passed. If `test_pio_program_matches_upstream` fails, `adafruit_pioasm` has changed its output; do not edit `EXPECTED_PIO` to match. Diff the two and understand the change first, because these 24 words are what the hardware executes.

- [ ] **Step 8: Commit**

```bash
git add LICENSE .gitignore tools components/galactic_unicorn tests/test_assets.py
git commit -m "feat: generate gamma table and PIO program from upstream sources"
```

---

### Task 2: Panel driver and PIO contention gate

**This is the go/no-go task.** The Pico W drives its CYW43439 wireless chip over PIO. Pimoroni hardcode `bitstream_pio = pio0` and claim a state machine with `pio_claim_unused_sm(pio0, true)`, which never looks at `pio1`. If WiFi has taken what we need and we cannot fall back, the project stops here.

**Files:**
- Create: `components/galactic_unicorn/gu_panel.h`, `components/galactic_unicorn/gu_panel.cpp`
- Create: `components/galactic_unicorn/gu_hub.h`, `components/galactic_unicorn/gu_hub.cpp`
- Create: `components/galactic_unicorn/__init__.py`
- Create: `example/pio-smoke-test.yaml`, `example/secrets.yaml.example`

**Interfaces:**
- Consumes: `GALACTIC_UNICORN_GAMMA_14BIT` from `gu_gamma.h`; `galactic_unicorn_program`, `galactic_unicorn_program_get_default_config` from `gu_pio.h`.
- Produces:
  - `class GalacticUnicornPanel` with `bool init()`, `void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)`, `void set_brightness(float)`, `float get_brightness()`, `void clear()`. Static constants `WIDTH = 53`, `HEIGHT = 11`.
  - `class GalacticUnicornHub : public esphome::Component` with `void setup()`, `float get_setup_priority()`, `void dump_config()`, and pass-throughs `set_pixel(...)`, `set_brightness(float)`, `get_brightness()`, `clear()`.

- [ ] **Step 1: Write the panel header**

Create `components/galactic_unicorn/gu_panel.h`:

```cpp
// Port of the Galactic Unicorn display driver from pimoroni/pimoroni-pico
// (MIT, Copyright (c) 2021 Pimoroni Ltd). Audio, buttons, and the light
// sensor are deliberately omitted.
#pragma once

#ifdef USE_RP2040

#include <cstdint>
#include "hardware/pio.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornPanel {
 public:
  static const int WIDTH = 53;
  static const int HEIGHT = 11;

  // Pin assignments are fixed by the hardware.
  static const uint8_t COLUMN_CLOCK = 13;
  static const uint8_t COLUMN_DATA = 14;
  static const uint8_t COLUMN_LATCH = 15;
  static const uint8_t COLUMN_BLANK = 16;
  static const uint8_t ROW_BIT_0 = 17;

  // Returns false if a PIO state machine or the two DMA channels could not
  // be claimed. Never aborts.
  bool init();

  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
  void set_brightness(float value);
  float get_brightness() const;
  void clear();

  // Populated by init() for dump_config().
  int claimed_pio_index{-1};
  int claimed_sm{-1};

 protected:
  static const uint32_t ROW_COUNT = HEIGHT;
  static const uint32_t BCD_FRAME_COUNT = 14;
  static const uint32_t BCD_FRAME_BYTES = 60;
  static const uint32_t ROW_BYTES = BCD_FRAME_COUNT * BCD_FRAME_BYTES;
  static const uint32_t BITSTREAM_LENGTH = ROW_COUNT * ROW_BYTES;  // 9240 bytes

  void configure_shift_registers_();

  PIO pio_{nullptr};
  uint sm_{0};
  uint offset_{0};
  int dma_channel_{-1};
  int dma_ctrl_channel_{-1};
  uint16_t brightness_{128};

  // Must be 4-byte aligned for the 32-bit DMA transfer.
  alignas(4) uint8_t bitstream_[BITSTREAM_LENGTH] = {0};
  const uint32_t bitstream_addr_ = (uint32_t) bitstream_;
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
```

- [ ] **Step 2: Write the panel implementation**

Create `components/galactic_unicorn/gu_panel.cpp`. Note the bitstream layout: byte 0 is the pixel count, byte 1 is the row select, bytes 2 to 54 are the 53 pixels, and bytes 56 to 59 are the BCD tick count. The comment block in Pimoroni's own source describes a different layout and is stale; the code below matches what their code actually does.

```cpp
// Port of the Galactic Unicorn display driver from pimoroni/pimoroni-pico
// (MIT, Copyright (c) 2021 Pimoroni Ltd).
#ifdef USE_RP2040

#include "gu_panel.h"
#include "gu_gamma.h"
#include "gu_pio.h"

#include <cmath>
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace esphome {
namespace galactic_unicorn {

bool GalacticUnicornPanel::init() {
  // Seed the per-frame constants: pixel count, row select, and BCD tick count.
  for (uint8_t row = 0; row < HEIGHT; row++) {
    for (uint8_t frame = 0; frame < BCD_FRAME_COUNT; frame++) {
      uint8_t *p = &this->bitstream_[row * ROW_BYTES + (BCD_FRAME_BYTES * frame)];
      p[0] = WIDTH - 1;
      p[1] = row;
      uint32_t bcd_ticks = (1u << frame);
      p[56] = (bcd_ticks & 0xff) >> 0;
      p[57] = (bcd_ticks & 0xff00) >> 8;
      p[58] = (bcd_ticks & 0xff0000) >> 16;
      p[59] = (bcd_ticks & 0xff000000) >> 24;
    }
  }

  gpio_init(COLUMN_CLOCK); gpio_set_dir(COLUMN_CLOCK, GPIO_OUT); gpio_put(COLUMN_CLOCK, false);
  gpio_init(COLUMN_DATA); gpio_set_dir(COLUMN_DATA, GPIO_OUT); gpio_put(COLUMN_DATA, false);
  gpio_init(COLUMN_LATCH); gpio_set_dir(COLUMN_LATCH, GPIO_OUT); gpio_put(COLUMN_LATCH, false);
  gpio_init(COLUMN_BLANK); gpio_set_dir(COLUMN_BLANK, GPIO_OUT); gpio_put(COLUMN_BLANK, true);

  // Park the row select on a non-visible row so setup does not flash.
  for (uint8_t pin = ROW_BIT_0; pin < ROW_BIT_0 + 4; pin++) {
    gpio_init(pin); gpio_set_dir(pin, GPIO_OUT); gpio_put(pin, true);
  }

  sleep_ms(100);
  this->configure_shift_registers_();

  // Claim a state machine on either PIO block. This is the single most
  // important deviation from Pimoroni's driver, which hardcodes pio0.
  //
  // arduino-pico ships pico-sdk 1.5.1, which has no
  // pio_claim_free_sm_and_add_program (that arrived in SDK 2.0), so the
  // cross-PIO search is done by hand. Check pio_can_add_program BEFORE
  // claiming a state machine, so a PIO with free SMs but no instruction
  // room does not leak a claimed SM.
  bool claimed = false;
  for (uint i = 0; i < NUM_PIOS && !claimed; i++) {
    PIO candidate = (i == 0) ? pio0 : pio1;
    if (!pio_can_add_program(candidate, &galactic_unicorn_program)) continue;
    int sm = pio_claim_unused_sm(candidate, false);
    if (sm < 0) continue;
    this->pio_ = candidate;
    this->sm_ = (uint) sm;
    this->offset_ = pio_add_program(candidate, &galactic_unicorn_program);
    claimed = true;
  }
  if (!claimed) return false;
  this->claimed_pio_index = pio_get_index(this->pio_);
  this->claimed_sm = (int) this->sm_;

  for (uint8_t pin = COLUMN_CLOCK; pin < COLUMN_CLOCK + 8; pin++) {
    pio_gpio_init(this->pio_, pin);
  }

  // Raise blank and the row bits before enabling outputs, to avoid a flash.
  const uint pins_to_set = 1 << COLUMN_BLANK | 0b1111 << ROW_BIT_0;
  pio_sm_set_pins_with_mask(this->pio_, this->sm_, pins_to_set, pins_to_set);
  pio_sm_set_consecutive_pindirs(this->pio_, this->sm_, COLUMN_CLOCK, 8, true);

  pio_sm_config c = galactic_unicorn_program_get_default_config(this->offset_);
  sm_config_set_out_shift(&c, true, true, 32);  // shift right, autopull, threshold 32
  sm_config_set_out_pins(&c, ROW_BIT_0, 4);
  sm_config_set_set_pins(&c, COLUMN_DATA, 3);
  sm_config_set_sideset_pins(&c, COLUMN_CLOCK);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  this->dma_channel_ = dma_claim_unused_channel(false);
  this->dma_ctrl_channel_ = dma_claim_unused_channel(false);
  if (this->dma_channel_ < 0 || this->dma_ctrl_channel_ < 0) {
    if (this->dma_channel_ >= 0) dma_channel_unclaim(this->dma_channel_);
    if (this->dma_ctrl_channel_ >= 0) dma_channel_unclaim(this->dma_ctrl_channel_);
    pio_sm_unclaim(this->pio_, this->sm_);
    return false;
  }

  // The control channel rewinds the data channel's read address, so the two
  // chain into each other and replay the bitstream forever.
  dma_channel_config ctrl_config = dma_channel_get_default_config(this->dma_ctrl_channel_);
  channel_config_set_transfer_data_size(&ctrl_config, DMA_SIZE_32);
  channel_config_set_read_increment(&ctrl_config, false);
  channel_config_set_write_increment(&ctrl_config, false);
  channel_config_set_chain_to(&ctrl_config, this->dma_channel_);
  dma_channel_configure(this->dma_ctrl_channel_, &ctrl_config,
                        &dma_hw->ch[this->dma_channel_].read_addr,
                        &this->bitstream_addr_, 1, false);

  dma_channel_config config = dma_channel_get_default_config(this->dma_channel_);
  channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
  channel_config_set_bswap(&config, false);
  channel_config_set_dreq(&config, pio_get_dreq(this->pio_, this->sm_, true));
  channel_config_set_chain_to(&config, this->dma_ctrl_channel_);
  dma_channel_configure(this->dma_channel_, &config, &this->pio_->txf[this->sm_],
                        nullptr, BITSTREAM_LENGTH / 4, false);

  pio_sm_init(this->pio_, this->sm_, this->offset_, &c);
  pio_sm_set_enabled(this->pio_, this->sm_, true);
  dma_start_channel_mask(1u << this->dma_ctrl_channel_);
  return true;
}

void GalacticUnicornPanel::configure_shift_registers_() {
  // Select full output current in the column driver chips' register 2.
  const uint16_t reg1 = 0b1111111111001110;
  for (int j = 0; j < 9; j++) {
    for (int i = 0; i < 16; i++) {
      gpio_put(COLUMN_DATA, (reg1 & (1U << (15 - i))) != 0);
      sleep_us(10);
      gpio_put(COLUMN_CLOCK, true);
      sleep_us(10);
      gpio_put(COLUMN_CLOCK, false);
    }
  }
  // The tenth chip latches partway through, which is what commits the value.
  for (int i = 0; i < 16; i++) {
    gpio_put(COLUMN_DATA, (reg1 & (1U << (15 - i))) != 0);
    sleep_us(10);
    gpio_put(COLUMN_CLOCK, true);
    sleep_us(10);
    gpio_put(COLUMN_CLOCK, false);
    if (i == 4) gpio_put(COLUMN_LATCH, true);
  }
  gpio_put(COLUMN_LATCH, false);

  // Reassert blank; the sequence above leaves a faint glow otherwise.
  gpio_put(COLUMN_BLANK, false);
  sleep_us(10);
  gpio_put(COLUMN_BLANK, true);
}

void GalacticUnicornPanel::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;

  // The panel is wired with both axes reversed.
  x = (WIDTH - 1) - x;
  y = (HEIGHT - 1) - y;

  r = (r * this->brightness_) >> 8;
  g = (g * this->brightness_) >> 8;
  b = (b * this->brightness_) >> 8;

  uint16_t gamma_r = GALACTIC_UNICORN_GAMMA_14BIT[r];
  uint16_t gamma_g = GALACTIC_UNICORN_GAMMA_14BIT[g];
  uint16_t gamma_b = GALACTIC_UNICORN_GAMMA_14BIT[b];

  // One bit of each channel lands in each of the 14 BCD frames.
  for (uint8_t frame = 0; frame < BCD_FRAME_COUNT; frame++) {
    uint8_t *p = &this->bitstream_[y * ROW_BYTES + (BCD_FRAME_BYTES * frame) + 2 + x];
    *p = ((gamma_b & 0b1) << 0) | ((gamma_g & 0b1) << 1) | ((gamma_r & 0b1) << 2);
    gamma_r >>= 1;
    gamma_g >>= 1;
    gamma_b >>= 1;
  }
}

void GalacticUnicornPanel::set_brightness(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  // 256 is full scale because it is applied as (channel * brightness) >> 8.
  this->brightness_ = (uint16_t) floorf(value * 256.0f);
}

float GalacticUnicornPanel::get_brightness() const { return this->brightness_ / 256.0f; }

void GalacticUnicornPanel::clear() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) this->set_pixel(x, y, 0, 0, 0);
  }
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
```

- [ ] **Step 3: Write the hub**

Create `components/galactic_unicorn/gu_hub.h`:

```cpp
#pragma once

#ifdef USE_RP2040

#include "esphome/core/component.h"
#include "gu_panel.h"

namespace esphome {
namespace galactic_unicorn {

class GalacticUnicornHub : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_initial_brightness(float b) { this->initial_brightness_ = b; }

  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    this->panel_.set_pixel(x, y, r, g, b);
  }
  void set_brightness(float b) { this->panel_.set_brightness(b); }
  float get_brightness() const { return this->panel_.get_brightness(); }
  void clear() { this->panel_.clear(); }

 protected:
  GalacticUnicornPanel panel_;
  float initial_brightness_{0.5f};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
```

Create `components/galactic_unicorn/gu_hub.cpp`:

```cpp
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
```

- [ ] **Step 4: Write the hub codegen**

Create `components/galactic_unicorn/__init__.py`:

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@andrewcfitz"]

CONF_GALACTIC_UNICORN_ID = "galactic_unicorn_id"
CONF_BRIGHTNESS = "brightness"

galactic_unicorn_ns = cg.esphome_ns.namespace("galactic_unicorn")
GalacticUnicornHub = galactic_unicorn_ns.class_("GalacticUnicornHub", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GalacticUnicornHub),
            cv.Optional(CONF_BRIGHTNESS, default=0.5): cv.float_range(min=0.0, max=1.0),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(["rp2040"]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_initial_brightness(config[CONF_BRIGHTNESS]))
```

- [ ] **Step 5: Write the smoke test config**

Create `example/secrets.yaml.example`:

```yaml
wifi_ssid: "your-ssid"
wifi_password: "your-password"
api_encryption_key: "generate-with-esphome-and-paste-here"
```

Create `example/pio-smoke-test.yaml`. This exists to answer one question: does the panel light up while WiFi is connected?

```yaml
# Minimal config that proves the PIO/DMA driver coexists with WiFi.
# Expect: the panel fills solid red, and the device stays online.
esphome:
  name: galactic-unicorn-smoke
  friendly_name: Galactic Unicorn Smoke Test
  on_boot:
    priority: -100
    then:
      - lambda: |-
          for (int y = 0; y < 11; y++) {
            for (int x = 0; x < 53; x++) {
              id(unicorn)->set_pixel(x, y, 255, 0, 0);
            }
          }

external_components:
  - source:
      type: local
      path: ../components

rp2040:
  board: rpipicow

logger:

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

galactic_unicorn:
  id: unicorn
  brightness: 0.4
```

- [ ] **Step 6: Verify it compiles**

```bash
.venv/bin/pip install -q esphome==2026.7.3
cp example/secrets.yaml.example example/secrets.yaml   # fill in real values
.venv/bin/esphome compile example/pio-smoke-test.yaml
```

Expected: a successful build producing a `.uf2`. Fix compile errors before going near the hardware.

- [ ] **Step 7: THE GATE. Flash and observe**

```bash
.venv/bin/esphome run example/pio-smoke-test.yaml
```

Hold BOOTSEL while plugging in the board for the first flash, then copy the `.uf2` to the mounted `RPI-RP2` drive. Subsequent flashes go over OTA.

Check all three, in this order:

1. The panel shows solid red across all 53x11 pixels.
2. `esphome logs example/pio-smoke-test.yaml` shows WiFi connected and an IP address.
3. The `dump_config` line reports which PIO block was claimed, for example `PIO: pio1 sm0`.

**If the panel lights and WiFi connects, the gate is passed. Continue.**

**If setup logs "Could not claim a PIO state machine", stop.** Both PIO blocks are exhausted. Do not continue to Task 3. Report which resources were taken and re-open the design question, because the spec's approach does not survive this.

**If the panel lights but WiFi drops,** the resources coexist but something is timing-sensitive. Note it and continue; Task 3 changes the access pattern substantially and may resolve it.

- [ ] **Step 8: Commit**

```bash
git add components/galactic_unicorn example/pio-smoke-test.yaml example/secrets.yaml.example
git commit -m "feat: add Galactic Unicorn PIO/DMA panel driver and hub component"
```

---

### Task 3: Display platform

**Files:**
- Create: `components/galactic_unicorn/display.py`, `components/galactic_unicorn/gu_display.h`, `components/galactic_unicorn/gu_display.cpp`
- Create: `example/galactic-unicorn.yaml`

**Interfaces:**
- Consumes: `GalacticUnicornHub::set_pixel(int, int, uint8_t, uint8_t, uint8_t)` and `GalacticUnicornHub::clear()`.
- Produces: `class GalacticUnicornDisplay : public display::DisplayBuffer` with `void update() override`, `void setup() override`, `void set_hub(GalacticUnicornHub *)`, `int get_width_internal() override`, `int get_height_internal() override`, `display::DisplayType get_display_type() override`.

- [ ] **Step 1: Read the real DisplayBuffer API**

Do this before writing code. Signatures have changed across releases and guessing wastes a compile cycle.

```bash
sed -n '1,140p' .venv/lib/python3*/site-packages/esphome/components/display/display_buffer.h
grep -n "class Display\b\|draw_absolute_pixel_internal\|get_display_type\|init_internal_" \
  .venv/lib/python3*/site-packages/esphome/components/display/display.h
```

Confirm the exact spelling of `draw_absolute_pixel_internal`, `get_display_type`, and `DisplayType::DISPLAY_TYPE_COLOR`, then adjust the code below if they differ.

- [ ] **Step 2: Write the display header**

Create `components/galactic_unicorn/gu_display.h`:

```cpp
#pragma once

#ifdef USE_RP2040

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
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  static const size_t BUFFER_LENGTH =
      GalacticUnicornPanel::WIDTH * GalacticUnicornPanel::HEIGHT * 3;

  GalacticUnicornHub *hub_{nullptr};
  uint8_t rgb_[BUFFER_LENGTH] = {0};
};

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
```

- [ ] **Step 3: Write the display implementation**

Create `components/galactic_unicorn/gu_display.cpp`:

```cpp
#ifdef USE_RP2040

#include "gu_display.h"
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

  // Clear, run the user's (or generated) drawing lambda, then blit. The panel
  // has no staging buffer: set_pixel writes straight into the live DMA
  // bitstream, so a torn frame is the worst case, never a blank one.
  memset(this->rgb_, 0, BUFFER_LENGTH);
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

#endif  // USE_RP2040
```

- [ ] **Step 4: Write the display codegen**

Create `components/galactic_unicorn/display.py`:

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_ID, CONF_LAMBDA

from . import (
    CONF_GALACTIC_UNICORN_ID,
    GalacticUnicornHub,
    galactic_unicorn_ns,
)

DEPENDENCIES = ["galactic_unicorn"]

GalacticUnicornDisplay = galactic_unicorn_ns.class_(
    "GalacticUnicornDisplay", display.DisplayBuffer
)

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GalacticUnicornDisplay),
        cv.GenerateID(CONF_GALACTIC_UNICORN_ID): cv.use_id(GalacticUnicornHub),
    }
).extend(cv.polling_component_schema("33ms"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_GALACTIC_UNICORN_ID])
    cg.add(var.set_hub(hub))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
```

- [ ] **Step 5: Write the example config**

Create `example/galactic-unicorn.yaml`. This is the file CI compiles, so it must always be valid.

```yaml
esphome:
  name: galactic-unicorn
  friendly_name: Galactic Unicorn

external_components:
  - source:
      type: local
      path: ../components

rp2040:
  board: rpipicow

logger:

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

font:
  - file: "gfonts://Roboto"
    id: sign_font
    size: 8

galactic_unicorn:
  id: unicorn
  brightness: 0.5

display:
  - platform: galactic_unicorn
    id: unicorn_display
    update_interval: 33ms
    lambda: |-
      it.print(0, 0, id(sign_font), Color(255, 180, 0), "hi");
```

- [ ] **Step 6: Compile and verify on hardware**

```bash
.venv/bin/esphome compile example/galactic-unicorn.yaml
.venv/bin/esphome run example/galactic-unicorn.yaml
```

Expected: the word `hi` appears in amber at the top left of the panel, and the device stays connected. If text appears mirrored or upside down, the coordinate flip in `set_pixel` is being applied twice; the flip belongs only in the panel.

- [ ] **Step 7: Commit**

```bash
git add components/galactic_unicorn/display.py components/galactic_unicorn/gu_display.* example/galactic-unicorn.yaml
git commit -m "feat: add display platform backed by ESPHome DisplayBuffer"
```

---

### Task 4: Scroll math

Pure arithmetic with no ESPHome and no pico-sdk dependency, so it compiles and runs on the host. This is the only logic in the project worth testing in isolation, and getting the wrap seamless is fiddlier than it looks.

**Files:**
- Create: `components/galactic_unicorn/gu_scroll.h`
- Test: `tests/test_scroll.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct ScrollPositions { bool scrolling; int primary; int secondary; bool has_secondary; };`
  - `ScrollPositions compute_scroll(int text_width, int panel_width, int gap, float accumulator);`
  - `float advance_accumulator(float accumulator, float speed_px_s, float dt_s, int text_width, int gap);`

- [ ] **Step 1: Write the failing test**

Create `tests/test_scroll.cpp`. No test framework: plain asserts keep CI dependency-free.

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
mkdir -p tests/build
g++ -std=c++17 -Wall -Wextra -o tests/build/test_scroll tests/test_scroll.cpp && ./tests/build/test_scroll
```

Expected: FAIL at compile time with `gu_scroll.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

Create `components/galactic_unicorn/gu_scroll.h`:

```cpp
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
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
g++ -std=c++17 -Wall -Wextra -o tests/build/test_scroll tests/test_scroll.cpp && ./tests/build/test_scroll
```

Expected: `all scroll tests passed`, exit code 0, and no compiler warnings.

- [ ] **Step 5: Commit**

```bash
git add components/galactic_unicorn/gu_scroll.h tests/test_scroll.cpp
git commit -m "feat: add scroll position maths with host unit tests"
```

---

### Task 5: Text platform

**Files:**
- Create: `components/galactic_unicorn/text.py`, `components/galactic_unicorn/gu_text.h`, `components/galactic_unicorn/gu_text.cpp`
- Modify: `example/galactic-unicorn.yaml` (replace the hardcoded lambda with the text entity)

**Interfaces:**
- Consumes: `compute_scroll`, `advance_accumulator` from `gu_scroll.h`; `GalacticUnicornDisplay`.
- Produces: `class GalacticUnicornText : public text::Text, public Component` with `void draw(display::Display &it)` and `void control(const std::string &value) override`.

- [ ] **Step 1: Read the real text and font APIs**

```bash
sed -n '1,80p' .venv/lib/python3*/site-packages/esphome/components/text/text.h
grep -n "void measure\|int measure\|class Font" .venv/lib/python3*/site-packages/esphome/components/font/font.h
grep -n "def text_schema\|TEXT_SCHEMA\|async def new_text\|async def register_text" \
  .venv/lib/python3*/site-packages/esphome/components/text/__init__.py
```

Confirm the exact signature of `Font::measure(...)` and whether the codegen helper is `text.new_text` or `text.register_text`, then adjust below to match.

- [ ] **Step 2: Write the text header**

Create `components/galactic_unicorn/gu_text.h`:

```cpp
#pragma once

#ifdef USE_RP2040

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

#endif  // USE_RP2040
```

- [ ] **Step 3: Write the text implementation**

Create `components/galactic_unicorn/gu_text.cpp`:

```cpp
#ifdef USE_RP2040

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
  if (this->font_ == nullptr || this->value_.empty()) return;

  int width = 0, x_offset = 0, baseline = 0, height = 0;
  this->font_->measure(this->value_.c_str(), &width, &x_offset, &baseline, &height);

  const uint32_t now = millis();
  float dt = (now - this->last_draw_ms_) / 1000.0f;
  this->last_draw_ms_ = now;
  // Guard against the first frame and against clock jumps.
  if (dt < 0.0f || dt > 1.0f) dt = 0.0f;

  const int panel_width = it.get_width();
  ScrollPositions pos = compute_scroll(width, panel_width, this->scroll_gap_, this->accumulator_);

  if (pos.scrolling) {
    this->accumulator_ = advance_accumulator(this->accumulator_, this->scroll_speed_, dt,
                                             width, this->scroll_gap_);
  }

  // Vertically centre using the measured glyph height.
  const int y = (it.get_height() - height) / 2;

  it.print(pos.primary, y, this->font_, this->color_, display::TextAlign::TOP_LEFT,
           this->value_.c_str());
  if (pos.has_secondary) {
    it.print(pos.secondary, y, this->font_, this->color_, display::TextAlign::TOP_LEFT,
             this->value_.c_str());
  }
}

void GalacticUnicornText::dump_config() {
  LOG_TEXT("", "Galactic Unicorn Text", this);
  ESP_LOGCONFIG(TAG, "  Scroll speed: %.1f px/s", this->scroll_speed_);
  ESP_LOGCONFIG(TAG, "  Scroll gap: %d px", this->scroll_gap_);
}

}  // namespace galactic_unicorn
}  // namespace esphome

#endif  // USE_RP2040
```

- [ ] **Step 4: Write the text codegen**

Create `components/galactic_unicorn/text.py`:

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, text
from esphome.const import CONF_COLOR, CONF_FONT, CONF_ID

from . import galactic_unicorn_ns

DEPENDENCIES = ["galactic_unicorn", "display"]

CONF_SCROLL_SPEED = "scroll_speed"
CONF_SCROLL_GAP = "scroll_gap"

GalacticUnicornText = galactic_unicorn_ns.class_(
    "GalacticUnicornText", text.Text, cg.Component
)


def _color(value):
    value = cv.ensure_list(cv.int_range(min=0, max=255))(value)
    if len(value) != 3:
        raise cv.Invalid("color must be a list of exactly three values: [r, g, b]")
    return value


CONFIG_SCHEMA = (
    text.text_schema(GalacticUnicornText)
    .extend(
        {
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_COLOR, default=[255, 255, 255]): _color,
            cv.Optional(CONF_SCROLL_SPEED, default=20.0): cv.float_range(min=0.0, max=500.0),
            cv.Optional(CONF_SCROLL_GAP, default=12): cv.int_range(min=0, max=200),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text.new_text(config)
    await cg.register_component(var, config)

    fnt = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(fnt))

    r, g, b = config[CONF_COLOR]
    cg.add(var.set_color(cg.RawExpression(f"esphome::Color({r}, {g}, {b})")))
    cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
    cg.add(var.set_scroll_gap(config[CONF_SCROLL_GAP]))
```

- [ ] **Step 5: Wire the text into the example config**

Replace the `display:` and add a `text:` block in `example/galactic-unicorn.yaml`:

```yaml
display:
  - platform: galactic_unicorn
    id: unicorn_display
    update_interval: 33ms
    lambda: |-
      id(sign_text)->draw(it);

text:
  - platform: galactic_unicorn
    id: sign_text
    name: "Sign Text"
    font: sign_font
    color: [255, 180, 0]
    scroll_speed: 20
    scroll_gap: 12
```

- [ ] **Step 6: Compile, flash, and verify end to end**

```bash
.venv/bin/esphome compile example/galactic-unicorn.yaml
.venv/bin/esphome run example/galactic-unicorn.yaml
```

Verify each of these on the real device:

1. Home Assistant shows a `text.galactic_unicorn_sign_text` entity.
2. Typing `hello` leaves it static and horizontally centred.
3. Typing a sentence long enough to overflow makes it scroll leftward.
4. The scroll wraps with the configured gap and no blank frame or stutter.
5. Changing the text mid-scroll restarts it from the beginning.
6. Clearing the text blanks the panel.

- [ ] **Step 7: Commit**

```bash
git add components/galactic_unicorn/text.py components/galactic_unicorn/gu_text.* example/galactic-unicorn.yaml
git commit -m "feat: add scrolling text platform exposed as a Home Assistant text entity"
```

---

### Task 6: CI, documentation, and release polish

**Files:**
- Create: `.github/workflows/ci.yml`, `README.md`
- Create: `example/secrets.yaml` is NOT committed; CI generates its own.

**Interfaces:**
- Consumes: everything above.
- Produces: a green CI badge and a repo someone else can actually use.

- [ ] **Step 1: Write the CI workflow**

Create `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  unit-tests:
    name: Host unit tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - name: Install asset tooling
        run: pip install pytest adafruit-circuitpython-pioasm
      - name: Verify generated assets match upstream
        run: python -m pytest tests/test_assets.py -v
      - name: Build and run scroll tests
        run: |
          mkdir -p tests/build
          g++ -std=c++17 -Wall -Wextra -Werror -o tests/build/test_scroll tests/test_scroll.cpp
          ./tests/build/test_scroll

  compile:
    name: ESPHome compile
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - name: Install ESPHome
        run: pip install esphome==2026.7.3
      - name: Generate CI secrets
        run: |
          cat > example/secrets.yaml <<'EOF'
          wifi_ssid: "ci-ssid"
          wifi_password: "ci-password-placeholder"
          api_encryption_key: "bXlfc2VjdXJlX2tleV9mb3JfY2lfb25seV8xMjM0NTY="
          EOF
      - name: Validate config
        run: esphome config example/galactic-unicorn.yaml
      - name: Compile firmware
        run: esphome compile example/galactic-unicorn.yaml
      - name: Compile smoke test config
        run: esphome compile example/pio-smoke-test.yaml
```

- [ ] **Step 2: Verify CI passes locally before pushing**

```bash
.venv/bin/python -m pytest tests/test_assets.py -v
g++ -std=c++17 -Wall -Wextra -Werror -o tests/build/test_scroll tests/test_scroll.cpp && ./tests/build/test_scroll
.venv/bin/esphome config example/galactic-unicorn.yaml
```

Expected: all three succeed. `-Werror` is on in CI, so fix every warning now rather than debugging a red build later.

- [ ] **Step 3: Write the README**

Create `README.md` covering, in this order:

1. One-line description and a photo placeholder.
2. What it supports: Galactic Unicorn 53x11, original Pico W / RP2040 revision only. State plainly that the Pico 2 W revision, the Cosmic and Stellar variants, and the buttons, light sensor, and speaker are not supported.
3. Quick start: the full contents of `example/galactic-unicorn.yaml`, plus how to generate an API key and create `secrets.yaml`.
4. Configuration reference: a table of every option on `galactic_unicorn:`, `display:`, and `text:` with types and defaults, matching what the schemas actually accept.
5. How to draw your own content: that `display:` is a normal ESPHome `DisplayBuffer`, so `it.print`, `it.printf`, `it.image`, and shapes all work, and that `id(sign_text)->draw(it)` can be combined with other drawing calls in one lambda.
6. **Hardware smoke test checklist**, copied from Task 5 Step 6 plus the three checks in Task 2 Step 7.
7. Troubleshooting: what `Could not claim a PIO state machine` means, and that brightness changes only take effect as pixels are redrawn.
8. Credits: the driver is a port of `pimoroni/pimoroni-pico`, MIT, and a link to it. License section naming both copyright holders.

- [ ] **Step 4: Regenerate assets and confirm the tree is clean**

```bash
.venv/bin/python tools/generate_assets.py
git status --porcelain
```

Expected: no output. If the generated headers differ from what is committed, commit the regenerated versions.

- [ ] **Step 5: Commit and push**

```bash
git add .github README.md components tools tests example
git commit -m "feat: add CI, documentation, and hardware smoke test checklist"
git push
```

- [ ] **Step 6: Confirm CI is green**

```bash
gh run watch
```

Expected: both jobs pass. Do not consider the project done until the badge is green.

---

## Self-Review

**Spec coverage.** Every section of the spec maps to a task: the panel driver and contention gate to Task 2, DisplayBuffer integration to Task 3, scroll behaviour to Tasks 4 and 5, the config surface to Tasks 2, 3, and 5, error handling to Task 2 Step 3 (PIO failure), Task 3 Step 3 (failed hub guard), Task 5 Step 3 (empty string, clock jumps) and the schema validators, testing to Tasks 1, 4, and 6, and licensing to Task 1 Step 2. The three spec deviations are recorded above rather than silently applied.

**Placeholders.** None. Every code step carries the real content, and the two hardware-dependent steps (Task 2 Step 7, Task 5 Step 6) list explicit observable pass criteria rather than "check it works".

**Type consistency.** `set_pixel(int, int, uint8_t, uint8_t, uint8_t)` is spelled identically in the panel, the hub, and the display. `compute_scroll` and `advance_accumulator` are declared in Task 4's Interfaces block and consumed with matching argument order in Task 5. `GALACTIC_UNICORN_GAMMA_14BIT` is produced in Task 1 and consumed in Task 2. `galactic_unicorn_ns` is defined in Task 2's `__init__.py` and imported by both `display.py` and `text.py`.

**Known soft spots**, called out so the implementer treats them as verification points rather than gospel: the ESPHome-facing signatures in Tasks 3 and 5 (`DisplayBuffer`, `text.new_text`, `Font::measure`, `display::TextAlign`) are written from the stable public API but must be checked against 2026.7.3, which is why both tasks open with a step that reads the installed source.
