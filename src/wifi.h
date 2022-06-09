#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "esp_netif.h"
#include "defines.h"


esp_err_t wifi_connect(void);


esp_err_t wifi_disconnect(void);


#endif /* WIFI_H */