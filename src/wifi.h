#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "esp_netif.h"

#define WIFI_SSID       "ssid"
#define WIFI_PASSWORD   "pwd"


esp_err_t wifi_connect(void);


esp_err_t wifi_disconnect(void);


#endif /* WIFI_H */