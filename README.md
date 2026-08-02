# ESPHome Galactic Unicorn

[![CI](https://github.com/andrewcfitz/esphome-galactic-unicorn/actions/workflows/ci.yml/badge.svg)](https://github.com/andrewcfitz/esphome-galactic-unicorn/actions/workflows/ci.yml)

An ESPHome external component that drives the [Pimoroni Galactic Unicorn](https://shop.pimoroni.com/products/galactic-unicorn) 53x11 RGB LED matrix and exposes a scrolling text sign to Home Assistant.

*(photo of the panel running goes here)*

> **Status: not yet hardware-verified.** This component compiles and passes CI (host unit tests, config validation, a full firmware build), but nothing in this repo has run on a physical Galactic Unicorn yet. The [Hardware smoke test checklist](#hardware-smoke-test-checklist) below is exactly the verification that's still outstanding. Treat it as unverified until someone checks every item on real hardware.

## What this supports

- Pimoroni **Galactic Unicorn**, 53x11 RGB LED matrix.
- The **original Pico W / RP2040** revision of the board only.

**Not supported:**

- The newer **Pico 2 W / RP2350** revision of the Galactic Unicorn.
- The **Cosmic Unicorn** and **Stellar Unicorn** variants.
- The board's buttons, light sensor, and I2S speaker. This component only drives the LED matrix.

The panel's GPIO pins are fixed by the hardware and are not configurable in YAML. This component claims them at startup; nothing in ESPHome stops you from also wiring an `i2c:`, `spi:`, or `GPIOPin` config onto the same pins, and doing so causes silent corruption of the panel output rather than a config-time error. **Do not use any of the following pins for anything else:**

| Function | GPIO |
|---|---|
| Column clock (PIO sideset) | 13 |
| Column data | 14 |
| Latch | 15 |
| Blank | 16 |
| Row select, bits 0 to 3 | 17, 18, 19, 20 |

## Quick start

This section uses the `github://` external component source, which works from any directory: you don't need a clone of this repo, just an ESPHome install. (If you're running the smoke-test checklist below, or otherwise working from a clone of this repo, see that section instead: the example configs there use a local path and need one extra step.)

Generate an API encryption key (any base64-encoded 32-byte string works; ESPHome can generate one for you when you first run `esphome config` on a fresh device, or you can run `esphome secrets` tooling of your choice). Then create `secrets.yaml` next to your config:

```yaml
wifi_ssid: "your-ssid"
wifi_password: "your-password"
api_encryption_key: "generate-with-esphome-and-paste-here"
```

Full working config:

```yaml
esphome:
  name: galactic-unicorn
  friendly_name: Galactic Unicorn

external_components:
  - source: github://andrewcfitz/esphome-galactic-unicorn

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

For the first flash, build with `esphome compile <your-config>.yaml`, hold BOOTSEL while plugging in the board, then copy the produced `.uf2` file to the `RPI-RP2` drive that appears. It lands at `<your-config-dir>/.esphome/build/<node-name>/.pioenvs/<node-name>/firmware.uf2`, where `<node-name>` is the `esphome.name` value in your config (for the config above, `galactic-unicorn`). Every flash after that goes over OTA, so `esphome run` is enough.

## Configuration reference

### `galactic_unicorn:`

One hub per device. Everything else (`display:`, `text:`) attaches to it.

| Option | Type | Default | Notes |
|---|---|---|---|
| `id` | ID | auto-generated | Reference this hub from `display:` when more than one hub exists. |
| `brightness` | float, 0.0-1.0 | `0.5` | Initial panel brightness. |

### `display:` (`platform: galactic_unicorn`)

A normal ESPHome `DisplayBuffer` platform.

| Option | Type | Default | Notes |
|---|---|---|---|
| `id` | ID | auto-generated | |
| `galactic_unicorn_id` | ID | the only configured hub | Only required if you define more than one `galactic_unicorn:` hub. |
| `update_interval` | time | `33ms` | Standard ESPHome polling-component interval. |
| `lambda` | lambda | none | Standard ESPHome display lambda; see below. |

### `text:` (`platform: galactic_unicorn`)

Exposes a Home Assistant `text` entity backed by the panel's built-in scrolling renderer.

| Option | Type | Default | Notes |
|---|---|---|---|
| `id` | ID | auto-generated | Referenced from a `display:` lambda as `id(sign_text)`. |
| `name` | string | required by ESPHome's `text` base | Entity name shown in Home Assistant. |
| `font` | ID | **required** | Must reference a `font:` entry. |
| `color` | `[r, g, b]`, 0-255 each | `[255, 255, 255]` | Text colour. Ignored if `light_id` is set. |
| `scroll_speed` | float, px/sec | `20` | Set to `0` and the text simply never scrolls, even if it overflows. |
| `scroll_gap` | int, px | `12` | Blank pixels inserted between the end and the restart of the scroll. |
| `light_id` | ID | none | Reference to a `light: platform: galactic_unicorn` entity (see below). When set, the light drives the text colour instead of `color`. |

### `light:` (`platform: galactic_unicorn`)

Exposes a Home Assistant `light` entity with a colour wheel, a brightness slider, and an on/off toggle, all bundled into ESPHome's standard RGB colour mode. It drives no LEDs directly; it just holds colour state for a `text:` entity to read via `light_id`.

| Option | Type | Default | Notes |
|---|---|---|---|
| `id` | ID | auto-generated | Referenced from a `text:` entity's `light_id`. |
| `name` | string | required by ESPHome's `light` base | Entity name shown in Home Assistant. |

Point a `text:` entity at it with `light_id`, and the colour picker's hue becomes the text colour, the brightness slider dims it, and turning the light off blanks the sign entirely. The scroll keeps running while blanked, so turning the light back on resumes the scroll where it would have been rather than restarting it. `color:` on the `text:` entity is ignored once `light_id` is set.

```yaml
light:
  - platform: galactic_unicorn
    id: sign_light
    name: "Sign Color"

text:
  - platform: galactic_unicorn
    id: sign_text
    name: "Sign Text"
    font: sign_font
    light_id: sign_light
```

See `example/galactic-unicorn-color.yaml` for a full working config.

## Drawing your own content

`display:` here is an ordinary ESPHome `DisplayBuffer`, so anything that works on any other ESPHome display works here: `it.print(...)`, `it.printf(...)`, `it.image(...)`, `it.line(...)`, `it.rectangle(...)`, and so on. The scrolling text entity is just one more thing you can draw. Combine it with other drawing calls in the same lambda:

```yaml
display:
  - platform: galactic_unicorn
    id: unicorn_display
    lambda: |-
      if (id(show_clock)) {
        it.strftime(0, 0, id(sign_font), "%H:%M", id(homeassistant_time).now());
      } else {
        id(sign_text)->draw(it);
      }
```

`id(sign_text)->draw(it)` only draws the scrolling text entity into whatever region you call it in. It does not clear the rest of the buffer, so mix it with other drawing calls freely.

## Behaviour notes

- Text that fits within the 53px panel width is centred and static.
- Text too long to fit scrolls leftward continuously, wrapping with `scroll_gap` pixels of blank space between the end of one pass and the start of the next, with no blank frame at the wrap.
- Changing the text value restarts the scroll from the beginning.
- Setting the text to an empty string blanks the panel.

## Hardware smoke test checklist

Nothing here is covered by CI. Automated tests run on GitHub's runners, which do not have a Galactic Unicorn attached, so **every item below must be checked by hand on real hardware** before trusting a change.

The example configs in this section (`example/pio-smoke-test.yaml` and `example/galactic-unicorn.yaml`) use a local `external_components` path (`../components`), so they only work from a clone of this repo, not from a bare `esphome` install:

```sh
git clone https://github.com/andrewcfitz/esphome-galactic-unicorn.git
cd esphome-galactic-unicorn/example
cp secrets.yaml.example secrets.yaml
# edit secrets.yaml: fill in your WiFi SSID/password and an API encryption key
```

Both example configs read from that same `example/secrets.yaml`.

### PIO/DMA driver and WiFi coexistence

Flash `example/pio-smoke-test.yaml` and check, in this order:

1. The panel shows solid red across all 53x11 pixels.
2. `esphome logs` shows WiFi connected with an IP address.
3. The `dump_config` output reports which PIO block was claimed, for example `PIO: pio1 sm0`.

If setup instead logs `Could not claim a PIO state machine`, see Troubleshooting below.

For the first flash, build with `esphome compile example/pio-smoke-test.yaml`, hold BOOTSEL while plugging in the board, then copy `example/.esphome/build/galactic-unicorn-smoke/.pioenvs/galactic-unicorn-smoke/firmware.uf2` to the `RPI-RP2` drive that appears. Every flash after that goes over OTA, so `esphome run example/pio-smoke-test.yaml` is enough.

### Scrolling text platform

Flash `example/galactic-unicorn.yaml` and check:

1. Home Assistant shows a `text.galactic_unicorn_sign_text` entity.
2. Typing short text like `hello` leaves it static and horizontally centred.
3. Typing a sentence long enough to overflow the panel makes it scroll leftward.
4. The scroll wraps with the configured `scroll_gap` and no blank frame or stutter.
5. Changing the text mid-scroll restarts it from the beginning.
6. Clearing the text blanks the panel.

## Troubleshooting

**`Could not claim a PIO state machine`.** The RP2040 has two PIO blocks with four state machines each. This component searches across both blocks for a free state machine, and the Pico W's WiFi driver (cyw43) only claims one state machine and around 15 instruction words on one PIO block, so on a stock config there's plenty of room left for the panel driver and this failure is close to unreachable. It is not an expected outcome; if you do see this message, something unusual is also claiming PIO resources (another component, or a heavily customized build), and you'll need to free one up. There's no configuration workaround within this component; it's a hardware constraint of sharing a fixed number of PIO state machines.

**Typing runs the log full of `Unknown character` (or similar) warnings.** This comes from ESPHome's own font code, not from this component, and we can't change it: it logs an `ESP_LOGW` warning for every codepoint your font doesn't have a glyph for, on every single print call. At the default 33ms update interval with two draw calls a frame (primary and wrapped-scroll copies), typing even one unsupported character into the Home Assistant text box produces 60+ warnings a second, forever, over both UART and the API log stream. This happens because the default font glyphset is Latin-only, so anything outside it (emoji, accented characters, CJK, etc.) triggers the flood. Fix it by adding the specific characters you need to the font's `glyphs:` list in your `font:` config, or by sticking to Latin text in the entity.

Example:

```yaml
font:
  - file: "gfonts://Roboto"
    id: sign_font
    size: 8
    glyphs: "!\"%()+=,-.:?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
```

## Credits

The panel driver in this component is a port of the PIO/DMA driver from [pimoroni/pimoroni-pico](https://github.com/pimoroni/pimoroni-pico), Pimoroni's C++ SDK for their boards.

## License

MIT. See [LICENSE](LICENSE).

```
Copyright (c) 2021 Pimoroni Ltd
Copyright (c) 2026 Andrew Fitz
```
