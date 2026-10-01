#include "friend_wifi.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include "friend_wifi_store.h"
#include "friend_http.h"

#define WIFI_CONNECTED_BIT       BIT0
#define WIFI_DISCONNECTED_BIT    BIT1
#define WIFI_MANUAL_REQUEST_BIT  BIT2


#define WIFI_MANAGER_STACK_SIZE  6144
#define WIFI_MANAGER_PRIORITY    5

#define WIFI_OFFLINE_SCAN_MS     5000
#define WIFI_CONNECT_TIMEOUT_MS  15000

static const char *TAG = "WIFI";

static EventGroupHandle_t wifi_event_group = NULL;
static TaskHandle_t wifi_manager_task_handle = NULL;

static volatile friend_wifi_state_t wifi_state = FRIEND_WIFI_STATE_IDLE;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_manager_task(void *arg);
static esp_err_t wifi_manager_scan_and_connect(void);
static esp_err_t wifi_manager_find_best_saved_network(char *ssid, size_t ssid_size, char *password, size_t password_size, int8_t *rssi);
static esp_err_t wifi_connect_internal(const char *ssid, const char *password);
static esp_err_t wifi_enter_manual_mode(void);
static void wifi_set_state(friend_wifi_state_t state);

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

    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_set_state(FRIEND_WIFI_STATE_IDLE);

    ESP_LOGI(TAG, "Wi-Fi initialized");

    return ESP_OK;
}

esp_err_t friend_wifi_manager_start(void)
{
    if (wifi_manager_task_handle != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t result = xTaskCreate(
        wifi_manager_task,
        "wifi_manager",
        WIFI_MANAGER_STACK_SIZE,
        NULL,
        WIFI_MANAGER_PRIORITY,
        &wifi_manager_task_handle
    );

    if (result != pdPASS)
    {
        wifi_manager_task_handle = NULL;

        ESP_LOGE(TAG, "Failed to create Wi-Fi manager task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Wi-Fi manager started");

    return ESP_OK;
}

friend_wifi_state_t friend_wifi_get_state(void)
{
    return wifi_state;
}

bool friend_wifi_is_online(void)
{
    return wifi_state == FRIEND_WIFI_STATE_ONLINE;
}

void friend_wifi_request_manual_mode(void)
{
    if (wifi_event_group == NULL)
    {
        return;
    }

    ESP_LOGI(TAG, "Manual Wi-Fi setup requested");

    xEventGroupSetBits(wifi_event_group, WIFI_MANUAL_REQUEST_BIT);
}

esp_err_t friend_wifi_scan(void)
{
    ESP_LOGI(TAG, "Scanning Wi-Fi networks");

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

    ret = esp_wifi_scan_get_ap_num(&ap_count);

    if (ret != ESP_OK)
    {
        return ret;
    }

    ESP_LOGI(TAG, "Found %u Wi-Fi networks", ap_count);

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

esp_err_t friend_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms)
{
    if (ssid == NULL || password == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(ssid) == 0 || strlen(ssid) > 32 || strlen(password) > 64)
    {
        return ESP_ERR_INVALID_ARG;
    }

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);

    esp_err_t ret = wifi_connect_internal(ssid, password);

    if (ret != ESP_OK)
    {
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (bits & WIFI_CONNECTED_BIT)
    {
        return ESP_OK;
    }

    if (bits & WIFI_DISCONNECTED_BIT)
    {
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
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

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enter APSTA mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure setup AP: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Config hotspot ready");
    ESP_LOGI(TAG, "SSID: %s", ap_config.ap.ssid);
    ESP_LOGI(TAG, "Password: none");

    return ESP_OK;
}
static void wifi_manager_task(void *arg)
{
    ESP_LOGI(TAG, "Wi-Fi manager running");

    while (1)
    {
        if (wifi_state == FRIEND_WIFI_STATE_MANUAL)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (wifi_state == FRIEND_WIFI_STATE_ONLINE)
        {
            EventBits_t bits = xEventGroupWaitBits(
                wifi_event_group,
                WIFI_DISCONNECTED_BIT | WIFI_MANUAL_REQUEST_BIT,
                pdTRUE,
                pdFALSE,
                portMAX_DELAY
            );

            if (bits & WIFI_MANUAL_REQUEST_BIT)
            {
                wifi_enter_manual_mode();
                continue;
            }

            if (bits & WIFI_DISCONNECTED_BIT)
            {
                wifi_set_state(FRIEND_WIFI_STATE_OFFLINE);

                ESP_LOGW(TAG, "Wi-Fi lost, returning to offline scan");
            }

            continue;
        }

        if (xEventGroupGetBits(wifi_event_group) & WIFI_MANUAL_REQUEST_BIT)
        {
            xEventGroupClearBits(wifi_event_group, WIFI_MANUAL_REQUEST_BIT);

            wifi_enter_manual_mode();
            continue;
        }

        esp_err_t ret = wifi_manager_scan_and_connect();

        if (ret == ESP_OK)
        {
            continue;
        }

        if (wifi_state == FRIEND_WIFI_STATE_MANUAL)
        {
            continue;
        }

        wifi_set_state(FRIEND_WIFI_STATE_OFFLINE);

        ESP_LOGI(TAG, "Offline, next Wi-Fi scan in %u ms", WIFI_OFFLINE_SCAN_MS);

        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_MANUAL_REQUEST_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_OFFLINE_SCAN_MS)
        );

        if (bits & WIFI_MANUAL_REQUEST_BIT)
        {
            wifi_enter_manual_mode();
        }
    }
}

static esp_err_t wifi_manager_scan_and_connect(void)
{
    char ssid[FRIEND_WIFI_SSID_MAX_LEN + 1] = {0};
    char password[FRIEND_WIFI_PASSWORD_MAX_LEN + 1] = {0};
    int8_t rssi = -127;

    wifi_set_state(FRIEND_WIFI_STATE_SCANNING);

    ESP_LOGI(TAG, "Looking for saved Wi-Fi networks");

    esp_err_t ret = wifi_manager_find_best_saved_network(ssid, sizeof(ssid), password, sizeof(password), &rssi);

    if (ret == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No saved Wi-Fi network is currently visible");
        return ESP_ERR_NOT_FOUND;
    }

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Saved Wi-Fi scan failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Best saved Wi-Fi: %s, RSSI=%d", ssid, rssi);

    wifi_set_state(FRIEND_WIFI_STATE_CONNECTING);

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);

    ret = wifi_connect_internal(ssid, password);

    memset(password, 0, sizeof(password));

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to start connection: %s", esp_err_to_name(ret));
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | WIFI_MANUAL_REQUEST_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS)
    );

    if (bits & WIFI_MANUAL_REQUEST_BIT)
    {
        ESP_LOGI(TAG, "Connection interrupted by manual setup request");

        esp_wifi_disconnect();

        return wifi_enter_manual_mode();
    }

    if (bits & WIFI_CONNECTED_BIT)
    {
        wifi_set_state(FRIEND_WIFI_STATE_ONLINE);

        ESP_LOGI(TAG, "Wi-Fi online: %s", ssid);

        return ESP_OK;
    }

    if (bits & WIFI_DISCONNECTED_BIT)
    {
        ESP_LOGW(TAG, "Connection failed: %s", ssid);
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Connection timeout: %s", ssid);

    esp_wifi_disconnect();

    return ESP_ERR_TIMEOUT;
}

