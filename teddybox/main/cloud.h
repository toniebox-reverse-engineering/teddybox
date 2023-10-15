#pragma once


#define MAX_HTTP_HEADER_SIZE 1024 // Adjust this size as needed

typedef struct {
    esp_err_t (*original_cbr)(void *ctx, uint8_t *data, size_t length);
    void *original_ctx;
    uint8_t http_header_buffer[MAX_HTTP_HEADER_SIZE];
    size_t http_header_size;
    size_t content_length;
    int header_parsed;
} http_wrapper_ctx_t;


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

