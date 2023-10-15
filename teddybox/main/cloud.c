
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_tls.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "cloud.h"

#define CLOUD_HOST "192.168.1.26"

static const char *TAG = "Cloud";

uint8_t *ca_der = NULL;
uint8_t *client_der = NULL;
uint8_t *private_der = NULL;
size_t ca_der_len = 0;
size_t client_der_len = 0;
size_t private_der_len = 0;
static esp_app_desc_t running_app_info;

esp_err_t http_parser_handler(void *ctx, uint8_t *data, size_t length)
{
    esp_err_t ret = ESP_OK;
    http_parser_t *parser_ctx = (http_parser_t *)ctx;

    if (!parser_ctx->header_parsed)
    {
        if (parser_ctx->header_size + length > MAX_HTTP_HEADER_SIZE)
        {
            ESP_LOGI(TAG, "Buffer overflow");
            return ESP_FAIL;
        }

        memcpy(parser_ctx->header_buffer + parser_ctx->header_size, data, length);
        parser_ctx->header_size += length;

        char *header_end = strstr((char *)parser_ctx->header_buffer, "\r\n\r\n");
        if (header_end)
        {
            ESP_LOGI(TAG, "Found end of header");

            int status_code;
            if (sscanf((char *)parser_ctx->header_buffer, "HTTP/1.1 %d ", &status_code) == 1)
            {
                ESP_LOGI(TAG, "Parsed HTTP Status Code: %d", status_code);
                if (parser_ctx->http_status_cbr)
                {
                    ret = parser_ctx->http_status_cbr(parser_ctx->ctx, status_code);
                }
            }

            char *content_length_str = strstr((char *)parser_ctx->header_buffer, "Content-Length: ");
            if (content_length_str)
            {
                sscanf(content_length_str, "Content-Length: %zu", &parser_ctx->content_length);
                ESP_LOGI(TAG, "Parsed Content-Length: %zu", parser_ctx->content_length);

                if (parser_ctx->content_length_cbr)
                {
                    parser_ctx->content_length_cbr(parser_ctx->ctx, parser_ctx->content_length);
                }
            }
            parser_ctx->header_parsed = 1;

            uint8_t *content_start = (uint8_t *)(header_end + 4);
            size_t content_size = parser_ctx->header_size - (content_start - parser_ctx->header_buffer);

            if (parser_ctx->http_data_cbr)
            {
                ret = parser_ctx->http_data_cbr(parser_ctx->ctx, content_start, content_size);
            }
        }
    }
    else
    {
        parser_ctx->received_data_length += length;

        if (parser_ctx->http_data_cbr)
        {
            ret = parser_ctx->http_data_cbr(parser_ctx->ctx, data, length);
        }

        if (parser_ctx->received_data_length >= parser_ctx->content_length)
        {
            if (parser_ctx->http_end_cbr)
            {
                parser_ctx->http_end_cbr(parser_ctx->ctx);
            }
            ret = CLOUD_SUCCESS_DISCONNECT;
        }
    }

    return ret;
}

esp_err_t cloud_load_cert(const char *path, uint8_t **ptr, size_t *length)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        ESP_LOGE(TAG, "Certificate %s not found", path);
        return ESP_FAIL;
    }

    *ptr = malloc(st.st_size);
    *length = st.st_size;

    FILE *ca = fopen(path, "rb");
    if (!ca)
    {
        ESP_LOGE(TAG, "Certificate %s not found", path);
        return ESP_FAIL;
    }

    if (fread(*ptr, *length, 1, ca) != 1)
    {
        ESP_LOGE(TAG, "Failed to read certificate");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Loaded '%s' with %d bytes", path, *length);

    return ESP_OK;
}

