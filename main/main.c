#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "board/board_display.h"

static const char *TAG = "FriendOS";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting FriendOS");

    ESP_ERROR_CHECK(board_display_init());

    ESP_LOGI(TAG, "Moc is awake!");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}