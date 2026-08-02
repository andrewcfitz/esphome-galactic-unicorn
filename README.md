# ESPHome Galactic Unicorn

[![CI](https://github.com/andrewcfitz/esphome-galactic-unicorn/actions/workflows/ci.yml/badge.svg)](https://github.com/andrewcfitz/esphome-galactic-unicorn/actions/workflows/ci.yml)

An ESPHome external component that drives the [Pimoroni Galactic Unicorn](https://shop.pimoroni.com/products/galactic-unicorn) 53x11 RGB LED matrix and exposes a scrolling text sign to Home Assistant.

*(photo of the panel running goes here)*

## What this supports

- Pimoroni **Galactic Unicorn**, 53x11 RGB LED matrix.
- The **original Pico W / RP2040** revision of the board only.

**Not supported:**

- The newer **Pico 2 W / RP2350** revision of the Galactic Unicorn.
- The **Cosmic Unicorn** and **Stellar Unicorn** variants.
- The board's buttons, light sensor, and I2S speaker. This component only drives the LED matrix.

The panel's GPIO pins are fixed by the hardware and are not configurable in YAML.

## Quick start

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

For the first flash, hold BOOTSEL while plugging in the board, then copy the compiled `.uf2` file to the `RPI-RP2` drive that appears. Every flash after that goes over OTA, so `esphome run` is enough.

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
| `color` | `[r, g, b]`, 0-255 each | `[255, 255, 255]` | Text colour. |
| `scroll_speed` | float, px/sec | `20` | Set to `0` and the text simply never scrolls, even if it overflows. |
| `scroll_gap` | int, px | `12` | Blank pixels inserted between the end and the restart of the scroll. |

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

### PIO/DMA driver and WiFi coexistence

Flash `example/pio-smoke-test.yaml` and check, in this order:

1. The panel shows solid red across all 53x11 pixels.
2. `esphome logs` shows WiFi connected with an IP address.
3. The `dump_config` output reports which PIO block was claimed, for example `PIO: pio1 sm0`.

If setup instead logs `Could not claim a PIO state machine`, see Troubleshooting below.

### Scrolling text platform

Flash `example/galactic-unicorn.yaml` and check:

1. Home Assistant shows a `text.galactic_unicorn_sign_text` entity.
2. Typing short text like `hello` leaves it static and horizontally centred.
3. Typing a sentence long enough to overflow the panel makes it scroll leftward.
4. The scroll wraps with the configured `scroll_gap` and no blank frame or stutter.
5. Changing the text mid-scroll restarts it from the beginning.
6. Clearing the text blanks the panel.

## Troubleshooting

**`Could not claim a PIO state machine`.** The RP2040 has two PIO blocks with four state machines each. This component claims one for the LED matrix; ESPHome's WiFi driver on the Pico W also needs PIO resources and claims first during boot. If you see this message, the WiFi driver got there before the panel driver and took the resources this component needed. There's no configuration workaround; it's a known hardware constraint of sharing PIO between WiFi and the display driver.

**Brightness change doesn't seem to apply instantly.** Brightness is baked into each pixel's colour value at the moment `set_pixel()` writes it, not applied globally when the panel refreshes. In practice this is invisible because the display redraws every frame (`update_interval`, default `33ms`), so a brightness change shows up on the next redraw. If you set brightness once at boot and never touch it again, this doesn't matter. If you're changing it dynamically and something else is holding the display static, that's why it looks delayed.

## Credits

The panel driver in this component is a port of the PIO/DMA driver from [pimoroni/pimoroni-pico](https://github.com/pimoroni/pimoroni-pico), Pimoroni's C++ SDK for their boards.

## License

MIT. See [LICENSE](LICENSE).

```
Copyright (c) 2021 Pimoroni Ltd
Copyright (c) 2026 Andrew Fitz
```
