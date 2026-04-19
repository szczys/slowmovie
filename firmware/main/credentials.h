#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stddef.h>
#include <stdint.h>

struct credential {
  uint8_t *buf;
  size_t len;
};

struct slowmovie_creds {
  struct credential wifi_ssid;
  struct credential wifi_psk;
  struct credential crt_pem;
  struct credential key_pem;
};

int cred_load_all(struct slowmovie_creds *creds);

#ifdef __cplusplus
}
#endif