static esp_err_t cloud_request(cloud_req_t *req)
{
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "New request");

    char *auth_line = strdup("");
    char *request;
    char *url;

    if (req->auth)
    {
        free(auth_line);
        asprintf(&auth_line, "Authorization: BD %s\r\n", req->auth);
    }
    asprintf(&url, "https://%s:%d%s", req->host, req->port, req->path);
    ESP_LOGI(TAG, "Connect to %s...", url);

    asprintf(&request, "GET %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "%s"
                       "User-Agent: teddybox/1.0 (ESP32) %s\r\n"
                       "\r\n",
             req->path, req->host, auth_line, running_app_info.version);

    esp_tls_cfg_t cfg = {
        .cacert_buf = ca_der,
        .cacert_bytes = ca_der_len,
        .clientcert_buf = client_der,
        .clientcert_bytes = client_der_len,
        .clientkey_buf = private_der,
        .clientkey_bytes = private_der_len,
        .skip_common_name = true};

    uint8_t *receive_buffer = malloc(HTTP_RECEIVE_SIZE);
    struct esp_tls *tls = esp_tls_conn_http_new(url, &cfg);

    if (!tls)
    {
        ESP_LOGE(TAG, "Connection failed...");
        goto exit;
    }
    ESP_LOGI(TAG, "Connection to %s established...", url);

    size_t written_bytes = 0;
    size_t request_len = strlen(request);
    do
    {
        int ret = esp_tls_conn_write(tls, request + written_bytes, request_len - written_bytes);
        if (ret >= 0)
        {
            ESP_LOGI(TAG, "%d bytes request written", ret);
            written_bytes += ret;
        }
        else if (ret != ESP_TLS_ERR_SSL_WANT_READ && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "esp_tls_conn_write returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto exit;
        }
    } while (written_bytes < request_len);

    ESP_LOGI(TAG, "Reading HTTP response...");
    bool connected = true;

    while (connected)
    {
        int len = esp_tls_conn_read(tls, receive_buffer, HTTP_RECEIVE_SIZE);

        if (len == ESP_TLS_ERR_SSL_WANT_WRITE || len == ESP_TLS_ERR_SSL_WANT_READ)
        {
            ESP_LOGI(TAG, "continue");
            continue;
        }

        if (len < 0)
        {
            ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)", -len, esp_err_to_name(len));
            if (req->connection_closed_cbr)
            {
                req->connection_closed_cbr(req->connection_closed_ctx);
            }
            connected = false;
            break;
        }

        if (len == 0)
        {
            ESP_LOGI(TAG, "connection closed");
            if (req->connection_closed_cbr)
            {
                req->connection_closed_cbr(req->connection_closed_ctx);
            }
            connected = false;
            break;
        }

        if (req->data_received_cbr)
        {
            connected = (req->data_received_cbr(req->data_received_ctx, receive_buffer, len) == ESP_OK);
        }
        else
        {
            ESP_LOGI(TAG, "%d bytes received", len);
        }
        ret = ESP_OK;
    }

exit:
    esp_tls_conn_delete(tls);

    free(auth_line);
    free(request);
    free(url);
    free(receive_buffer);

    return ret;
}

