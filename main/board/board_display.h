#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the board display.
 *
 * Initializes:
 * - Backlight GPIO
 * - SPI bus
 * - ST7789 LCD
 * - LVGL
 * - LVGL display buffers
 *
 * @return ESP_OK on success.
 */
esp_err_t board_display_init(void);

#ifdef __cplusplus
}
#endif