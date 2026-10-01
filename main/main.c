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
#include "network/friend_wifi_store.h"

static const char *TAG = "FriendOS";

#define FRIEND_UI_TASK_STACK_SIZE 8192
#define FRIEND_UI_TASK_PRIORITY   4
#define FRIEND_UI_TASK_DELAY_MS   10
#define FRIEND_UI_STACK_LOG_MS    10000

static void friend_ui_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "UI task started, stack=%d bytes", FRIEND_UI_TASK_STACK_SIZE);

    TickType_t last_stack_log = xTaskGetTickCount();

    while (1)
    {
        lv_timer_handler();

        TickType_t now = xTaskGetTickCount();

        if ((now - last_stack_log) >= pdMS_TO_TICKS(FRIEND_UI_STACK_LOG_MS))
        {
            UBaseType_t stack_free = uxTaskGetStackHighWaterMark(NULL);

            ESP_LOGI(TAG, "UI task minimum free stack: %u bytes", (unsigned int)stack_free);

            last_stack_log = now;
        }

        vTaskDelay(pdMS_TO_TICKS(FRIEND_UI_TASK_DELAY_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting FriendOS");

    ESP_ERROR_CHECK(board_display_init());
    ESP_ERROR_CHECK(friend_ui_init());
    ESP_ERROR_CHECK(board_buttons_init());
    ESP_ERROR_CHECK(board_audio_init());

    ESP_ERROR_CHECK(friend_wifi_init());
    ESP_ERROR_CHECK(friend_wifi_store_init());
    ESP_ERROR_CHECK(friend_wifi_manager_start());

    board_buttons_set_boot_long_press_callback(friend_wifi_request_manual_mode);

    friend_ui_set_idle_actions(true);

    UBaseType_t main_stack_free = uxTaskGetStackHighWaterMark(NULL);

    ESP_LOGI(TAG, "Main task minimum free stack before UI task: %u bytes", (unsigned int)main_stack_free);

    BaseType_t task_result = xTaskCreate(friend_ui_task, "friend_ui", FRIEND_UI_TASK_STACK_SIZE, NULL, FRIEND_UI_TASK_PRIORITY, NULL);

    if (task_result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create UI task");
        return;
    }

    ESP_LOGI(TAG, "Moc is awake!");
    ESP_LOGI(TAG, "FriendOS initialization complete");
}