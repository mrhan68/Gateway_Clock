CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -O2 -DTEST_HOST
TARGET = host_tests

SRCS = test_runner.c \
       test_relay_interlock.c \
       test_monotonic_timing.c \
       test_ble_parsers.c \
       test_scheduler_snooze.c

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS) test_harness.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
