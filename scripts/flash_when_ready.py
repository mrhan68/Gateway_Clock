#!/usr/bin/env python3
import time
import os
import sys
import subprocess
import serial

ESPTOOL = "/home/chien/.espressif/python_env/idf5.3_py3.11_env/bin/esptool.py"
PORT = "/dev/ttyACM0"
BUILD_DIR = "/home/chien/ai-workspace/qwen-test/firmware/build"

print("=" * 60)
print("ESP32-S3 Auto-Flash Listener")
print(f"Monitoring {PORT}...")
print("=" * 60)

flash_cmd = [
    ESPTOOL, "--chip", "esp32s3", "-p", PORT, "-b", "460800",
    "--before", "no_reset", "--after", "hard_reset",
    "write_flash", "--flash_mode", "dio", "--flash_size", "16MB", "--flash_freq", "40m",
    "0x0", os.path.join(BUILD_DIR, "bootloader/bootloader.bin"),
    "0x8000", os.path.join(BUILD_DIR, "partition_table/partition-table.bin"),
    "0xf000", os.path.join(BUILD_DIR, "ota_data_initial.bin"),
    "0x20000", os.path.join(BUILD_DIR, "smart_clock_gateway.bin")
]

def try_flash():
    print("\n--> Attempting to flash...")
    res = subprocess.run(flash_cmd)
    return res.returncode == 0

# Check if already in bootloader
try:
    s = serial.Serial(PORT, 115200, timeout=0.8)
    line = s.readline()
    s.close()
    if not line or b"50%" not in line:
        print("Board might already be in download mode! Trying flash...")
        if try_flash():
            print("\n*** FLASH SUCCESSFUL! ***")
            sys.exit(0)
except Exception:
    pass

print("\nBoard is currently running factory test firmware.")
print(">>> VUI LONG BAM TREN BOARD: GIU BOOT -> BAM NHA RST -> THA BOOT <<<")
print("Waiting for bootloader signal...")

start_time = time.time()
while time.time() - start_time < 90:
    if os.path.exists(PORT):
        try:
            s = serial.Serial(PORT, 115200, timeout=0.5)
            data = s.read(100)
            s.close()
            # If data is empty or does not contain factory RGB pattern 50%
            if b"50%" not in data:
                print(f"\nDetected state change! Received: {data}")
                time.sleep(0.3)
                if try_flash():
                    print("\n*** FLASH HOAN TAT THANH CONG! ***")
                    sys.exit(0)
            else:
                sys.stdout.write(".")
                sys.stdout.flush()
        except serial.SerialException:
            # USB reconnected / reset!
            print("\nUSB reset detected! Waiting 1s for port to settle...")
            time.sleep(1.0)
            if os.path.exists(PORT):
                if try_flash():
                    print("\n*** FLASH HOAN TAT THANH CONG! ***")
                    sys.exit(0)
        except Exception:
            time.sleep(0.5)
    else:
        sys.stdout.write("x")
        sys.stdout.flush()
        time.sleep(0.5)

print("\nTimeout waiting for bootloader trigger.")
sys.exit(1)
