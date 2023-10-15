
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

esp_err_t http_body_decode(void *ctx, uint8_t *data, size_t length)
{
    http_wrapper_ctx_t *wrapper_ctx = (http_wrapper_ctx_t *)ctx;

    if (!wrapper_ctx->header_parsed)
    {
        if (wrapper_ctx->http_header_size + length > MAX_HTTP_HEADER_SIZE)
        {
            return ESP_FAIL;
        }
        memcpy(wrapper_ctx->http_header_buffer + wrapper_ctx->http_header_size, data, length);
        wrapper_ctx->http_header_size += length;

        char *header_end = strstr((char *)wrapper_ctx->http_header_buffer, "\r\n\r\n");
        if (header_end)
        {
            char *content_length_str = strstr((char *)wrapper_ctx->http_header_buffer, "Content-Length: ");
            if (content_length_str)
            {
                sscanf(content_length_str, "Content-Length: %zu", &wrapper_ctx->content_length);
            }

            wrapper_ctx->header_parsed = 1;

            uint8_t *content_start = (uint8_t *)(header_end + 4);
            size_t content_size = wrapper_ctx->http_header_size - (content_start - wrapper_ctx->http_header_buffer);

            if (wrapper_ctx->original_cbr)
            {
                return wrapper_ctx->original_cbr(wrapper_ctx->original_ctx, content_start, content_size);
            }
        }
    }
    else
    {
        if (wrapper_ctx->original_cbr)
        {
            return wrapper_ctx->original_cbr(wrapper_ctx->original_ctx, data, length);
        }
    }

    return ESP_OK;
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
        uint8_t buf[513] = {0};

        int len = esp_tls_conn_read(tls, buf, sizeof(buf) - 1);

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

        ESP_LOGI(TAG, "%d bytes received", len);
        if (req->data_received_cbr)
        {
            connected = (req->data_received_cbr(req->data_received_ctx, buf, len) == ESP_OK);
        }
        else
        {
            ESP_LOGI(TAG, "'%s'", buf);
        }
        ret = ESP_OK;
    }

exit:
    esp_tls_conn_delete(tls);

    free(auth_line);
    free(request);
    free(url);

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
    static http_wrapper_ctx_t http_body_decode_ctx = {
        .original_cbr = &cloud_set_time_cbr};
    static cloud_req_t req = {
        .host = "192.168.1.26",
        .port = 443,
        .path = "/v1/time",
        .auth = NULL,
        .data_received_cbr = &http_body_decode,
        .data_received_ctx = &http_body_decode_ctx};

    return cloud_request(&req);
}

static void cloud_task(void *ctx)
{
    bool time_set = false;
    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (!time_set)
        {
            ESP_LOGI(TAG, "Try to get time");
            if (cloud_set_time() == ESP_OK)
            {
                time_set = true;
            }
            else
            {
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        }

        if (0)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Start request");
            cloud_req_t req = {
                .host = "192.168.1.26",
                .port = 443,
                .path = "/v2/content/00112233030405E0",
                .auth = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                .data_received_cbr = NULL};

            cloud_request(&req);

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

    xTaskCreate(&cloud_task, "cloud_task", 8192, NULL, 5, NULL);
}
