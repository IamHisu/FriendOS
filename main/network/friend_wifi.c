#include "friend_wifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "nvs_flash.h"


static const char *TAG = "WIFI";


esp_err_t friend_wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialized");

    return ESP_OK;
}


esp_err_t friend_wifi_scan(void)
{
    ESP_LOGI(TAG, "Scanning Wi-Fi networks...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t ap_count = 0;

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    ESP_LOGI(TAG, "Found %u Wi-Fi networks", ap_count);

    if (ap_count == 0)
    {
        return ESP_OK;
    }

    wifi_ap_record_t *records = calloc(ap_count, sizeof(wifi_ap_record_t));

    if (records == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    uint16_t record_count = ap_count;

    ret = esp_wifi_scan_get_ap_records(&record_count, records);

    if (ret != ESP_OK)
    {
        free(records);
        return ret;
    }

    for (uint16_t i = 0; i < record_count; i++)
    {
        ESP_LOGI(TAG, "[%u] SSID=\"%s\" RSSI=%d channel=%u", i + 1, (char *)records[i].ssid, records[i].rssi, records[i].primary);
    }

    free(records);

    return ESP_OK;
}

esp_err_t friend_wifi_start_config_ap(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi config hotspot");

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Moc-Setup",
            .ssid_len = 0,
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Config hotspot ready");
    ESP_LOGI(TAG, "SSID: %s", ap_config.ap.ssid);
    ESP_LOGI(TAG, "Password: none");

    return ESP_OK;
}

esp_err_t friend_wifi_get_scan_results(wifi_ap_record_t *records, uint16_t *count)
{
    if (records == NULL || count == NULL || *count == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    ESP_LOGI(TAG, "Scanning Wi-Fi from config mode");

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_scan_get_ap_records(count, records);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Config scan found %u networks", *count);

    return ESP_OK;
}