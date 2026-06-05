# ESP8266 NTP Multi-OLED Clock

A four-panel desk clock driven by an ESP8266. Each of the four grayscale OLED
displays shows one part of the time/date in a large 14-segment ("DSEG") font:

```
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│  STUNDE  │ │  MINUTE  │ │ SEKUNDEN │ │ Mittwoch │
│          │ │          │ │          │ │    05    │
│    14    │ │    37    │ │    09    │ │ ──────── │
│          │ │          │ │          │ │   Juni   │
│          │ │          │ │          │ │   2026   │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
   hours       minutes      seconds        date
```

Time is fetched over WiFi via NTP, so the clock is always accurate and handles
itself without an RTC module.

## Features

- **NTP-synced time** over WiFi (no RTC needed). Periodic re-sync corrects any drift.
- **Four independent 128×128 OLEDs** on a single I²C bus via a TCA9548A multiplexer.
- **Partial display refresh** – only the changed digits are pushed over I²C
  (see [How the partial refresh works](#how-the-partial-refresh-works)), so the
  seconds tick over crisply without lag or skipped seconds.
- **Time-of-day brightness** – dimmer in the evening, minimal at night.
- **Nightly anti–burn-in scrub** – a short sweeping pattern runs once per night.
- **Boot animation & status screens** – logo sweep, display self-test, WiFi/NTP status.
- Localized **German** weekday and month names.

## Hardware

| Part | Notes |
|------|-------|
| **NodeMCU Lolin V3** (ESP8266 ESP-12F) | WiFi MCU, 3.3 V logic |
| **TCA9548A** I²C multiplexer | Lets all four OLEDs share one I²C bus (they all use the same address) |
| **4× Waveshare 1.5" OLED**, 128×128, 16 gray levels, SSD1327 | SKU 13992, run in **I²C** mode |
| Jumper wires, 3.3 V supply | The four OLEDs + MCU draw enough that a stable supply is recommended |

### Wiring

The ESP8266 talks I²C only to the TCA9548A; each OLED hangs off one of the
multiplexer's downstream channels.

```
ESP8266            TCA9548A            OLEDs
-------            --------            -----
D2 (GPIO4, SDA) ── SDA      SD2/SC2 ── OLED 1 (hours)   channel 2
D1 (GPIO5, SCL) ── SCL      SD3/SC3 ── OLED 2 (minutes) channel 3
3V3 ───────────── VIN       SD4/SC4 ── OLED 3 (seconds) channel 4
GND ───────────── GND       SD5/SC5 ── OLED 4 (date)    channel 5
```

- **TCA9548A I²C address:** `0x70`
- **OLED I²C address:** `0x3D` (all four are identical – that's why the
  multiplexer is required)
- Multiplexer channels used: **2, 3, 4, 5** (`buses[]` in the sketch)
- Add I²C pull-up resistors (typically 2.2k–4.7k) if your modules don't already
  include them; the sketch runs the bus fast.

## Software / Libraries

Built with the **Arduino IDE** (ESP8266 core). Required libraries:

- `ESP8266WiFi` (part of the ESP8266 Arduino core)
- [`NTPClient`](https://github.com/arduino-libraries/NTPClient)
- [`Adafruit GFX Library`](https://github.com/adafruit/Adafruit-GFX-Library)
- [`Adafruit SSD1327`](https://github.com/adafruit/Adafruit_SSD1327)
- `Wire` (bundled with the core)
- `font.h` – bundled in this repo, contains the DSEG14 fonts in GFX format

## Configuration

Everything user-specific lives near the top of [`clock.ino`](clock.ino):

```cpp
const char* ssid     = "YOURSSID";       // your WiFi name
const char* password = "YOURPASSWORD";   // your WiFi password

NTPClient timeClient(ntpUDP, "pool.ntp.org");  // NTP server (use a local one if you like)

timeClient.setTimeOffset(7200);          // UTC offset in seconds (7200 = UTC+2 / CEST)
```

| Setting | Where | Meaning |
|---------|-------|---------|
| `ssid` / `password` | top of file | WiFi credentials |
| NTP server | `NTPClient(...)` | defaults to `pool.ntp.org` |
| `setTimeOffset()` | `setup()` | timezone offset in **seconds** (3600 = +1 h). No automatic DST. |
| `buses[]` | `{2,3,4,5}` | which multiplexer channel each panel sits on |
| `displayRotations[]` | `{3,1,3,3}` | per-panel rotation (0–3); flip a value by ±2 if a panel is upside-down |

> **Timezone note:** `setTimeOffset()` is a fixed offset; there is no automatic
> summer/winter time switch. Adjust it (3600 ↔ 7200 for Germany) when the clocks
> change, or add a DST rule.

## How the partial refresh works

The Adafruit SSD1327 library already tracks a "dirty window" – `display()` only
transmits the bounding box of whatever was drawn since the last call. The trick
is simply to **not** repaint the whole screen every tick:

- The static chrome (the grey header bar and its label) is drawn **once** per
  panel, the first time it renders.
- On every update, only the digits are repainted via the `drawValue()` helper,
  which clears just the digit box and prints the new value.
- The clear box is measured against a full-width `0` template (the widest
  digit), so a narrow digit like `1` still fully erases a previous `0`.

The result: pushing the seconds is ~30–50× less I²C traffic than a full frame,
which keeps the clock from drifting or skipping seconds.

## Build & flash

1. Install the **ESP8266 board package** in the Arduino IDE
   (Boards Manager → "esp8266").

   > **Note:** The NodeMCU is not part of the IDE's standard repertoire, so the
   > Board Manager must be extended first. In the same window, under
   > *"Additional Board Manager URLs"*, add the following address:
   > `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. Install the libraries listed above (Library Manager).
3. Open [`clock.ino`](clock.ino) and set your WiFi credentials and timezone offset.
4. Select board **NodeMCU 1.0 (ESP-12E Module)**, choose the serial port.
5. Upload.

On boot the displays run a short animation, connect to WiFi (showing the
assigned IP), sync time over NTP, and then start showing the clock.

## Behavior reference

- **NTP re-sync:** a scheduled re-sync runs at 01:00 and 13:00.
- **Brightness:** normal during the day, reduced in the evening (about an hour
  after the approximate sunset for the current month, until 22:00), and minimal
  between 02:00 and 07:00.
- **Burn-in scrub:** at 04:00 a brief moving pattern runs on all panels, then
  the panels redraw fully.

## License

Released under the [MIT License](LICENSE) — use, modify and distribute freely.
