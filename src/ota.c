#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "esp_log.h"

#include "ota.h"
#include "defines.h"

static const char *TAG = "ota";

// openssl s_client -showcerts -connect drive.google.com:443 < /dev/null
// Copy the last cert one to server_cert.pem
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

extern bool ota_done;  // TODO

void ota_task(void *pvParameter) {
   esp_http_client_config_t config = {
      .url = OTA_URL,
      .cert_pem = (char *)server_cert_pem_start
   };
   esp_err_t ret = esp_https_ota(&config);
   if (ret == ESP_OK) {
      esp_restart();
   } else {
      printf("esp_https_ota failed with error (%d)\n", ret);
   }
   ota_done = true;
   vTaskDelete(NULL);
}

void advanced_ota_task(void *pvParameter) {
   esp_http_client_config_t config = {
      .url = OTA_URL,
      .cert_pem = (char *)server_cert_pem_start,
      .timeout_ms = 7000,
      .keep_alive_enable = true,
   };

   esp_https_ota_config_t ota_config = {.http_config = &config};

   esp_https_ota_handle_t https_ota_handle = NULL;
   esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
   if (err != ESP_OK) {
      ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed (%d)", err);
      goto exit;
   }

   esp_app_desc_t desc;
   err = esp_https_ota_get_img_desc(https_ota_handle, &desc);
   if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed (%d)", err);
   } else {
      printf("magic_word = %x, version = %s, project_name = %s\ntime = %s, date %s, idf_ver = %s\n", 
            desc.magic_word, desc.version, desc.project_name, desc.time, desc.date, desc.idf_ver);
   }
   esp_https_ota_abort(https_ota_handle);

exit:
   ota_done = true;
   vTaskDelete(NULL);
}
