#ifndef DEFINES_H
#define DEFINES_H

#define WIFI_SSID       "ssid"
#define WIFI_PASSWORD   "pwd"

// Stuff related to the Google Script
#define API_TOKEN "xyz"
#define WEB_SERVER "script.google.com"
#define WEB_URL "https://script.google.com/macros/s/" API_TOKEN "/exec?%s"

// Stuff related to OTA
#define OTA_TOKEN "xyz"
#define OTA_URL "https://drive.google.com/uc?export=download&id=" OTA_TOKEN

// This one defines whether there is any network stuff included (for debugging ULP)
#define DO_UPLOAD

// This one defines whether all debug prints are printed (for debugging)
#define PRINT_ALL

// Magic number to check whether the RTC memory is intact
#define MAGIC_NUMBER    0xdeadbeef

// Which pin REED switch is connected to
#define REED_PIN        GPIO_NUM_0

// Which pin activity LED is connected to
#define LED_PIN         GPIO_NUM_2

// ULP wakeup period in ms
#define ULP_WAKEUP_MS   10

// Excercise timeout in seconds
#define TIMEOUT_S       10

// Combined connection & upload timeout in seconds
#define UPLOAD_TIMEOUT_S       15

// Low battery treshold in mV
#define LOW_BATTERY_MV  4000

// How many measurements are saved before discarding (in case of no WiFi)
#define MEAS_COUNT      16

// Reference voltage of the ADC
//#define VREF            1100UL
#define VREF            (1100UL * 4 / 3)

// Voltage devision for battery voltage measurement
#define R1_VAL          47070UL
#define R2_VAL          9992UL

#endif