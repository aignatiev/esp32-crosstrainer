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

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

/* This function is called once after power-on reset, to load ULP program into
 * RTC memory and configure the ADC.
 */
static void init_ulp_program(void);

/* This function is called every time before going into deep sleep.
 * It starts the ULP program and resets measurement counter.
 */
static void start_ulp_program(void);

void app_main(void) {
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
      printf("Value=%d\n", ulp_last_result);
      printf("Bullshit=%d\n", ulp_bullshit);
      printf("Duration=%d\n", ulp_duration);
      printf("Steps=%d\n", ulp_steps);
      printf("Load=%d\n", ulp_load);
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

   ulp_io_number = rtcio_num; /* map from GPIO# to RTC_IO# */
   ulp_timeout_max = 50*20;  // 20s delay

   /* Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown */
   rtc_gpio_init(gpio_num);
   rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
   rtc_gpio_pulldown_dis(gpio_num);
   rtc_gpio_pullup_dis(gpio_num);
   rtc_gpio_hold_en(gpio_num);

   /* Set ULP wake up period to 20ms */
   ulp_set_wakeup_period(0, 20000);

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