esp_err_t cloud_set_time_cbr(void *ctx, uint8_t *data, size_t length)
{
    if (length <= 5 || length > 32)
    {
        return ESP_OK;
    }

    char str[33];
    memcpy(str, data, length);
    str[length] = 0;
    ESP_LOGI(TAG, "Received time string: %s", str);

    time_t unixTime = atol(str);
    if (unixTime == 0 && strcmp(str, "0") != 0)
    {
        return ESP_FAIL;
    }

    struct timeval tv = {
        .tv_sec = unixTime,
        .tv_usec = 0};

    if (settimeofday(&tv, NULL) == -1)
    {
        return ESP_FAIL;
    }

    time_t currentTime;
    struct tm *timeInfo;
    char buffer[80];

    time(&currentTime);
    timeInfo = localtime(&currentTime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    ESP_LOGI(TAG, "Current Date and Time: %s", buffer);

    return ESP_FAIL;
}

esp_err_t cloud_set_time(void)
{
    http_parser_t http_parser_handler_ctx = {
        .http_data_cbr = &cloud_set_time_cbr};
    cloud_req_t req = {
        .host = "192.168.1.26",
        .port = 443,
        .path = "/v1/time",
        .data_received_cbr = &http_parser_handler,
        .data_received_ctx = &http_parser_handler_ctx};

    return cloud_request(&req);
}

typedef struct
{
    uint32_t status_code;
    uint32_t content_length;
    uint32_t received;
} cloud_content_ctx_t;

esp_err_t cloud_content_status_cbr(void *ctx_in, int status_code)
{
    cloud_content_ctx_t *ctx = (cloud_content_ctx_t *)ctx_in;
    ctx->status_code = status_code;
    ESP_LOGI(TAG, "cloud_content_status_cbr: '%d'", status_code);
    return ESP_OK;
}

esp_err_t cloud_content_length_cbr(void *ctx_in, size_t content_length)
{
    cloud_content_ctx_t *ctx = (cloud_content_ctx_t *)ctx_in;
    ctx->content_length = content_length;
    ESP_LOGI(TAG, "%d bytes total", ctx->content_length);
    return ESP_OK;
}

esp_err_t cloud_content_cbr(void *ctx_in, uint8_t *data, size_t length)
{
    cloud_content_ctx_t *ctx = (cloud_content_ctx_t *)ctx_in;
    ctx->received += length;

    time_t current = time(NULL);
    static time_t start = 0;
    static time_t last = 0;

    if (start == 0)
    {
        start = current;
    }

    if (last != current)
    {
        last = current;

        float elapsed = difftime(current, start);
        float speed = (ctx->received / elapsed) / 1024.0; // KiB/s
        float percent_complete = (ctx->received * 100.0f / ctx->content_length);
        float eta = ((ctx->content_length - ctx->received) / (ctx->received / elapsed)); // ETA in seconds

        ESP_LOGI(TAG, "%d bytes received (%2.2f%%), Speed: %2.2f KiB/s, ETA: %2.2f s",
                 ctx->received, percent_complete, speed, eta);
    }
    return ESP_OK;
}

esp_err_t cloud_content_end_cbr(void *ctx_in)
{
    cloud_content_ctx_t *ctx = (cloud_content_ctx_t *)ctx_in;
    ESP_LOGI(TAG, "cloud_content_end_cbr %d/%d", ctx->received, ctx->content_length);
    return ESP_OK;
}

esp_err_t cloud_download(uint64_t uid)
{
    cloud_content_ctx_t cc_ctx = {0};
    char filename[64];
    char *filename_ptr = filename;

    filename_ptr += sprintf(filename_ptr, "/v2/content/");

    for (int i = 0; i < 8; ++i)
    {
        uint8_t byte = (uid >> (i * 8)) & 0xFF;
        filename_ptr += sprintf(filename_ptr, "%02X", byte);
    }

    http_parser_t http_parser_handler_ctx = {
        .http_status_cbr = &cloud_content_status_cbr,
        .content_length_cbr = &cloud_content_length_cbr,
        .http_data_cbr = &cloud_content_cbr,
        .http_end_cbr = &cloud_content_end_cbr,
        .ctx = &cc_ctx};

    cloud_req_t req = {
        .host = "192.168.1.26",
        .port = 443,
        .path = filename_ptr,
        .auth = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        .data_received_cbr = &http_parser_handler,
        .data_received_ctx = &http_parser_handler_ctx};

    return cloud_request(&req);
}

static void cloud_task(void *ctx)
{
    bool cloud_available = false;
    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (!cloud_available)
        {
            ESP_LOGI(TAG, "Try to get time");
            if (cloud_set_time() == ESP_OK)
            {
                cloud_available = true;
            }
            else
            {
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        }

        if (cloud_available)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Start request");

            cloud_download(0xE0040350103E913B);

            ESP_LOGI(TAG, "Finish request");
        }
    }
    vTaskDelete(NULL);
}

void cloud_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app_info);

    ESP_LOGI(TAG, "Loading certificates");
    cloud_load_cert("/spiflash/cert/ca.der", &ca_der, &ca_der_len);
    cloud_load_cert("/spiflash/cert/client.der", &client_der, &client_der_len);
    cloud_load_cert("/spiflash/cert/private.der", &private_der, &private_der_len);

    xTaskCreate(&cloud_task, "cloud_task", 8000, NULL, 5, NULL);
}
