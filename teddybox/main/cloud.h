#pragma once


#define MAX_HTTP_HEADER_SIZE 1024
#define HTTP_RECEIVE_SIZE 512

#define CLOUD_SUCCESS_DISCONNECT -2

typedef struct {
    esp_err_t (*http_status_cbr)(void *ctx, int status_code);
    esp_err_t (*http_data_cbr)(void *ctx, uint8_t *data, size_t length);
    esp_err_t (*http_end_cbr)(void *ctx);
    esp_err_t (*content_length_cbr)(void *ctx, size_t content_length);
    void *ctx;
    size_t content_length;
    size_t received_data_length;
    uint8_t header_buffer[MAX_HTTP_HEADER_SIZE];
    size_t header_size;
    int header_parsed;
} http_parser_t;


typedef struct {
    const char *host;
    uint16_t port;
    const char *path;
    const char *auth;
    esp_err_t (*data_received_cbr)(void *ctx, uint8_t *data, size_t length);
    void *data_received_ctx;
    esp_err_t (*connection_closed_cbr)(void *ctx);
    void *connection_closed_ctx;
} cloud_req_t;

void cloud_init(void);
esp_err_t cloud_set_time(void);

