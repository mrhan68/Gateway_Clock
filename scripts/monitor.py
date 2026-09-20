import serial
import sys
import time

port = '/dev/ttyACM0'
print(f"Opening {port} at 115200 baud...")

while True:
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
        ser.dtr = True
        ser.rts = False
        print("Connected! Waiting for serial data (Press EN/RESET on board)...")
        while True:
            raw = ser.read(ser.in_waiting or 1)
            if raw:
                sys.stdout.write(raw.decode('utf-8', errors='replace'))
                sys.stdout.flush()
    except serial.SerialException:
        time.sleep(0.5)
    except KeyboardInterrupt:
        break