static esp_err_t wifi_manager_find_best_saved_network(char *ssid, size_t ssid_size, char *password, size_t password_size, int8_t *rssi)
{
    if (ssid == NULL || password == NULL || rssi == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t saved_count = friend_wifi_store_count();

    if (saved_count == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }

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
        return ret;
    }

    uint16_t ap_count = 0;

    ret = esp_wifi_scan_get_ap_num(&ap_count);

    if (ret != ESP_OK)
    {
        return ret;
    }

    if (ap_count == 0)
    {
        return ESP_ERR_NOT_FOUND;
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

    bool found = false;
    int8_t best_rssi = -127;
    char best_ssid[FRIEND_WIFI_SSID_MAX_LEN + 1] = {0};

    for (uint16_t i = 0; i < record_count; i++)
    {
        const char *candidate_ssid = (const char *)records[i].ssid;

        if (candidate_ssid[0] == '\0')
        {
            continue;
        }

        if (!friend_wifi_store_is_saved(candidate_ssid))
        {
            continue;
        }

        if (!found || records[i].rssi > best_rssi)
        {
            found = true;
            best_rssi = records[i].rssi;
            strlcpy(best_ssid, candidate_ssid, sizeof(best_ssid));
        }
    }

    free(records);

    if (!found)
    {
        return ESP_ERR_NOT_FOUND;
    }

    ret = friend_wifi_store_get(best_ssid, password, password_size);

    if (ret != ESP_OK)
    {
        return ret;
    }

    strlcpy(ssid, best_ssid, ssid_size);
    *rssi = best_rssi;

    return ESP_OK;
}

static esp_err_t wifi_connect_internal(const char *ssid, const char *password)
{
    wifi_config_t sta_config = {0};

    strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

    sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    sta_config.sta.threshold.rssi = -127;
    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure STA: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Connecting to saved Wi-Fi: %s", ssid);

    ret = esp_wifi_connect();

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static void wifi_set_state(friend_wifi_state_t state)
{
    wifi_state = state;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupClearBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);

        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG, "STA disconnected, reason=%u", event->reason);

        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
    }
}

static esp_err_t wifi_enter_manual_mode(void)
{
    if (wifi_state == FRIEND_WIFI_STATE_MANUAL)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Entering manual Wi-Fi setup");

    wifi_set_state(FRIEND_WIFI_STATE_MANUAL);

    esp_err_t ret = friend_wifi_start_config_ap();

    if (ret != ESP_OK)
    {
        wifi_set_state(FRIEND_WIFI_STATE_OFFLINE);

        ESP_LOGE(TAG, "Failed to start manual setup AP");
        return ret;
    }

    ret = friend_http_start();

    if (ret != ESP_OK)
    {
        wifi_set_state(FRIEND_WIFI_STATE_OFFLINE);

        ESP_LOGE(TAG, "Failed to start provisioning HTTP server");
        return ret;
    }

    ESP_LOGI(TAG, "Manual Wi-Fi setup ready");
    ESP_LOGI(TAG, "Connect to Moc-Setup and open http://192.168.4.1");

    return ESP_OK;
}