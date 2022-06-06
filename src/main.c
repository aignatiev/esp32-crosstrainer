
// pio run -t menuconfig

#include <stdio.h>
#include <string.h>
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

// This one defines whether there is any network stuff included (for debugging ULP)
#define DO_UPLOAD

// This one defines whether all debug prints are printed (for debugging)
#define PRINT_ALL

// Magic number to check whether the RTC memory is intact
#define MAGIC_NUMBER    0xdeadbeef

// Which pin REED switch is connected to
#define REED_PIN        GPIO_NUM_0

// ULP wakeup period in ms
#define ULP_WAKEUP_MS   10

// Excercise timeout in seconds
#define TIMEOUT_S       10

// Stuff related to the Google Script
#define API_TOKEN "xyz"
#define WEB_SERVER "script.google.com"
#define WEB_URL "https://script.google.com/macros/s/" API_TOKEN "/exec?%s"

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

static void init_ulp_program(void) {
   esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
         (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
   ESP_ERROR_CHECK(err);

   // Configure ADC (same ADC channel has to be set in ULP code)
   adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_2_5);
   adc1_config_width(ADC_WIDTH_BIT_12);
   adc1_ulp_enable();

   // Map GPIO pin to RTC pin
   gpio_num_t reed_gpio_num = REED_PIN;
   int rtcio_num = rtc_io_number_get(reed_gpio_num);
   assert(rtc_gpio_is_valid_gpio(reed_gpio_num) && "GPIO used for pulse counting must be an RTC IO");
   printf("Using RTC pin %d for GPIO pin %d\n", rtcio_num, reed_gpio_num);

   // Configure the necessary ULP variables
   ulp_io_number = rtcio_num;
   ulp_int_to_second_max = 1000 / ULP_WAKEUP_MS;
   ulp_timeout_max = TIMEOUT_S * 1000 / ULP_WAKEUP_MS;

   // Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown
   rtc_gpio_init(reed_gpio_num);
   rtc_gpio_set_direction(reed_gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
   rtc_gpio_pulldown_dis(reed_gpio_num);
   //rtc_gpio_pullup_dis(reed_gpio_num);
   rtc_gpio_hold_en(reed_gpio_num);

   ulp_set_wakeup_period(0, ULP_WAKEUP_MS * 1000UL);

   // Disconnect GPIO12 and GPIO15 to remove current drain through pullup/pulldown resistors.
   // GPIO12 may be pulled high to select flash voltage.
   rtc_gpio_isolate(GPIO_NUM_12);
   rtc_gpio_isolate(GPIO_NUM_15);
#if CONFIG_IDF_TARGET_ESP32
   esp_deep_sleep_disable_rom_logging(); // suppress boot messages
#endif
}

void app_main(void) {
   // Counter for counting number of uploads since the start
   if (rtc_magic_number != MAGIC_NUMBER) {
      rtc_magic_number = MAGIC_NUMBER;
      rtc_id = 0;
   }

   esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
   if (cause != ESP_SLEEP_WAKEUP_ULP) {
      printf("Not ULP wakeup\n");

      // check 8M/256 clock against XTAL, get 8M/256 clock period
      uint32_t rtc_8md256_period = rtc_clk_cal(RTC_CAL_8MD256, 1024);
      uint32_t rtc_fast_freq_hz = 1000000ULL * (1 << RTC_CLK_CAL_FRACT) * 256 / rtc_8md256_period;
      printf("rtc_8md256_period = %d\n", rtc_8md256_period);
      printf("rtc_fast_freq_hz = %d\n", rtc_fast_freq_hz);

      init_ulp_program();
   } else {
      printf("Deep sleep wakeup\n");
      ulp_last_result &= UINT16_MAX;
      ulp_duration &= UINT16_MAX;
      ulp_revs &= UINT16_MAX;
      ulp_load_hi &= UINT16_MAX;
      ulp_load_lo &= UINT16_MAX;
      uint32_t load = ulp_load_hi * 1000UL + ulp_load_lo;
      printf("id = %d\n", rtc_id);
      printf("last_value = %d\n", ulp_last_result);
      printf("duration = %d\n", ulp_duration);
      printf("timeout = %d\n", TIMEOUT_S);
      printf("revolutions = %d\n", ulp_revs);
      printf("load_hi = %d\n", ulp_load_hi);
      printf("load_lo = %d\n", ulp_load_lo);
      printf("load_sum = %d\n", load);
      load /= ulp_revs;
      printf("load = %d\n", load);

#ifdef DO_UPLOAD
      ESP_ERROR_CHECK( nvs_flash_init() );
      ESP_ERROR_CHECK( esp_netif_init() );
      ESP_ERROR_CHECK( esp_event_loop_create_default() );
      ESP_ERROR_CHECK( wifi_connect() );

      // Get the current RSSI value
      wifi_ap_record_t ap;
      esp_wifi_sta_get_ap_info(&ap);
      printf("rssi = %d\n", ap.rssi);

      // Get the MAC for logging
      unsigned char mac[6] = {0};
      char mac_str[6*2+5+1];
      esp_efuse_mac_get_default(mac);
      esp_read_mac(mac, ESP_MAC_WIFI_STA);
      sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0] ,mac[1] ,mac[2] ,mac[3] ,mac[4] ,mac[5]);
      printf("mac = %s\n", mac_str);
   	
      // Construct the arguments, whole URL and the HTTP request
      char params[128];
      sprintf(params, "id=%d&dur=%d&to=%d&revs=%d&diff=%d&vbat=%d&rssi=%d&mac=%s", 
            rtc_id, ulp_duration, TIMEOUT_S, ulp_revs, load, 0, ap.rssi, mac_str);
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
   ESP_ERROR_CHECK( ulp_run(&ulp_entry - RTC_SLOW_MEM) );
   ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
   esp_deep_sleep_start();
}
