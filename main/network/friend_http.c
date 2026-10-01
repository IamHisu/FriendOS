#include "friend_http.h"

#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "friend_wifi.h"

static const char *TAG = "HTTP";
static httpd_handle_t server = NULL;

static const char setup_page[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Moc Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;background:#f4f4f4;text-align:center;padding:40px 20px;color:#222;}"
".card{max-width:420px;margin:auto;background:white;padding:30px;border-radius:18px;box-shadow:0 4px 20px #0002;}"
"h1{margin-bottom:10px;}"
"p{color:#666;line-height:1.6;}"
".status{margin-top:24px;padding:14px;background:#eaf8ee;border-radius:12px;}"
"</style>"
"</head>"
"<body>"
"<div class=\"card\">"
"<h1>Moc Setup</h1>"
"<p>Moc da san sang ket noi Wi-Fi.</p>"
"<div class=\"status\">HTTP server is working!</div>"
"</div>"
"</body>"
"</html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, setup_page, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    wifi_ap_record_t records[20];
    uint16_t count = 20;

    esp_err_t ret = friend_wifi_get_scan_results(records, &count);

    if (ret != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
        return ret;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"networks\":[");

    for (uint16_t i = 0; i < count; i++)
    {
        char item[160];

        snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%u}",
            i > 0 ? "," : "",
            (char *)records[i].ssid,
            records[i].rssi,
            records[i].primary);

        httpd_resp_sendstr_chunk(req, item);
    }

    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);

    ESP_LOGI(TAG, "GET /api/wifi/scan -> %u networks", count);

    return ESP_OK;
}

esp_err_t friend_http_start(void)
{
    if (server != NULL)
    {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting HTTP server with stack=%u", config.stack_size);

    esp_err_t ret = httpd_start(&server, &config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        server = NULL;
        return ret;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &root_uri);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register root handler: %s", esp_err_to_name(ret));
        httpd_stop(server);
        server = NULL;
        return ret;
    }

    httpd_uri_t wifi_scan_uri = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = wifi_scan_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &wifi_scan_uri);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register Wi-Fi scan handler: %s", esp_err_to_name(ret));
        httpd_stop(server);
        server = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "HTTP server ready");
    ESP_LOGI(TAG, "Open http://192.168.4.1");

    return ESP_OK;
}