#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t friend_wifi_init(void);

esp_err_t friend_wifi_scan(void);

esp_err_t friend_wifi_start_config_ap(void);

esp_err_t friend_wifi_get_scan_results(wifi_ap_record_t *records, uint16_t *count);

#ifdef __cplusplus
}
#endif