/* ULP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// pio run -t menuconfig

#include <stdio.h>
#include <string.h>
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

// This one defines whether there is any network stuff included (for debugging ULP)
#define DO_UPLOAD

// This one defines whether all debug prints are printed (for debugging)
//#define PRINT_ALL

// Magic number to check whether the RTC memory is intact
#define MAGIC_NUMBER 0xdeadbeef

#ifdef DO_UPLOAD
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define MBEDTLS_CONFIG_FILE "mbedtls/esp_config.h"  // Hack for new esp-idf & tls
#include "esp_tls.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"

#include "wifi.h"

#define API_TOKEN "xyz"
#define WEB_SERVER "script.google.com"
#define WEB_URL "https://script.google.com/macros/s/" API_TOKEN "/exec?%s"

static const char *TAG = "main";

static char url[256];
static char request[512];
static char buf[512];

static const char REQUEST_FMT[] = "GET %s HTTP/1.1\r\n"
                              "Host: "WEB_SERVER"\r\n"
                              "User-Agent: esp-idf/1.0 esp32\r\n"
                              "Connection: close\r\n"
                              "\r\n";

#endif

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

RTC_DATA_ATTR uint32_t rtc_magic_number;  // To see whether RCT mem is valid
RTC_DATA_ATTR uint8_t rtc_id;             // Measurement ID

/* This function is called once after power-on reset, to load ULP program into
 * RTC memory and configure the ADC.
 */
static void init_ulp_program(void);

/* This function is called every time before going into deep sleep.
 * It starts the ULP program and resets measurement counter.
 */
static void start_ulp_program(void);

#ifdef DO_UPLOAD
static void https_get_request(const char *WEB_SERVER_URL, const char *REQUEST) {
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
      ret = esp_tls_conn_write(tls,
                              REQUEST + written_bytes,
                              strlen(REQUEST) - written_bytes);
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
      len = sizeof(buf) - 1;
      memset(buf, 0x00, sizeof(buf));
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
      /* Print response directly to stdout as it is read */
      for (int i = 0; i < len; i++)
         putchar(buf[i]);
#endif
   } while (1);

cleanup:
   esp_tls_conn_destroy(tls);
}
#endif

void app_main(void) {
   if (rtc_magic_number != MAGIC_NUMBER) {
      rtc_magic_number = MAGIC_NUMBER;
      rtc_id = 0;
   }

   esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
   if (cause != ESP_SLEEP_WAKEUP_ULP) {
      printf("Not ULP wakeup\n");
      init_ulp_program();
   } else {
      printf("Deep sleep wakeup\n");
      ulp_last_result &= UINT16_MAX;
      ulp_bullshit &= UINT16_MAX;
      ulp_duration &= UINT16_MAX;
      ulp_steps &= UINT16_MAX;
      ulp_load &= UINT16_MAX;
      printf("id = %d\n", rtc_id);
      printf("last_value = %d\n", ulp_last_result);
      printf("bullshit = %d\n", ulp_bullshit);
      printf("duration = %d\n", ulp_duration);
      printf("steps = %d\n", ulp_steps);
      printf("load = %d\n", ulp_load);

#ifdef DO_UPLOAD
      ESP_ERROR_CHECK(nvs_flash_init());
      ESP_ERROR_CHECK(esp_netif_init());
      ESP_ERROR_CHECK(esp_event_loop_create_default());
      ESP_ERROR_CHECK(wifi_connect());

      char params[128];
      sprintf(params, "id=%d&dur=%d&steps=%d&diff=%d&vbat=%d", 
            rtc_id, ulp_duration/60, ulp_steps, ulp_load, 0);
      sprintf(url, WEB_URL, params);
      sprintf(request, REQUEST_FMT, url);

#ifdef PRINT_ALL
      printf("params=%s\n", params);
      printf("url=%s\n", url);
      printf("request=%s\n", request);
#endif

      https_get_request(url, request);
#endif
      rtc_id++;
   }
   printf("Entering deep sleep\n\n");
   start_ulp_program();
   ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
   esp_deep_sleep_start();
}

static void init_ulp_program(void) {
   esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
         (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
   ESP_ERROR_CHECK(err);

   /* Configure ADC channel */
   /* Note: when changing channel here, also change 'adc_channel' constant
      in adc.S */
   adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
#if CONFIG_IDF_TARGET_ESP32
   adc1_config_width(ADC_WIDTH_BIT_12);
#elif CONFIG_IDF_TARGET_ESP32S2
   adc1_config_width(ADC_WIDTH_BIT_13);
#endif
   adc1_ulp_enable();

   /* GPIO used for pulse counting. */
   gpio_num_t gpio_num = GPIO_NUM_0;
   int rtcio_num = rtc_io_number_get(gpio_num);
   assert(rtc_gpio_is_valid_gpio(gpio_num) && "GPIO used for pulse counting must be an RTC IO");
   printf("Using RTC pin %d for GPIO pin %d\n", rtcio_num, gpio_num);

   ulp_io_number = rtcio_num;
   ulp_timeout_max = 100*10;     // 10s delay assuming 10ms interval
   ulp_int_to_second_max = 100;  // Assuming 10ms interval

   /* Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown */
   rtc_gpio_init(gpio_num);
   rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
   rtc_gpio_pulldown_dis(gpio_num);
   rtc_gpio_pullup_dis(gpio_num);
   rtc_gpio_hold_en(gpio_num);

   /* Set ULP wake up period to 10ms */
   ulp_set_wakeup_period(0, 10000);  // Delay for inactive mode

   /* Disconnect GPIO12 and GPIO15 to remove current drain through
    * pullup/pulldown resistors.
    * GPIO12 may be pulled high to select flash voltage.
    */
   rtc_gpio_isolate(GPIO_NUM_12);
   rtc_gpio_isolate(GPIO_NUM_15);
#if CONFIG_IDF_TARGET_ESP32
   esp_deep_sleep_disable_rom_logging(); // suppress boot messages
#endif
}

static void start_ulp_program(void) {
   /* Start the program */
   esp_err_t err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
   ESP_ERROR_CHECK(err);
}