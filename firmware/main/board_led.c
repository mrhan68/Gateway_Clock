#include "board_led.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BOARD_LED";

void board_led_set_rgb(gpio_num_t gpio, uint8_t r, uint8_t g, uint8_t b)
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 64,
        .resolution_hz = 10000000, // 10MHz (1 tick = 0.1us)
        .trans_queue_depth = 4,
    };
    rmt_channel_handle_t tx_channel = NULL;
    if (rmt_new_tx_channel(&tx_chan_config, &tx_channel) != ESP_OK) {
        return;
    }

    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = 10000000,
    };
    if (rmt_new_led_strip_encoder(&encoder_config, &led_encoder) != ESP_OK) {
        rmt_del_channel(tx_channel);
        return;
    }

    rmt_enable(tx_channel);

    // Chuẩn bị mảng 4 pixels GRB (Green, Red, Blue) để tắt sạch toàn bộ chuỗi LED
    uint8_t pixels[12];
    for (int i = 0; i < 4; i++) {
        pixels[i * 3 + 0] = g;
        pixels[i * 3 + 1] = r;
        pixels[i * 3 + 2] = b;
    }

    rmt_transmit_config_t tx_config = { .loop_count = 0 };
    rmt_transmit(tx_channel, led_encoder, pixels, sizeof(pixels), &tx_config);
    rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);

    rmt_disable(tx_channel);
    rmt_del_encoder(led_encoder);
    rmt_del_channel(tx_channel);
}

void board_led_all_off(void)
{
    static const gpio_num_t candidate_pins[] = {
        48, 38, 21, 47, 14, 2, 3, 5, 6, 7, 35, 36, 37, 39, 40, 41, 42, 45, 46
    };

    for (size_t i = 0; i < sizeof(candidate_pins) / sizeof(candidate_pins[0]); i++) {
        gpio_num_t pin = candidate_pins[i];
        board_led_set_rgb(pin, 0, 0, 0);
    }

    ESP_LOGI(TAG, "Official RMT led_strip_encoder OFF sent to %d candidate pins.",
             (int)(sizeof(candidate_pins) / sizeof(candidate_pins[0])));
}

void board_led_init(void)
{
    board_led_all_off();
}
