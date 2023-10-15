
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
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"

#define CLOUD_HOST "tc.fritz.box"
#define CLOUD_URI "/v2/content/00112233030405E0"

static const char *TAG = "Cloud";

uint8_t *ca_der = NULL;
uint8_t *client_der = NULL;
uint8_t *private_der = NULL;
size_t ca_der_len = 0;
size_t client_der_len = 0;
size_t private_der_len = 0;
static esp_app_desc_t running_app_info;

static void https_get_request(esp_tls_cfg_t cfg, const char *url, const char *req)
{
    char buf[512];
    int ret, len;

    struct esp_tls *tls = esp_tls_conn_http_new(url, &cfg);

    if (!tls)
    {
        ESP_LOGE(TAG, "Connection failed...");
        goto exit;
    }
    ESP_LOGI(TAG, "Connection to %s established, request:\r\n%s...", url, req);

    size_t written_bytes = 0;
    do
    {
        ret = esp_tls_conn_write(tls, req + written_bytes, strlen(req) - written_bytes);
        if (ret >= 0)
        {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        }
        else if (ret != ESP_TLS_ERR_SSL_WANT_READ && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto exit;
        }
    } while (written_bytes < strlen(req));

    ESP_LOGI(TAG, "Reading HTTP response...");

    do
    {
        len = sizeof(buf) - 1;
        bzero(buf, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_WANT_READ)
        {
            ESP_LOGI(TAG, "continue");
            continue;
        }

        if (ret < 0)
        {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        }

        if (ret == 0)
        {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGI(TAG, "%d bytes read", len);
        ESP_LOGI(TAG, "'%s'", buf);
    } while (1);

exit:
    esp_tls_conn_delete(tls);
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

static void cloud_request(const char *host, uint16_t port, const char *uri)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = ca_der,
        .cacert_bytes = ca_der_len,
        .clientcert_buf = client_der,
        .clientcert_bytes = client_der_len,
        .clientkey_buf = private_der,
        .clientkey_bytes = private_der_len,
        .skip_common_name = true};

    char *request;
    char *url;

    asprintf(&request, "GET %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "User-Agent: teddybox/1.0 %s\r\n"
                       "\r\n",
             uri, host, running_app_info.version);

    asprintf(&url, "https://%s%s", host, uri);
    https_get_request(cfg, url, request);
}

static void https_request_task(void *pvparameters)
{
    if (0)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Start https_request example");

        cloud_load_cert("/spiflash/CERT/CA.DER", &ca_der, &ca_der_len);
        cloud_load_cert("/spiflash/CERT/CLIENT.DER", &client_der, &client_der_len);
        cloud_load_cert("/spiflash/CERT/PRIVATE.DER", &private_der, &private_der_len);

        cloud_request(CLOUD_HOST, 443, CLOUD_URI);

        ESP_LOGI(TAG, "Finish https_request example");
    }
    vTaskDelete(NULL);
}

void cloud_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app_info);

    xTaskCreate(&https_request_task, "https_get_task", 8192, NULL, 5, NULL);
}
