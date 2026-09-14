# User Manual — [Project Name]

> **Status:** Pre-alpha — project scaffolding only. The FSD and first firmware have
> not been written yet. This manual ships from day one with an honest status line.

## 1. What this system does

[Brief description of the product and its purpose.]

## 2. What you need

| Item | Specification |
|---|---|
| Board | [e.g., ESP32-S3 DevKitC] |
| USB cable | Data cable (not charge-only) |
| Computer | With serial port access, Python 3.11+, pytest |
| WiFi | Access to a network the board can join |

## 3. Getting the firmware

**Option A — Build locally:**

```bash
# Install ESP-IDF (one-time)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. ./export.sh

# Build
cd /path/to/your/project/firmware
idf.py set-target esp32
idf.py build
```

**Option B — Download from CI:**

```bash
# After a push to main, download the artifact
gh run list -L 1 -R <owner>/<repo>
gh run download <run-id> -D fw
```

## 4. Flashing

```bash
# Build artifacts + flash_args are in build/
idf.py -p /dev/ttyUSB0 flash

# Or, if you have just the binary:
esptool.py --port /dev/ttyUSB0 --chip esp32 \
  write-flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 firmware.bin
```

**Check flash offsets from `build/flash_args`** — do not guess them.

## 5. Observing serial output

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Or with `screen`/`minicom` at 115200 baud.

## 6. Common operations

### Power-cycle the device

Unplug the USB cable, wait 2 seconds, plug it back in.

### Factory reset (if supported)

Hold the BOOT button for 5 seconds, then release.

### Enter download mode

1. Hold BOOT
2. Press and release RESET
3. Release BOOT

## 7. Running tests

### Host tests (automated)

```bash
pytest tests/host/ -q
```

### Target and manual tests

Follow the instructions in `testing/test-plan.yaml`. Each test at the target or
manual tier includes **operator instructions** — numbered steps, exact commands,
and expected output. Run them and report results to the AI.

## 8. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| No serial output | Wrong port, wrong baud, or board not in serial mode | Check `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`; confirm 115200 baud |
| Board not detected | Charge-only cable | Use a data cable |
| `invalid magic byte` on boot | Flash offset wrong | Check `build/flash_args`; re-flash with correct offsets |
| Boot loop | Corrupt flash | Erase and reflash: `esptool.py --port /dev/ttyUSB0 erase_flash` |
| Permission denied on serial port | User not in `dialout` group | `sudo usermod -a -G dialout $USER` then re-login |
