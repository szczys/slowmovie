#include "credentials.h"
#include <esp_log.h>
#include <golioth/client.h>
#include <golioth/ota.h>
#include <golioth/settings.h>
#include <string.h>
#include "compression.h"
#include "golioth.h"
#include "fw_update.h"
#include "version.h"

#define TAG "golioth"

struct frame_context
{
    SemaphoreHandle_t uri_setting_lock;
    SemaphoreHandle_t new_uri_setting_received;
    char uri[GOLIOTH_OTA_MAX_COMPONENT_URI_LEN + 1];

    SemaphoreHandle_t component_lock;
    SemaphoreHandle_t dl_finished;
    bool display_update_needed;
    struct golioth_client *client;
    struct golioth_ota_component component;
    uint8_t *framebuffer;
    size_t framebuffer_data_len;
    uint8_t *zzbuffer;
    size_t zzbuffer_data_len;
    golioth_frame_cb_t cb;
} golioth_frame_ctx;

#define FRAMEBUF_SIZE 50000
#define ZZBUF_SIZE (FRAMEBUF_SIZE / 2)
#define FRAME_MONITOR_STACK_SIZE 4096

extern const uint8_t ca_pem_start[] asm("_binary_isrgrootx1_goliothrootx1_pem_start");
extern const uint8_t ca_pem_end[] asm("_binary_isrgrootx1_goliothrootx1_pem_end");

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    ESP_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

static enum golioth_settings_status on_new_frame_setting(const char *new_value,
                                                         size_t new_value_len,
                                                         void *arg)
{
    if (NULL == arg)
    {
        ESP_LOGE(TAG, "Golioth frame context is NULL");
        return GOLIOTH_SETTINGS_GENERAL_ERROR;
    }

    struct frame_context *ctx = (struct frame_context *) arg;

    if (pdTRUE != xSemaphoreTake(ctx->uri_setting_lock, pdMS_TO_TICKS(1)))
    {
        ESP_LOGE(TAG, "Failed to acquire lock");
        return GOLIOTH_SETTINGS_GENERAL_ERROR;
    }

    enum golioth_settings_status err = GOLIOTH_SETTINGS_SUCCESS;

    if (new_value_len >= sizeof(ctx->uri))
    {
        ESP_LOGE(TAG, "New string too long: %d > %d", new_value_len, sizeof(ctx->uri) - 1);
        err = GOLIOTH_SETTINGS_VALUE_STRING_TOO_LONG;
        goto finish;
    }

    if (0 != strncmp(new_value, ctx->uri, new_value_len))
    {
        strncpy(ctx->uri, new_value, new_value_len);
        ctx->uri[new_value_len] = '\0';
        ESP_LOGI(TAG, "New frame string: %s", ctx->uri);
        xSemaphoreGive(ctx->new_uri_setting_received);
    }
    else
    {
        ESP_LOGI(TAG, "Already received frame: %s", ctx->uri);
    }

finish:
    xSemaphoreGive(ctx->uri_setting_lock);
    return err;
}

