#ifndef MOCK_PERIPHERALS_H
#define MOCK_PERIPHERALS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structs matching FSD §4.3
typedef struct {
    char  device_id[32];   // e.g. "sensor_temp_01", "sensor_hum_01"
    uint8_t slave_id;      // Modbus Slave ID (e.g. 1)
    uint16_t reg_addr;     // Modbus Register
    float value;           // Scaled value
    bool  valid;           // true if CRC valid; false on timeout/error
    uint32_t timestamp;    // UNIX epoch
} rs485_telemetry_t;

typedef struct {
    char target_device_id[32];
    uint8_t slave_id;
    uint8_t function_code; // 0x05 or 0x06
    uint16_t reg_addr;
    uint16_t value;
} rs485_cmd_t;

/**
 * @brief Initialize all mock peripheral subsystems (RS485, Storage, Display)
 */
void mock_peripherals_init(void);

/**
 * @brief Start automatic mock RS485 telemetry polling
 * @param interval_ms Polling interval in ms (e.g. 3000ms)
 */
void mock_rs485_start(uint32_t interval_ms);

/**
 * @brief Stop automatic mock RS485 telemetry polling
 */
void mock_rs485_stop(void);

/**
 * @brief Manually set sensor values for mock RS485 slave #1
 */
void mock_rs485_set_values(float temp, float hum, bool valid);

/**
 * @brief Inject faults into mock RS485 bus
 * @param error_type "none", "timeout", "invalid"
 */
void mock_rs485_inject_fault(const char *error_type);

/**
 * @brief Check if mock RS485 is currently running
 */
bool mock_rs485_is_running(void);

/**
 * @brief Test writing a CSV record to virtual storage
 */
bool mock_storage_write_sample_csv(void);

/**
 * @brief Print status of all mock peripherals
 */
void mock_peripherals_print_status(void);

/**
 * @brief Get latest simulated sensor readings
 */
void mock_rs485_get_latest(float *temp, float *hum, bool *valid);

/**
 * @brief Get current coil status of simulated 4-channel relay board
 */
void mock_rs485_get_relays(uint8_t coils[4]);

#ifdef __cplusplus
}
#endif

#endif // MOCK_PERIPHERALS_H
