#include "friend_http.h"

#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "friend_wifi.h"
#include "friend_wifi_store.h"

static const char *TAG = "HTTP";

static httpd_handle_t server = NULL;

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

extern const char style_css_start[] asm("_binary_style_css_start");
extern const char style_css_end[] asm("_binary_style_css_end");

extern const char app_js_start[] asm("_binary_app_js_start");
extern const char app_js_end[] asm("_binary_app_js_end");

static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t style_css_handler(httpd_req_t *req);
static esp_err_t app_js_handler(httpd_req_t *req);
static esp_err_t wifi_scan_handler(httpd_req_t *req);
static esp_err_t wifi_connect_handler(httpd_req_t *req);

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }

    return -1;
}

static void url_decode(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || src == NULL || dst_size == 0)
    {
        return;
    }

    size_t di = 0;

    for (size_t si = 0; src[si] != '\0' && di + 1 < dst_size; si++)
    {
        if (src[si] == '+')
        {
            dst[di++] = ' ';
            continue;
        }

        if (src[si] == '%' && src[si + 1] != '\0' && src[si + 2] != '\0')
        {
            int high = hex_value(src[si + 1]);
            int low = hex_value(src[si + 2]);

            if (high >= 0 && low >= 0)
            {
                dst[di++] = (char)((high << 4) | low);
                si += 2;
                continue;
            }
        }

        dst[di++] = src[si];
    }

    dst[di] = '\0';
}

static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /");

    size_t length = index_html_end - index_html_start;

    if (length > 0 && index_html_start[length - 1] == '\0')
    {
        length--;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, length);
}

static esp_err_t style_css_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /style.css");

    size_t length = style_css_end - style_css_start;

    if (length > 0 && style_css_start[length - 1] == '\0')
    {
        length--;
    }

    httpd_resp_set_type(req, "text/css; charset=utf-8");
    return httpd_resp_send(req, style_css_start, length);
}

static esp_err_t app_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /app.js");

    size_t length = app_js_end - app_js_start;

    if (length > 0 && app_js_start[length - 1] == '\0')
    {
        length--;
    }

    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    return httpd_resp_send(req, app_js_start, length);
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

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len >= 256)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    char body[256] = {0};

    size_t received = 0;

    while (received < req->content_len)
    {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);

        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            continue;
        }

        if (ret <= 0)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
            return ESP_FAIL;
        }

        received += ret;
    }

    body[received] = '\0';

    char encoded_ssid[96] = {0};
    char encoded_password[128] = {0};

    esp_err_t ret = httpd_query_key_value(body, "ssid", encoded_ssid, sizeof(encoded_ssid));

    if (ret != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ret;
    }

    ret = httpd_query_key_value(body, "password", encoded_password, sizeof(encoded_password));

    if (ret != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing password");
        return ret;
    }

    char ssid[33] = {0};
    char password[65] = {0};

    url_decode(ssid, sizeof(ssid), encoded_ssid);
    url_decode(password, sizeof(password), encoded_password);

    if (strlen(ssid) == 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "POST /api/wifi/connect SSID=%s", ssid);

    ret = friend_wifi_connect(ssid, password, 15000);

    if (ret == ESP_OK)
    {
        ret = friend_wifi_store_save(ssid, password);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Wi-Fi credentials verified and saved");

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\":true}");

            return ESP_OK;
        }

        ESP_LOGE(TAG, "Wi-Fi connected but failed to save credentials: %s", esp_err_to_name(ret));

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false}");

        return ESP_OK;
    }

    ESP_LOGW(TAG, "Wi-Fi credentials rejected, not saved");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":false}");

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

    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };

    const httpd_uri_t style_css_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = style_css_handler,
        .user_ctx = NULL,
    };

    const httpd_uri_t app_js_uri = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = app_js_handler,
        .user_ctx = NULL,
    };

    const httpd_uri_t wifi_scan_uri = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = wifi_scan_handler,
        .user_ctx = NULL,
    };

    const httpd_uri_t wifi_connect_uri = {
        .uri = "/api/wifi/connect",
        .method = HTTP_POST,
        .handler = wifi_connect_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &style_css_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &app_js_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_scan_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_connect_uri));

    ESP_LOGI(TAG, "HTTP server ready");
    ESP_LOGI(TAG, "Open http://192.168.4.1");

    return ESP_OK;
}