static enum golioth_status frame_block_cb(const struct golioth_ota_component *component,
                                          uint32_t block_idx,
                                          const uint8_t *block_buffer,
                                          size_t block_buffer_len,
                                          bool is_last,
                                          size_t negotiated_block_size,
                                          void *arg)
{
    if (NULL == arg)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct frame_context *ctx = (struct frame_context *) arg;
    uint32_t offset = block_idx * negotiated_block_size;
    if (ZZBUF_SIZE < offset + block_buffer_len)
    {
        ESP_LOGE(TAG, "Buffer overflow: %d > %d", offset + block_buffer_len, ZZBUF_SIZE);
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    memcpy(ctx->zzbuffer + offset, block_buffer, block_buffer_len);
    ctx->zzbuffer_data_len = offset + block_buffer_len;
    ESP_LOGI(TAG, "Received: %d bytes", ctx->zzbuffer_data_len);

    return GOLIOTH_OK;
}


static void frame_end_cb(enum golioth_status status,
                         const struct golioth_coap_rsp_code *rsp_code,
                         const struct golioth_ota_component *component,
                         uint32_t block_idx,
                         void *arg)
{
    if (NULL == arg)
    {
        ESP_LOGE(TAG, "Context cannot be NULL");
        return;
    }

    struct frame_context *ctx = (struct frame_context *) arg;

    if (GOLIOTH_OK != status)
    {
        ESP_LOGE(TAG, "Download failed: %d", status);
        goto finish;
    }

    ESP_LOGI(TAG, "Download complete!");
    ctx->display_update_needed = true;

finish:
    xSemaphoreGive(ctx->dl_finished);
    return;
}

void frame_monitor_task(void *pvParameters)
{
    if (NULL == pvParameters)
    {
        ESP_LOGE(TAG, "Frame monitor task failed to recieve context.");
        return;
    }

    struct frame_context *ctx = (struct frame_context *) pvParameters;

    while (true)
    {
        xSemaphoreTake(ctx->new_uri_setting_received, portMAX_DELAY);

        xSemaphoreTake(ctx->component_lock, portMAX_DELAY);

        xSemaphoreTake(ctx->uri_setting_lock, portMAX_DELAY);
        memcpy(ctx->component.uri, ctx->uri, GOLIOTH_OTA_MAX_COMPONENT_URI_LEN + 1);
        xSemaphoreGive(ctx->uri_setting_lock);

        ctx->zzbuffer_data_len = 0;

        golioth_ota_download_component(ctx->client,
                                       &ctx->component,
                                       0,
                                       frame_block_cb,
                                       frame_end_cb,
                                       ctx);

        xSemaphoreTake(ctx->dl_finished, portMAX_DELAY);

        if (true == ctx->display_update_needed)
        {
            ctx->display_update_needed = false;

            ctx->framebuffer_data_len = FRAMEBUF_SIZE;
            int err = decompress(ctx->zzbuffer,
                                 ctx->zzbuffer_data_len,
                                 ctx->framebuffer,
                                 &ctx->framebuffer_data_len);

            if (0 != err)
            {
                ESP_LOGE(TAG, "Failed to decompress image: %d", err);
            }
            else
            {
                ctx->cb(ctx->framebuffer, ctx->framebuffer_data_len);
            }
        }
        xSemaphoreGive(ctx->component_lock);
    }
}

void golioth_register_frames(struct slowmovie_creds *creds, golioth_frame_cb_t cb)
{
    if (NULL == cb)
    {
        ESP_LOGE(TAG, "Callback cannot be NULL");
        return;
    }

    golioth_frame_ctx.cb = cb;
    golioth_frame_ctx.new_uri_setting_received = xSemaphoreCreateBinary();
    golioth_frame_ctx.uri_setting_lock = xSemaphoreCreateMutex();
    golioth_frame_ctx.component_lock = xSemaphoreCreateMutex();
    golioth_frame_ctx.dl_finished = xSemaphoreCreateBinary();
    golioth_frame_ctx.display_update_needed = false;

    golioth_frame_ctx.zzbuffer = (uint8_t *) malloc(ZZBUF_SIZE);
    if (NULL == golioth_frame_ctx.zzbuffer)
    {
        ESP_LOGE(TAG, "Failed to allocate zzbuffer");
        return;
    }

    golioth_frame_ctx.framebuffer = (uint8_t *) malloc(FRAMEBUF_SIZE);
    if (NULL == golioth_frame_ctx.framebuffer)
    {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return;
    }

    struct golioth_client_config client_config = {
        .credentials = {.auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                        .pki = {
                            .ca_cert = ca_pem_start,
                            .ca_cert_len = ca_pem_end - ca_pem_start,
                            .public_cert = creds->crt_pem.buf,
                            .public_cert_len = creds->crt_pem.len,
                            .private_key = creds->key_pem.buf,
                            .private_key_len = creds->key_pem.len,
                        }}};

    golioth_frame_ctx.client = golioth_client_create(&client_config);
    golioth_client_register_event_callback(golioth_frame_ctx.client, on_client_event, NULL);

    struct golioth_settings *settings = golioth_settings_init(golioth_frame_ctx.client);

    golioth_settings_register_string(settings, "FRAME", on_new_frame_setting, &golioth_frame_ctx);

    golioth_fw_update_init(golioth_frame_ctx.client, _current_version);

    /* Start task to handle frame downloads */
    BaseType_t frame_task;
    TaskHandle_t xHandle = NULL;

    frame_task = xTaskCreate(frame_monitor_task,
                             "frame_monitor",
                             FRAME_MONITOR_STACK_SIZE,
                             &golioth_frame_ctx,
                             4,
                             &xHandle);

    if (frame_task != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create frame task");
    }
}
