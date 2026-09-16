import serial
import time
import sys

PORT = '/dev/ttyACM0'
BAUD = 115200

print(f"Connecting to {PORT} at {BAUD} baud...")
ser = serial.Serial(PORT, BAUD, timeout=0.2)
ser.dtr = True
ser.rts = False
time.sleep(0.5)

# Clear any input buffer
ser.reset_input_buffer()

def read_until(timeout=3.0):
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

def send_cmd(cmd, wait=1.5):
    print(f"\n--- [SEND COMMAND]: {cmd} ---")
    ser.write((cmd + "\r\n").encode('utf-8'))
    ser.flush()
    return read_until(timeout=wait)

print("\n=== CAPTURING BOOT & RUNTIME OUTPUT (3s) ===")
initial_output = read_until(timeout=3.0)

# 1. Test status
out_status = send_cmd("status", wait=1.5)

# 2. Test rule list
out_rules = send_cmd("rule list", wait=1.5)

# 3. Test rule test (GT condition met -> Fan ON)
out_rule_gt = send_cmd("rule test sensor_temp_01 32.5 1", wait=2.0)

# 4. Test safe-default (valid=0 -> No rule evaluation)
out_safe_def = send_cmd("rule test sensor_temp_01 35.0 0", wait=2.0)

# 5. Test schedule list
out_sched = send_cmd("schedule list", wait=1.5)

# 6. Test manual alarm trigger
out_alarm = send_cmd("schedule test Siren Check", wait=2.0)

# 7. Test command dispatch
out_cmd = send_cmd("cmd send relay_ch3 TOGGLE", wait=2.0)

ser.close()
print("\n=== TEST COMPLETED SUCCESSFULLY ===")
