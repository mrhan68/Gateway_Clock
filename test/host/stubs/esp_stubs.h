#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ESP_LOGI(tag, ...) do { printf("[%s] ", tag); printf(__VA_ARGS__); printf("\n"); } while(0)
#define ESP_LOGW(tag, ...) do { printf("[%s] W: ", tag); printf(__VA_ARGS__); printf("\n"); } while(0)
#define ESP_LOGE(tag, ...) do { printf("[%s] E: ", tag); printf(__VA_ARGS__); printf("\n"); } while(0)
#define ESP_OK 0
#define ESP_FAIL -1

typedef int esp_err_t;

// FreeRTOS Mock
typedef void* xQueueHandle;
typedef void* xTimerHandle;
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
#define portTICK_PERIOD_MS 1

static inline xQueueHandle xQueueCreate(uint32_t len, uint32_t item_size) { return (xQueueHandle)1; }
static inline int xQueueSend(xQueueHandle q, const void *item, TickType_t ticks) { return pdTRUE; }
static inline int xQueueReceive(xQueueHandle q, void *item, TickType_t ticks) { return pdTRUE; }

static inline uint32_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) {}

// ESP Timer Mock
static inline int64_t esp_timer_get_time(void) { return 1000000; }

// NVS Mock
typedef uint32_t nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY 2
typedef int BaseType_t;
typedef void* SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
static inline int xSemaphoreTake(SemaphoreHandle_t m, int ticks) { return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t m) { return 1; }
#define portMAX_DELAY 0xffffffff
#define ESP_LOGD(tag, ...) do { } while(0)
