#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"

#include "board/board_display.h"
#include "board/board_buttons.h"
#include "board/board_audio.h"
#include "ui/friend_ui.h"
#include "network/friend_wifi.h"
#include "network/friend_http.h"


static const char *TAG = "FriendOS";


void app_main(void)
{
    ESP_LOGI(TAG, "Starting FriendOS");

    ESP_ERROR_CHECK(board_display_init());
    ESP_ERROR_CHECK(friend_ui_init());
    ESP_ERROR_CHECK(board_buttons_init());
    ESP_ERROR_CHECK(board_audio_init());
    ESP_ERROR_CHECK(friend_wifi_init());
    ESP_ERROR_CHECK(friend_wifi_scan());
    ESP_ERROR_CHECK(friend_wifi_start_config_ap());
    ESP_ERROR_CHECK(friend_http_start());

    friend_ui_set_idle_actions(true);

    ESP_LOGI(TAG, "Moc is awake!");

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}