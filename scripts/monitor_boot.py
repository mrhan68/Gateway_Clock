#!/usr/bin/env python3
import serial
import time
import os

PORT = "/dev/ttyACM0"
print(f"Listening on {PORT} (115200 baud)...")
start_time = time.time()
while time.time() - start_time < 45:
    if os.path.exists(PORT):
        try:
            with serial.Serial(PORT, 115200, timeout=1) as s:
                while time.time() - start_time < 45:
                    line = s.readline()
                    if line:
                        text = line.decode('utf-8', errors='replace').rstrip()
                        print(text, flush=True)
        except Exception:
            time.sleep(0.5)
    else:
        time.sleep(0.5)
