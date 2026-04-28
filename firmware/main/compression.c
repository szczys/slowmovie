#include <esp_log.h>
#include <esp_err.h>
#include "miniz.h"
#include <string.h>

#define TAG "compression"

int decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t *dst_len)
{
    int err;

    ESP_LOGW(TAG, "sz: %zu sz: %zu", src_len, *dst_len);
    tinfl_decompressor *decomp = (tinfl_decompressor *) malloc(sizeof(tinfl_decompressor));
    if (!decomp)
    {
        ESP_LOGE(TAG, "Failed to allocate decompressor state");
        err = -ESP_ERR_NO_MEM;
        goto free_and_return;
    }

    tinfl_init(decomp);

    size_t in_bytes = src_len;
    size_t out_bytes = *dst_len;

    tinfl_status status =
        tinfl_decompress(decomp,
                         src,
                         &in_bytes,
                         dst,
                         dst,
                         &out_bytes,
                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    if (status == TINFL_STATUS_DONE)
    {
        ESP_LOGI(TAG, "Success! Decompressed %zu bytes.", out_bytes);
        *dst_len = out_bytes;
        err = ESP_OK;
        goto free_and_return;
    }
    else
    {
        ESP_LOGE(TAG, "Decompression failed. Status: %d", (int) status);
        err = ESP_FAIL;
        goto free_and_return;
    }

free_and_return:
    free(decomp);
    return err;
}
