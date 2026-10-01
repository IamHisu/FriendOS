#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FRIEND_WIFI_STATE_IDLE = 0,
    FRIEND_WIFI_STATE_SCANNING,
    FRIEND_WIFI_STATE_CONNECTING,
    FRIEND_WIFI_STATE_ONLINE,
    FRIEND_WIFI_STATE_OFFLINE,
    FRIEND_WIFI_STATE_MANUAL
} friend_wifi_state_t;

esp_err_t friend_wifi_init(void);
esp_err_t friend_wifi_manager_start(void);

friend_wifi_state_t friend_wifi_get_state(void);
bool friend_wifi_is_online(void);

void friend_wifi_request_manual_mode(void);

esp_err_t friend_wifi_scan(void);
esp_err_t friend_wifi_get_scan_results(wifi_ap_record_t *records, uint16_t *count);
esp_err_t friend_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);
esp_err_t friend_wifi_start_config_ap(void);

#ifdef __cplusplus
}
#endif