#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"

#include "board/board_display.h"
#include "ui/friend_ui.h"


static const char *TAG = "FriendOS";


static void ui_wait(uint32_t ms)
{
    uint32_t elapsed = 0;

    while (elapsed < ms)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "Starting FriendOS");

    ESP_ERROR_CHECK(board_display_init());
    ESP_ERROR_CHECK(friend_ui_init());

    friend_ui_set_idle_actions(false);

    ESP_LOGI(TAG, "Moc is awake!");
    ESP_LOGI(TAG, "Starting full UI test");

    // ============================================================
    // Test all emotions
    // ============================================================

    ESP_LOGI(TAG, "Emotion: NEUTRAL");
    friend_ui_set_emotion(FRIEND_EMOTION_NEUTRAL);
    ui_wait(4000);

    ESP_LOGI(TAG, "Emotion: HAPPY");
    friend_ui_set_emotion(FRIEND_EMOTION_HAPPY);
    ui_wait(4000);

    ESP_LOGI(TAG, "Emotion: SLEEPY");
    friend_ui_set_emotion(FRIEND_EMOTION_SLEEPY);
    ui_wait(4000);

    ESP_LOGI(TAG, "Emotion: ANGRY");
    friend_ui_set_emotion(FRIEND_EMOTION_ANGRY);
    ui_wait(4000);

    ESP_LOGI(TAG, "Emotion: SURPRISED");
    friend_ui_set_emotion(FRIEND_EMOTION_SURPRISED);
    ui_wait(4000);

    ESP_LOGI(TAG, "Emotion: MISCHIEVOUS");
    friend_ui_set_emotion(FRIEND_EMOTION_MISCHIEVOUS);
    ui_wait(4000);

    // ============================================================
    // Return to neutral before testing actions
    // ============================================================

    ESP_LOGI(TAG, "Returning to NEUTRAL");
    friend_ui_set_emotion(FRIEND_EMOTION_NEUTRAL);
    ui_wait(2000);

    // ============================================================
    // Test all actions
    // ============================================================

    ESP_LOGI(TAG, "Action: SHAKE");
    friend_ui_play_action(FRIEND_ACTION_SHAKE);
    ui_wait(1500);

    ESP_LOGI(TAG, "Action: NOD");
    friend_ui_play_action(FRIEND_ACTION_NOD);
    ui_wait(1500);

    ESP_LOGI(TAG, "Action: BOUNCE");
    friend_ui_play_action(FRIEND_ACTION_BOUNCE);
    ui_wait(1800);

    ESP_LOGI(TAG, "Action: WINK");
    friend_ui_play_action(FRIEND_ACTION_WINK);
    ui_wait(1700);

    ESP_LOGI(TAG, "Action: LOOK_AROUND");
    friend_ui_play_action(FRIEND_ACTION_LOOK_AROUND);
    ui_wait(3000);

    ESP_LOGI(TAG, "Action: STARTLE");
    friend_ui_play_action(FRIEND_ACTION_STARTLE);
    ui_wait(1600);

    ESP_LOGI(TAG, "Action: GIGGLE");
    friend_ui_play_action(FRIEND_ACTION_GIGGLE);
    ui_wait(2000);

    ESP_LOGI(TAG, "Action: SNEAKY");
    friend_ui_play_action(FRIEND_ACTION_SNEAKY);
    ui_wait(2400);

    // ============================================================
    // Test complete
    // ============================================================

    ESP_LOGI(TAG, "Full UI test complete");

    friend_ui_set_emotion(FRIEND_EMOTION_NEUTRAL);

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}