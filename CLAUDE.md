# Claude Code — Project Guidelines

## Project Overview

M5Stack Dial (ESP32-S3) IoT remote control. Coordinator + Views architecture with MQTT over WiFi. See `docs/DESIGN.md` for full architecture and `docs/IDEA.md` for product spec.

## Hardware

- **Board:** M5Stack Dial — ESP32-S3, 240x240 round GC9A01 LCD, capacitive touch (FT5x06), rotary encoder, BtnA button
- **USB port:** Shows as `/dev/cu.usbmodem2101` on macOS (may vary)
- **FQBN:** `m5stack:esp32:m5stack_dial`

## Toolchain

### Build

```sh
arduino-cli compile --fqbn m5stack:esp32:m5stack_dial .
```

To preserve the ELF file for backtrace decoding:

```sh
arduino-cli compile --fqbn m5stack:esp32:m5stack_dial --output-dir /tmp/dial-build .
```

### Flash

```sh
# Find the port first — it can change after power cycles
ls /dev/cu.usb*
arduino-cli upload --fqbn m5stack:esp32:m5stack_dial --port /dev/cu.usbmodem2101 .
```

If using `--output-dir` during compile, pass `--input-dir` during upload:

```sh
arduino-cli upload --fqbn m5stack:esp32:m5stack_dial --port /dev/cu.usbmodem2101 --input-dir /tmp/dial-build .
```

### Serial Monitoring

The system `python3` (Xcode CLI tools) does NOT have pyserial. Use miniforge:

```sh
/opt/homebrew/Caskroom/miniforge/base/bin/python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbmodem2101', 115200, timeout=1)
ser.dtr = False; time.sleep(0.1); ser.dtr = True  # toggle DTR to reset device
start = time.time()
while time.time() - start < 20:
    line = ser.readline()
    if line: print(line.decode('utf-8', errors='replace').rstrip())
ser.close()
"
```

`arduino-cli monitor` and `cat /dev/cu.usbmodem*` are unreliable for capturing boot output. The pyserial DTR-toggle method above is the most reliable way to reset the device and capture output from the beginning.

### Backtrace Decoding

When the device crashes with a `Guru Meditation Error`, decode the backtrace using:

```sh
/Users/scottfrancis/Library/Arduino15/packages/m5stack/tools/esp-x32/2411/bin/xtensa-esp32s3-elf-addr2line \
  -pfiaC -e /tmp/dial-build/m5-dial-remote.ino.elf \
  0xADDR1 0xADDR2 0xADDR3
```

Requires the ELF from a `--output-dir` build. Always build with `--output-dir` when debugging crashes.

## Debugging Workflow

1. Add targeted `Serial.printf()` / `Serial.println()` guards (e.g. `if (loopCount < 5)`) to narrow the crash location
2. Build with `--output-dir /tmp/dial-build` to preserve the ELF
3. Flash and monitor serial with pyserial DTR-toggle method
4. Decode backtrace addresses with `xtensa-esp32s3-elf-addr2line`
5. Fix the bug, remove debug logging, rebuild clean

## Known Gotchas

- **`M5Dial.begin()` encoder:** Must pass `true` as second arg — `M5Dial.begin(cfg, true)`. Defaults to `false`, leaving encoder pin registers null. Causes `LoadProhibited` crash on any encoder access.
- **mDNS:** Unreliable on ESP32-S3. Use raw IP addresses for broker in `environment.h`.
- **M5GFX:** No `getTextSize()` method. Don't try to save/restore text size — just set directly.
- **WiFi after rapid flashing:** Can get stuck. Power cycle the device (unplug USB, wait, replug).
- **USB port name:** Can change between `/dev/cu.usbmodem*` and `/dev/cu.usbserial-*` after reboot. Always `ls /dev/cu.usb*` first.
- **Arduino src/ layout:** Source files live in `src/` with subdirectories (`core/`, `gfx/`, `views/`). The `.ino` must stay in the project root. Cross-directory includes use relative paths (`../Config.h`, `../core/DisplayManager.h`).

## Credentials

- `environment.h` contains WiFi SSID/password and MQTT broker IP — **gitignored**
- `environment.h.example` is the committed template with placeholder values
- Never commit `environment.h`

## File Structure

Source files organized under `src/` with subdirectories: `core/` (subsystems), `gfx/` (drawing), `views/` (device views). Shared headers (`Config.h`, `InputEvent.h`, `DeviceView.h`) in `src/` root. The `.ino` coordinator stays in the project root. See `docs/DESIGN.md` for the full layout.

## Adding a New Device View

1. Create `src/views/NewDeviceView.h` / `.cpp` implementing `DeviceView`
2. `#include "src/views/NewDeviceView.h"` in the `.ino`
3. Add a `static NewDeviceView` instance
4. Add its pointer to the `views[]` array
5. No framework changes needed (Open/Closed principle)

## Branching

- `main` is the deployed firmware branch
- Feature work on `feature/*` branches
