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

// ULP wakeup period in ms
#define ULP_WAKEUP_MS   10

// Excercise timeout in seconds
#define TIMEOUT_S       10

#endif