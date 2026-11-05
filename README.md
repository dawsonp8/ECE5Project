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
