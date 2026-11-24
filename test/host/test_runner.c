/**
 * @file test_runner.c
 * @brief Native Host C Test Runner & Static Assertion Gate (FSD §14.5 / FR-TEST-01)
 */
#include "test_harness.h"

// FSD §14.5 Compile-Time Static Assertions
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t write_offset;
    uint32_t read_offset;
    uint32_t total_records;
    uint32_t unacked_count;
    uint32_t crc32;
} host_sync_queue_header_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint8_t  record_type;
    uint8_t  flags;
    uint16_t payload_len;
    char     device_id[32];
    char     payload[128];
    uint32_t crc32;
} host_sync_queue_record_t;

typedef struct {
    int   code;
    void *data;
    size_t len;
    uint32_t timestamp;
} host_event_bus_msg_t;

_Static_assert(sizeof(host_sync_queue_record_t) == 172, "sync_queue_record_t size must be exactly 172 bytes");
_Static_assert(sizeof(host_sync_queue_header_t) == 24, "sync_queue_header_t size must be exactly 24 bytes");
_Static_assert(sizeof(host_event_bus_msg_t) <= 48, "event_bus_msg_t size must not exceed 48 bytes");

int g_tests_run = 0;
int g_tests_failed = 0;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("=====================================================\n");
    printf(" Smart Clock Gateway — C Native Host Test Harness\n");
    printf(" FSD §14.5 / FR-TEST-01 Verification Suite\n");
    printf("=====================================================\n\n");

    printf("--- [Suite 1: Relay Asymmetrical Interlock & Dwell] ---\n");
    run_relay_interlock_tests();

    printf("\n--- [Suite 2: Monotonic Timing vs NTP Steps] ---\n");
    run_monotonic_timing_tests();

    printf("\n--- [Suite 3: BLE Packet Parsers (SIG & BTHome)] ---\n");
    run_ble_parser_tests();

    printf("\n--- [Suite 4: Scheduler Snooze +9m & Anti-Retroactive] ---\n");
    run_scheduler_snooze_tests();

    printf("\n--- [Suite 5: Real Rule Engine Hysteresis, Cooldown & Multi-Metric] ---\n");
    run_rule_engine_tests();

    printf("\n=====================================================\n");
    if (g_tests_failed == 0) {
        printf(" ALL TESTS PASSED (%d/%d tests successful)\n", g_tests_run, g_tests_run);
        printf("=====================================================\n");
        return 0;
    } else {
        printf(" TEST FAILURE: %d/%d tests failed!\n", g_tests_failed, g_tests_run);
        printf("=====================================================\n");
        return 1;
    }
}
