# OpenHaldex S3 Touch Controller (ESP32-S3 4.3 inch)

In-car touchscreen controller for OpenHaldex-S3 and OpenHaldex-C6 using a Waveshare ESP32-S3 4.3 inch RGB display.

This project provides:
- Touch buttons for torque split presets
- Wi-Fi connection to OpenHaldex AP (S3 or C6)
- API mode switching over HTTP (`/api/mode`)
- On-screen confirmation from status endpoint (`/api/status` for S3, `/api/dashboard` for C6)
- Visual match with browser UI behavior

## Two sketch versions
Use the version that matches your controller firmware:
- `open_haldex_screen.ino` for OpenHaldex-S3 (original stable S3 sketch)
- `open_haldex_c6_screen.ino` for OpenHaldex-C6 (Forbes Automotive)

Controller-specific behavior:
- S3 version: SSID `OpenHaldex-S3`, IP `192.168.4.1`, status endpoint `/api/status`, string mode payloads
- C6 version: SSID `OpenHaldex-C6`, IP `192.168.1.1`, status endpoint `/api/dashboard`, numeric mode payloads (`0..5`)

## Current mode presets
- STOCK (FWD)
- 90 / 10
- 80 / 20
- 70 / 30
- 60 / 40
- 50 / 50

## Hardware
- Waveshare ESP32-S3 Touch LCD 4.3B (or pin-compatible setup)
- OpenHaldex-S3 or OpenHaldex-C6 unit
- Stable 5V power supply (recommended for car use)

## Arduino libraries
Install in Arduino IDE Library Manager:
- Arduino_GFX_Library
- TAMC_GT911

## Sketch
Available sketches:
- `open_haldex_screen.ino` (S3)
- `open_haldex_c6_screen.ino` (C6)

## Wi-Fi and API details
S3 sketch (`open_haldex_screen.ino`):
- SSID: `OpenHaldex-S3`
- Host: `192.168.4.1`
- `POST /api/mode` with JSON body such as `{ "mode": "9010" }`
- Polls `GET /api/status`

C6 sketch (`open_haldex_c6_screen.ino`):
- SSID: `OpenHaldex-C6`
- Host: `192.168.1.1`
- `POST /api/mode` with numeric mode JSON such as `{ "mode": 2 }`
- Polls `GET /api/dashboard`

## Build and upload
1. Open the correct sketch for your controller in Arduino IDE.
2. Select your ESP32-S3 board profile.
3. Ensure PSRAM is enabled if required by your board.
4. Compile and upload.

### Arduino IDE tool settings (important)
If these settings are wrong, the display can stay black and reboot with framebuffer memory errors.

Recommended for Waveshare ESP32-S3 4.3 RGB boards:
- **Board**: your exact Waveshare ESP32-S3 profile (or `ESP32S3 Dev Module` if needed)
- **PSRAM**: `OPI PSRAM` (or `Enabled`)
- **Flash Size**: match your board (commonly `16MB`)
- **Partition Scheme**: `Huge APP` (or another large app partition)

If Serial Monitor shows errors like:
- `lcd_rgb_panel_alloc_frame_buffers ... no mem for frame buffer`
- `ESP_ERR_NO_MEM`

then the fix is almost always correcting **Board** and **PSRAM** in Arduino IDE Tools.

## Known behavior
- Button presses only affect drivetrain mode when connected to the matching OpenHaldex Wi-Fi.
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
