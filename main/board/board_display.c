#include "board_display.h"

#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define LCD_HOST       SPI2_HOST

#define LCD_MOSI       41
#define LCD_SCLK       42
#define LCD_CS         21
#define LCD_DC         40
#define LCD_RST        45
#define LCD_BL         20

#define LCD_WIDTH      240
#define LCD_HEIGHT     240

static const char *TAG = "board_display";

static esp_lcd_panel_handle_t s_panel = NULL;

static void fill_rect(int x1, int y1, int x2, int y2, uint16_t color)
{
    int width = x2 - x1;
    int height = y2 - y1;

    if (width <= 0 || height <= 0) {
        return;
    }

    size_t pixel_count = width * height;
    uint8_t *buffer = malloc(pixel_count * 2);

    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer");
        return;
    }

    // Confirmed working on this board:
    // RGB565 high byte first, low byte second.
    uint8_t high = color >> 8;
    uint8_t low = color & 0xFF;

    for (size_t i = 0; i < pixel_count; i++) {
        buffer[i * 2] = high;
        buffer[i * 2 + 1] = low;
    }

    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(
            s_panel,
            x1,
            y1,
            x2,
            y2,
            buffer
        )
    );

    free(buffer);
}

static void draw_test_face(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t black = 0x0000;

    fill_rect(0, 0, 240, 240, bg);

    fill_rect(65, 80, 85, 105, black);
    fill_rect(155, 80, 175, 105, black);

    fill_rect(85, 150, 95, 160, black);
    fill_rect(95, 160, 145, 170, black);
    fill_rect(145, 150, 155, 160, black);
}

esp_err_t board_display_init(void)
{
    ESP_LOGI(TAG, "Initializing Zhengchen display");

    gpio_config_t bl_config = {
        .pin_bit_mask = 1ULL << LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&bl_config));

    // Keep backlight off during initialization.
    gpio_set_level(LCD_BL, 0);

    spi_bus_config_t bus_config = {
        .sclk_io_num = LCD_SCLK,
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            LCD_HOST,
            &bus_config,
            SPI_DMA_CH_AUTO
        )
    );

    esp_lcd_panel_io_handle_t io_handle = NULL;

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = 20 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi(
            (esp_lcd_spi_bus_handle_t)LCD_HOST,
            &io_config,
            &io_handle
        )
    );

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_st7789(
            io_handle,
            &panel_config,
            &s_panel
        )
    );

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    // Required by this specific panel.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    gpio_set_level(LCD_BL, 1);

    ESP_LOGI(TAG, "Display initialized");

    // Temporary hardware regression test.
    draw_test_face();

    return ESP_OK;
}