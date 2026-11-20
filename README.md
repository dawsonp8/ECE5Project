# ECE5 Project

Arduino firmware project targeting the **Arduino Uno**, managed with
[PlatformIO](https://platformio.org/).

## Getting started

Open this folder in VS Code with the PlatformIO extension installed, or install
the PlatformIO Core CLI. From the project root, run:

```sh
pio run
```

To upload the firmware to a connected Arduino Uno:

```sh
pio run --target upload
```

To run PlatformIO tests:

```sh
pio test
```

## Project layout

- `platformio.ini` — PlatformIO environment, board, and framework settings.
- `src/main.cpp` — Firmware entry point; implement initialization in `setup()`
  and repeated work in `loop()`.
- `include/` — Shared project header files.
- `lib/` — Project-specific libraries.
- `test/` — PlatformIO tests.

Build output and generated editor files are excluded in `.gitignore`.

## Heart-rate source: pulse sensor or Garmin watch

`HR_SOURCE_BLE` in `include/config.h` picks where the heart rate comes from:

- `0`: the analog pulse sensor on A2 (works best clipped to an earlobe).
- `1`: a Garmin watch through an ESP32 bridge.

### Garmin setup (`HR_SOURCE_BLE 1`)

1. **Wire the ESP32 DevKit V1:** GPIO17 (TX2) to Uno **D0**, GND to Uno GND. Power the ESP32 from its own USB cable.
2. **Upload the bridge to the ESP32:** `pio run -e esp32_hr -t upload --upload-port <ESP32 port>`.
3. **Upload to the Uno** (`pio run -e test_heart -t upload`, or the game) with the GPIO17 wire **unplugged from D0**, then plug it back in.
4. **On the watch:** Settings > Sensors & Accessories > Wrist Heart Rate > Broadcast Heart Rate.
5. `pio device monitor -b 115200 -p <ESP32 port>` shows the bridge finding the watch and each `HR` reading. If several watches nearby are broadcasting, set `WATCH_NAME` in `src/stages/esp32_hr_bridge.cpp` to part of your watch's name.

Without the watch (or with the bridge unplugged) the game uses the fixed move timer.
