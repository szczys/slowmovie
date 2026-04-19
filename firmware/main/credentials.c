#include "esp_log.h"
#include "credentials.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pem.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "credentials"

#define STORAGE_NAMESPACE "slowmovie_creds"
#define MAX_CRED_LEN 500

#define CRED_KEY_WIFI_SSID "wifi_ssid"
#define CRED_KEY_WIFI_PSK "wifi_psk"
#define CRED_KEY_CRT_DER "crt_der"
#define CRED_KEY_KEY_DER "key_der"

nvs_handle_t _handle;

void cred_free(struct credential *cred)
{
    free(cred->buf);
    cred->len = 0;
}

void cred_free_slowmovie(struct slowmovie_creds *creds)
{
    cred_free(&creds->wifi_ssid);
    cred_free(&creds->wifi_psk);
    cred_free(&creds->crt_pem);
    cred_free(&creds->key_pem);
}

static int cred_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

static int cred_handle_init(nvs_handle_t *handle)
{
    return nvs_open(STORAGE_NAMESPACE, NVS_READONLY, handle);
}

static int cred_string_load(nvs_handle_t handle, const char *key, struct credential *cred)
{
    cred->len = MAX_CRED_LEN;
    int err = nvs_get_str(handle, key, NULL, &cred->len);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to read %s len: %i", key, err);
        return err;
    }

    cred->buf = (uint8_t *) malloc(cred->len);
    if (NULL == cred->buf)
    {
        return ESP_ERR_NO_MEM;
    }

    err = nvs_get_str(handle, key, (char *) cred->buf, &cred->len);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load %s from NVS: %i", key, err);
        free(cred->buf);
        return err;
    }

    return 0;
}

static int cred_binary_load(nvs_handle_t handle, const char *key, struct credential *cred)
{
    cred->len = MAX_CRED_LEN;
    int err = nvs_get_blob(handle, key, NULL, &cred->len);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to read %s len: %i", key, err);
        return err;
    }

    cred->buf = (uint8_t *) malloc(cred->len);
    if (NULL == cred->buf)
    {
        return ESP_ERR_NO_MEM;
    }

    err = nvs_get_blob(handle, key, cred->buf, &cred->len);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load %s from NVS: %i", key, err);
        free(cred->buf);
        return err;
    }

    return 0;
}

static int cred_get_crt_as_pem(nvs_handle_t handle, struct credential *cred_pem)
{
    size_t max_pem_len;

    struct credential crt_der;
    int ret = cred_binary_load(_handle, CRED_KEY_CRT_DER, &crt_der);
    if (0 != ret)
    {
        goto free_der_and_return;
    }

    max_pem_len = crt_der.len * 2;
    cred_pem->buf = (uint8_t *) malloc(max_pem_len);
    if (NULL == cred_pem->buf)
    {
        ret = ESP_ERR_NO_MEM;
        goto free_der_and_return;
    }

    ret = mbedtls_pem_write_buffer("-----BEGIN CERTIFICATE-----\n",
                                   "-----END CERTIFICATE-----\n",
                                   crt_der.buf,
                                   crt_der.len,
                                   cred_pem->buf,
                                   max_pem_len,
                                   &cred_pem->len);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to convert CRT to PEM: -0x%04x", -ret);
        cred_free(cred_pem);
        goto free_der_and_return;
    }

free_der_and_return:
    cred_free(&crt_der);
    return ret;
}

static int cred_get_key_as_pem(nvs_handle_t handle, struct credential *cred_pem)
{
    size_t max_pem_len;
    mbedtls_pk_context pk;

    struct credential key_der;
    int ret = cred_binary_load(_handle, CRED_KEY_KEY_DER, &key_der);
    if (0 != ret)
    {
        goto free_der_and_return;
    }

    max_pem_len = key_der.len * 2;
    cred_pem->buf = (uint8_t *) malloc(max_pem_len);
    if (NULL == cred_pem->buf)
    {
        ret = ESP_ERR_NO_MEM;
        goto free_der_and_return;
    }

    mbedtls_pk_init(&pk);
    ret =
        mbedtls_pk_parse_key(&pk, key_der.buf, key_der.len, NULL, 0, mbedtls_ctr_drbg_random, NULL);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to parse DER key: -0x%04x", -ret);
        goto free_all_and_return;
    }

    ret = mbedtls_pk_write_key_pem(&pk, cred_pem->buf, max_pem_len);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to convert key DER to PEM: -0x%04x", -ret);
        goto free_all_and_return;
    }

    cred_pem->len = strlen((char *) cred_pem->buf) + 1;
    goto free_der_and_return;

free_all_and_return:
    cred_free(cred_pem);

free_der_and_return:
    mbedtls_pk_free(&pk);
    cred_free(&key_der);
    return ret;
}

int cred_load_all(struct slowmovie_creds *creds)
{
    int err;

    /* Initialization for structs instantiated at run time */
    creds->wifi_ssid.buf = NULL;
    creds->wifi_ssid.len = 0;
    creds->wifi_psk.buf = NULL;
    creds->wifi_psk.len = 0;
    creds->crt_pem.buf = NULL;
    creds->crt_pem.len = 0;
    creds->key_pem.buf = NULL;
    creds->key_pem.len = 0;

    err = cred_nvs_init();
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS");
        return err;
    }

    err = cred_handle_init(&_handle);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS handle");
        return err;
    }

    err = cred_string_load(_handle, CRED_KEY_WIFI_SSID, &creds->wifi_ssid);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load WiFi SSID");
        goto free_and_return;
    }

    err = cred_string_load(_handle, CRED_KEY_WIFI_PSK, &creds->wifi_psk);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load WiFi PSK");
        goto free_and_return;
    }

    err = cred_get_crt_as_pem(_handle, &creds->crt_pem);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load CRT");
        goto free_and_return;
    }

    err = cred_get_key_as_pem(_handle, &creds->key_pem);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load CRT");
        goto free_and_return;
    }

    return 0;

free_and_return:
    cred_free_slowmovie(creds);
    return err;
}
