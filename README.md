# OpenHaldex S3 Touch Controller (ESP32-S3 4.3 inch)

In-car touchscreen controller for OpenHaldex-S3 using a Waveshare ESP32-S3 4.3 inch RGB display.

This project provides:
- Touch buttons for torque split presets
- Wi-Fi connection to OpenHaldex-S3 AP (`OpenHaldex-S3`)
- API mode switching over HTTP (`/api/mode`)
- On-screen confirmation from `/api/status`
- Visual match with browser UI behavior
- RGB mode-status LED support (NeoPixel / WS2812)

## Current mode presets
- STOCK (FWD)
- 90 / 10
- 80 / 20
- 70 / 30
- 60 / 40
- 50 / 50

## Hardware
- Waveshare ESP32-S3 Touch LCD 4.3B (or pin-compatible setup)
- OpenHaldex-S3 unit
- Stable 5V power supply (recommended for car use)
- 1x WS2812/NeoPixel 5V LED (e.g. [Adafruit NeoPixel breakout](https://www.adafruit.com/product/1938) or [single WS2812B LED module examples](https://www.amazon.co.uk/s?k=ws2812b+led+module))
- 1x 330–470Ω resistor for the LED data line (recommended, e.g. [resistor kit](https://www.amazon.co.uk/s?k=330+ohm+resistor+kit))
- Hook-up wire and ground/power connection from the controller to the LED

## Arduino libraries
Install in Arduino IDE Library Manager:
- Arduino_GFX_Library
- TAMC_GT911
- Adafruit NeoPixel

## Sketch
Main sketch:
- `open_haldex_screen.ino`

## Wi-Fi and API details
The sketch is configured to connect to:
- SSID: `OpenHaldex-S3`
- Host: `192.168.4.1`

Mode commands are sent as:
- `POST /api/mode`
- JSON body: `{ "mode": "9010" }` (example)

After each command, the sketch polls:
- `GET /api/status`

And displays returned controller mode / effective mode.

## RGB LED behavior
- Default configuration in `open_haldex_screen.ino`:
  - `LED_PIN = 4`
  - `LED_COUNT = 1`
  - `LED_BRIGHTNESS = 180`
- While connected to OpenHaldex-S3 Wi-Fi, LED color follows selected mode:
  - FWD = blue
  - 90/10 = cyan
  - 80/20 = green
  - 70/30 = yellow
  - 60/40 = orange
  - 50/50 = red
- While not connected to Wi-Fi, LED pulses dim white to show the unit is powered but not linked.

## Build and upload
1. Open `open_haldex_screen.ino` in Arduino IDE.
2. Select your ESP32-S3 board profile.
3. Ensure PSRAM is enabled if required by your board.
4. Compile and upload.

## Known behavior
- Button presses only affect drivetrain mode when connected to OpenHaldex-S3 Wi-Fi.
- UI still updates locally if Wi-Fi is unavailable, but controller command will fail.

## Safety notice
This project influences drivetrain behavior.
Use at your own risk.
Test in a controlled environment before road use.
You are responsible for compliance with local laws, safety standards, and vehicle reliability.

## Roadmap ideas
- Save/restore last selected mode
- Config page for custom mode labels
- Brightness/day-night profile
- Optional long-press confirmation for aggressive modes

## Contributing
PRs are welcome for:
- Hardware compatibility improvements
- UI improvements
- Additional OpenHaldex status telemetry

## License
MIT License (see `LICENSE`).
