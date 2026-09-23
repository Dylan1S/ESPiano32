#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "bluetooth_mgr.h"

static const char *TAG = "app_main";

static void on_midi_message(uint16_t ts_ms, const uint8_t *msg, uint16_t msg_len)
{
    ESP_LOGI(TAG, "MIDI EVT ts=%u len=%u", ts_ms, msg_len);

    char line[96] = {0};
    size_t pos = 0;
    for (int i = 0; i < msg_len; i++) {
        int written = snprintf(&line[pos], sizeof(line) - pos, "%02X ", msg[i]);
        if (written > 0) {
            pos += (size_t)written;
            if (pos >= sizeof(line)) {
                pos = sizeof(line) - 1;
            }
        }
        if (pos > (sizeof(line) - 4)) {
            ESP_LOGI(TAG, "%s", line);
            pos = 0;
            line[0] = 0;
        }
    }
    if (pos) {
        ESP_LOGI(TAG, "%s", line);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(bluetooth_mgr_start(on_midi_message));
}
