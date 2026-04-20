#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct credential
{
    uint8_t *buf;
    size_t len;
    bool loaded;
};

struct slowmovie_creds
{
    struct credential wifi_ssid;
    struct credential wifi_psk;
    struct credential crt_pem;
    struct credential key_pem;
};

typedef int (*cred_set_fn_t)(const struct credential *cred);

int cred_load_all(struct slowmovie_creds *creds);
int cred_set_wifi_ssid(const struct credential *cred);
int cred_set_wifi_psk(const struct credential *cred);
int cred_set_device_crt(const struct credential *b64_der);
int cred_set_device_key(const struct credential *b64_der);

#ifdef __cplusplus
}
#endif
