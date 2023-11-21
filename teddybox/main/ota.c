
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "errno.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define BUFFSIZE 512
#define HASH_LEN 32
static char rx_buffer[BUFFSIZE];
static char addr_str[32];

static const char *TAG = "OTA";
static esp_app_desc_t new_app_info;
static esp_app_desc_t running_app_info;
static const esp_partition_t *configured = NULL;
static const esp_partition_t *running = NULL;
static const esp_partition_t *update_partition = NULL;

void ota_mainthread(void *arg)
{
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;

    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(63660);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            continue;
        }

        int len = 0;
        bool image_header_was_checked = false;
        do
        {
            len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
            if (len < 0)
            {
                ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
                shutdown(sock, 0);
                close(sock);
                continue;
            }
            else if (len == 0)
            {
                ESP_LOGW(TAG, "Connection closed");
            }
            else
            {
                err = esp_ota_write(update_handle, (const void *)rx_buffer, len);
                if (err != ESP_OK)
                {
                    esp_ota_abort(update_handle);
                    continue;
                }

                // check current version with downloading
                if (image_header_was_checked == false)
                {
                    memcpy(&new_app_info, &rx_buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW(TAG, "Current running version is the same as a new.");
                    }
                    image_header_was_checked = true;
                }
            }
        } while (len > 0);

        ESP_LOGW(TAG, "esp_ota_end");
        err = esp_ota_end(update_handle);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            else
            {
                ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
            }
            continue;
        }

        ESP_LOGW(TAG, "esp_ota_set_boot_partition");
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
            continue;
        }
        ESP_LOGW(TAG, "Prepare to restart system!");
        esp_restart();
    }
}

void ota_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Starting OTA");

    configured = esp_ota_get_boot_partition();
    running = esp_ota_get_running_partition();
    update_partition = esp_ota_get_next_update_partition(NULL);

    ESP_LOGI(TAG, "  Configured 0x%08lx '%s'", configured->address, configured->label);
    ESP_LOGI(TAG, "  Current    0x%08lx '%s'", running->address, running->label);
    ESP_LOGI(TAG, "  OTA        0x%08lx '%s'", update_partition->address, update_partition->label);

    if (esp_ota_get_partition_description(running, &running_app_info) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fetch firmware version");
        return;
    }

    ESP_LOGI(TAG, "  Current version: %s", running_app_info.version);

    xTaskCreatePinnedToCore(ota_mainthread, "[TB] OTA", 4096, NULL, 5, NULL, 0);
}
