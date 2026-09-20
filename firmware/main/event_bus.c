#include <stdlib.h>
#include <string.h>
#include "event_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "supervisor.h"

static const char *TAG = "EVENT_BUS";

static QueueHandle_t s_event_queue = NULL;
static gateway_event_handler_t s_handlers[EVT_MAX_CODES][EVENT_BUS_MAX_HANDLERS];
static uint8_t s_handler_counts[EVT_MAX_CODES];
static TaskHandle_t s_dispatcher_task_handle = NULL;
static bool s_event_bus_frozen = false;

void event_bus_init(void)
{
    if (s_event_queue != NULL) {
        ESP_LOGW(TAG, "Event bus already initialized");
        return;
    }

    memset(s_handlers, 0, sizeof(s_handlers));
    memset(s_handler_counts, 0, sizeof(s_handler_counts));

    s_event_queue = xQueueCreate(EVENT_BUS_QUEUE_LEN, sizeof(gateway_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue!");
        return;
    }

    ESP_LOGI(TAG, "Event bus queue created (depth=%d)", EVENT_BUS_QUEUE_LEN);
}

bool event_bus_register_handler(gateway_event_code_t code, gateway_event_handler_t handler)
{
    if (code <= EVT_NONE || code >= EVT_MAX_CODES || handler == NULL) {
        ESP_LOGE(TAG, "Invalid event code (%d) or NULL handler", code);
        return false;
    }

    if (s_handler_counts[code] >= EVENT_BUS_MAX_HANDLERS) {
        ESP_LOGW(TAG, "Max handlers reached for event code %d (limit=%d)", code, EVENT_BUS_MAX_HANDLERS);
        return false;
    }

    s_handlers[code][s_handler_counts[code]++] = handler;
    ESP_LOGD(TAG, "Handler registered for event %d (count=%d)", code, s_handler_counts[code]);
    return true;
}

BaseType_t event_bus_post(gateway_event_code_t code, void *data, size_t len, bool from_isr)
{
    if (code <= EVT_NONE || code >= EVT_MAX_CODES) {
        if (data) free(data);
        return pdFALSE;
    }

    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Cannot post: Event bus not initialized");
        if (data) free(data);
        return pdFALSE;
    }

    gateway_event_t evt = {
        .code = code,
        .data = data,
        .len = len,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000ULL)
    };

    BaseType_t result;
    if (from_isr) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        result = xQueueSendFromISR(s_event_queue, &evt, &higher_priority_task_woken);
        if (higher_priority_task_woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        result = xQueueSend(s_event_queue, &evt, pdMS_TO_TICKS(100));
    }

    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full! Dropped event %d", code);
        // FSD §4.1.6: Giai phong bo nho ngay lap tuc neu hang doi day
        if (data) free(data);
    }

    return result;
}

static void event_dispatcher_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Event dispatcher task started on Core %d", xPortGetCoreID());
    gateway_event_t evt;

    while (1) {
        if (s_event_bus_frozen) {
            // FSD §4.13.4 FR-SUP-02: Deliberately freeze task to test watchdog stall detection
            vTaskDelay(portMAX_DELAY);
        }

        // Báo cáo nhịp tim tới Task Supervisor
        supervisor_report_heartbeat(TASK_HEARTBEAT_EVENT_BUS);

        // Đọc sự kiện với timeout 1000ms để đảm bảo định kỳ feed heartbeat
        if (xQueueReceive(s_event_queue, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (evt.code > EVT_NONE && evt.code < EVT_MAX_CODES) {
                uint8_t count = s_handler_counts[evt.code];
                for (uint8_t i = 0; i < count; i++) {
                    if (s_handlers[evt.code][i] != NULL) {
                        s_handlers[evt.code][i](&evt);
                    }
                }
            }

            // Quy tắc bắt buộc của kiến trúc FSD: Dispatcher sở hữu và giải phóng bộ nhớ payload
            if (evt.data != NULL) {
                free(evt.data);
                evt.data = NULL;
            }
        }
    }
}

void event_bus_start_dispatcher(void)
{
    if (s_dispatcher_task_handle != NULL) {
        ESP_LOGW(TAG, "Dispatcher task already running");
        return;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        event_dispatcher_task,
        "event_dispatcher",
        4096,
        NULL,
        EVENT_BUS_DISPATCHER_PRIO,
        &s_dispatcher_task_handle,
        EVENT_BUS_DISPATCHER_CORE
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event_dispatcher task!");
    } else {
        ESP_LOGI(TAG, "Event dispatcher initialized successfully");
    }
}

void event_bus_freeze(void)
{
    s_event_bus_frozen = true;
    printf("[TEST HOOK] Freezing event_bus task indefinitely (stopping heartbeat)... Watchdog timeout in 15s!\n");
    fflush(stdout);
}
