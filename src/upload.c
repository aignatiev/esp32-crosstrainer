#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#define MBEDTLS_CONFIG_FILE "mbedtls/esp_config.h"  // Hack for new esp-idf & tls
#include "esp_tls.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"

#include "upload.h"
#include "defines.h"

#ifdef DO_UPLOAD

static const char *TAG = "upload";

static char buf[512];

void https_get_request(const char *WEB_SERVER_URL, const char *REQUEST) {
   int ret, len;

   esp_tls_cfg_t cfg = {.crt_bundle_attach = esp_crt_bundle_attach};
   esp_tls_t *tls = esp_tls_conn_http_new(WEB_SERVER_URL, &cfg);

   if (tls != NULL) {
      ESP_LOGI(TAG, "Connection established...");
   } else {
      ESP_LOGE(TAG, "Connection failed...");
      goto cleanup;
   }

   size_t written_bytes = 0;
   do {
      ret = esp_tls_conn_write(tls, REQUEST + written_bytes, strlen(REQUEST) - written_bytes);
      if (ret >= 0) {
         ESP_LOGI(TAG, "%d bytes written", ret);
         written_bytes += ret;
      } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
         ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
         goto cleanup;
      }
   } while (written_bytes < strlen(REQUEST));

   ESP_LOGI(TAG, "Reading HTTP response...");
   do {
      len = sizeof(buf);
      ret = esp_tls_conn_read(tls, (char *)buf, len);

      if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
         continue;
      } else if (ret < 0) {
         ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
         break;
      } else if (ret == 0) {
         ESP_LOGI(TAG, "connection closed");
         break;
      }

      len = ret;
      ESP_LOGD(TAG, "%d bytes read", len);
#ifdef PRINT_ALL
      for (int i = 0; i < len; i++)
         putchar(buf[i]);
#endif
   } while (1);

cleanup:
   esp_tls_conn_destroy(tls);
}
#endif