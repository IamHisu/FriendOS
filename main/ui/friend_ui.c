#include "friend_ui.h"

#include <stdint.h>

#include "esp_log.h"
#include "esp_random.h"

#include "lvgl.h"

static const char *TAG = "friend_ui";


// ============================================================
// Face configuration
// ============================================================

#define EYE_WIDTH           48
#define EYE_HEIGHT_OPEN     68
#define EYE_HEIGHT_CLOSED    8

#define LEFT_EYE_X          48
#define RIGHT_EYE_X        144
#define EYE_CENTER_Y       104

#define BLINK_DURATION_MS   110

#define BLINK_MIN_DELAY_MS  2000
#define BLINK_MAX_DELAY_MS  4500

#define EYE_COLOR           0x45E6FF
#define HIGHLIGHT_COLOR     0xDFFFFF
#define BACKGROUND_COLOR    0x05080A


// ============================================================
// Face objects
// ============================================================

static lv_obj_t *s_left_eye = NULL;
static lv_obj_t *s_right_eye = NULL;

static lv_obj_t *s_left_highlight = NULL;
static lv_obj_t *s_right_highlight = NULL;

static lv_obj_t *s_mouth = NULL;

static lv_timer_t *s_blink_timer = NULL;

static bool s_eyes_closed = false;


// ============================================================
// Helpers
// ============================================================

static lv_obj_t *create_face_part(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    uint32_t color,
    int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);

    lv_obj_set_style_bg_color(
        obj,
        lv_color_hex(color),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        obj,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        obj,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        obj,
        radius,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        obj,
        0,
        LV_PART_MAIN
    );

    lv_obj_remove_flag(
        obj,
        LV_OBJ_FLAG_SCROLLABLE
    );

    return obj;
}


// ============================================================
// Eye control
// ============================================================

static void set_eye_height(
    lv_obj_t *eye,
    int x,
    int height)
{
    /*
     * Keep the eye vertically centered while changing height.
     */
    int y = EYE_CENTER_Y - (height / 2);

    lv_obj_set_pos(
        eye,
        x,
        y
    );

    lv_obj_set_size(
        eye,
        EYE_WIDTH,
        height
    );
}


static void eyes_open(void)
{
    set_eye_height(
        s_left_eye,
        LEFT_EYE_X,
        EYE_HEIGHT_OPEN
    );

    set_eye_height(
        s_right_eye,
        RIGHT_EYE_X,
        EYE_HEIGHT_OPEN
    );

    // Show highlights again.
    lv_obj_remove_flag(
        s_left_highlight,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_obj_remove_flag(
        s_right_highlight,
        LV_OBJ_FLAG_HIDDEN
    );

    s_eyes_closed = false;
}


static void eyes_close(void)
{
    /*
     * Hide highlights while blinking.
     */
    lv_obj_add_flag(
        s_left_highlight,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_obj_add_flag(
        s_right_highlight,
        LV_OBJ_FLAG_HIDDEN
    );

    set_eye_height(
        s_left_eye,
        LEFT_EYE_X,
        EYE_HEIGHT_CLOSED
    );

    set_eye_height(
        s_right_eye,
        RIGHT_EYE_X,
        EYE_HEIGHT_CLOSED
    );

    s_eyes_closed = true;
}


// ============================================================
// Blink timing
// ============================================================

static uint32_t get_random_blink_delay(void)
{
    uint32_t range =
        BLINK_MAX_DELAY_MS -
        BLINK_MIN_DELAY_MS;

    return BLINK_MIN_DELAY_MS +
           (esp_random() % (range + 1));
}


static void blink_timer_cb(lv_timer_t *timer)
{
    if (!s_eyes_closed) {

        // Close eyes.
        eyes_close();

        /*
         * Run this timer again shortly
         * to reopen the eyes.
         */
        lv_timer_set_period(
            timer,
            BLINK_DURATION_MS
        );

    } else {

        // Open eyes.
        eyes_open();

        /*
         * Randomize the next blink so Moc
         * doesn't look mechanical.
         */
        lv_timer_set_period(
            timer,
            get_random_blink_delay()
        );
    }

    /*
     * Apply the new period starting now.
     */
    lv_timer_reset(timer);
}


// ============================================================
// Public API
// ============================================================

esp_err_t friend_ui_init(void)
{
    ESP_LOGI(
        TAG,
        "Initializing Moc UI"
    );

    lv_obj_t *screen =
        lv_screen_active();


    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(BACKGROUND_COLOR),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    // --------------------------------------------------------
    // Eyes
    // --------------------------------------------------------

    s_left_eye = create_face_part(
        screen,
        LEFT_EYE_X,
        EYE_CENTER_Y - EYE_HEIGHT_OPEN / 2,
        EYE_WIDTH,
        EYE_HEIGHT_OPEN,
        EYE_COLOR,
        22
    );

    s_right_eye = create_face_part(
        screen,
        RIGHT_EYE_X,
        EYE_CENTER_Y - EYE_HEIGHT_OPEN / 2,
        EYE_WIDTH,
        EYE_HEIGHT_OPEN,
        EYE_COLOR,
        22
    );


    // --------------------------------------------------------
    // Eye highlights
    // --------------------------------------------------------

    s_left_highlight = create_face_part(
        s_left_eye,
        11, 10,
        12, 18,
        HIGHLIGHT_COLOR,
        8
    );

    s_right_highlight = create_face_part(
        s_right_eye,
        11, 10,
        12, 18,
        HIGHLIGHT_COLOR,
        8
    );


    // --------------------------------------------------------
    // Mouth
    // --------------------------------------------------------

    s_mouth = create_face_part(
        screen,
        100, 169,
        40, 8,
        EYE_COLOR,
        4
    );


    // --------------------------------------------------------
    // Automatic blinking
    // --------------------------------------------------------

    s_blink_timer = lv_timer_create(
        blink_timer_cb,
        get_random_blink_delay(),
        NULL
    );

    if (s_blink_timer == NULL) {
        ESP_LOGE(
            TAG,
            "Failed to create blink timer"
        );

        return ESP_FAIL;
    }


    ESP_LOGI(
        TAG,
        "Moc face created - blinking enabled"
    );

    return ESP_OK;
}