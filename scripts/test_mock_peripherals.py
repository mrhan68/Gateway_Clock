import serial
import time
import sys

PORT = '/dev/ttyACM0'
BAUD = 115200

print(f"Connecting to {PORT} at {BAUD} baud to verify Mock Peripherals Subsystem...")
ser = serial.Serial(PORT, BAUD, timeout=0.2)
ser.dtr = True
ser.rts = False

def read_until(timeout=4.0):
    start = time.time()
    out = []
    while time.time() - start < timeout:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            text = chunk.decode('utf-8', errors='replace')
            sys.stdout.write(text)
            sys.stdout.flush()
            out.append(text)
    return "".join(out)

def send_cmd(cmd, wait=2.0):
    print(f"\n--- [SEND COMMAND]: {cmd} ---")
    ser.write((cmd + "\r\n").encode('utf-8'))
    ser.flush()
    return read_until(timeout=wait)

print("\n=== WAITING FOR BOOT COMPLETION & PROMPT ===")
start = time.time()
buf = ""
while time.time() - start < 8.0:
    chunk = ser.read(ser.in_waiting or 1)
    if chunk:
        text = chunk.decode('utf-8', errors='replace')
        sys.stdout.write(text)
        sys.stdout.flush()
        buf += text
        if "gateway>" in buf:
            break

print("\n=== WAITING FOR AUTOMATIC MOCK RS485 POLLING (5s) ===")
read_until(timeout=5.0)

# 1. Check mock status
send_cmd("mock status", wait=2.0)

# 2. Test manual value injection to trigger Fan ON rule (> 30C)
send_cmd("mock rs485 set 33.5 70.0 1", wait=5.0)

# 3. Test virtual storage write test
send_cmd("mock storage test", wait=2.0)

# 4. Check status again to see updated coils and CSV
send_cmd("mock status", wait=2.0)

ser.close()
print("\n=== MOCK PERIPHERALS VERIFICATION COMPLETED ===")
