

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>
#include "wifi.h"

#define MAX_RETRY_ATTEMPTS 2

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

static const char *TAG = "WiFi";
static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
static wifi_config_t wps_ap_creds[MAX_WPS_AP_CRED];
static int s_ap_creds_num = 0;
static int s_ap_creds_idx = 0;
static int s_retry_num = 0;
static bool nvs_updated = false;

static int r_ap_idx(int ap_idx) 
{
    return s_ap_creds_num-ap_idx-1;
}
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");

        if (s_ap_creds_idx < s_ap_creds_num)
        {
            ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                     wps_ap_creds[r_ap_idx(s_ap_creds_idx)].sta.ssid, wps_ap_creds[r_ap_idx(s_ap_creds_idx)].sta.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[r_ap_idx(s_ap_creds_idx)]));
            s_ap_creds_idx++;
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGI(TAG, "No credentials found. Entering WPS...");

            ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        }
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
        wifi_save_nvs();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        if (s_retry_num < MAX_RETRY_ATTEMPTS)
        {
            esp_wifi_connect();
            s_retry_num++;
        }
        else if (s_ap_creds_idx < s_ap_creds_num)
        {
            /* Try the next AP credential if first one fails */

            if (s_ap_creds_idx < s_ap_creds_num)
            {
                ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                         wps_ap_creds[r_ap_idx(s_ap_creds_idx)].sta.ssid, wps_ap_creds[r_ap_idx(s_ap_creds_idx)].sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[r_ap_idx(s_ap_creds_idx)]));
                s_ap_creds_idx++;
                esp_wifi_connect();
            }
            s_retry_num = 0;
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect!");
        }

        break;
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
        {
            wifi_event_sta_wps_er_success_t *evt =
                (wifi_event_sta_wps_er_success_t *)event_data;
            int i;

            if (evt)
            {
                s_ap_creds_num = evt->ap_cred_cnt;
                for (i = 0; i < s_ap_creds_num; i++)
                {
                    memcpy(wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                           sizeof(evt->ap_cred[i].ssid));
                    memcpy(wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                           sizeof(evt->ap_cred[i].passphrase));
                }
                /* NVS data needs to be updated if connection is successful */
                nvs_updated = true;

                /* If multiple AP credentials are received from WPS, connect with first one */
                ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                         wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[0]));

                /* need tp update NVS */
            }
            /*
             * If only one AP credential is received from WPS, there will be no event data and
             * esp_wifi_set_config() is already called by WPS modules for backward compatibility
             * with legacy apps. So directly attempt connection here.
             */
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            esp_wifi_connect();
        }
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_PIN");
        /* display the PIN code */
        wifi_event_sta_wps_er_pin_t *event = (wifi_event_sta_wps_er_pin_t *)event_data;
        ESP_LOGI(TAG, "WPS_PIN = " PINSTR, PIN2STR(event->pin_code));
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
}

void wifi_load_nvs(void)
{
    s_ap_creds_num = 0;
    s_ap_creds_idx = 0;
    
    do
    {
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("TB_WIFI", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
            break;
        }
        uint8_t index = 0;
        err = nvs_get_u8(nvs_handle, "INDEX", &index);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read WiFi INDEX (%s)", esp_err_to_name(err));
            nvs_close(nvs_handle);
            break;
        }

        if (index > MAX_WPS_AP_CRED) {
            ESP_LOGE(TAG, "WiFi INDEX too high %i (%s)", index, esp_err_to_name(err));
            index = MAX_WPS_AP_CRED;
        }
        for (size_t i=0; i<index; i++)
        {
            char *ssid = "SSID+";
            char *pw = "PW+";
            ssid[4] = 0x30 + i;
            pw[2] = 0x30 + i;

            size_t len = sizeof(wps_ap_creds[i].sta.ssid) - 1;
            err = nvs_get_blob(nvs_handle, ssid, wps_ap_creds[i].sta.ssid, &len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to read SSID (%s)", esp_err_to_name(err));
                break;
            }
            len = sizeof(wps_ap_creds[i].sta.password) - 1;
            err = nvs_get_blob(nvs_handle, pw, wps_ap_creds[i].sta.password, &len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to read PW (%s)", esp_err_to_name(err));
                break;
            }
            ESP_LOGI(TAG, "SSID: '%s' PW:'%s'", wps_ap_creds[i].sta.ssid, wps_ap_creds[i].sta.password);
            
            s_ap_creds_num++;
        }
        nvs_updated = false;

        nvs_close(nvs_handle);
    } while (0);
}

void wifi_save_nvs(void)
{
    if (s_ap_creds_num != 1 || !nvs_updated)
    {
        return;
    }
    do
    {
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("TB_WIFI", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
            break;
        }

        err = nvs_set_blob(nvs_handle, "SSID0", wps_ap_creds[0].sta.ssid, strlen((char*)wps_ap_creds[0].sta.ssid));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write SSID (%s)", esp_err_to_name(err));
            nvs_close(nvs_handle);
            break;
        }
        err = nvs_set_blob(nvs_handle, "PW0", wps_ap_creds[0].sta.password, strlen((char*)wps_ap_creds[0].sta.password));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write PW (%s)", esp_err_to_name(err));
            nvs_close(nvs_handle);
            break;
        }
        nvs_updated = false;

        nvs_close(nvs_handle);
    } while (0);
}

void wifi_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    wifi_load_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}
