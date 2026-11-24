CC = gcc
CFLAGS = -Wall -Istubs -DTEST_HOST  -Wno-error=unused-variable -Wno-error=unused-function -I../../firmware/main

SRCS = test_runner.c test_relay_interlock.c test_monotonic_timing.c test_ble_parsers.c test_scheduler_snooze.c test_rule_engine.c stubs/rule_engine_stub.c ../../firmware/main/rule_engine.c
TARGET = host_tests

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
