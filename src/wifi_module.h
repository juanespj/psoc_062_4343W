#ifndef WIFI_MODULE_H_
#define WIFI_MODULE_H_

#include <stdbool.h>

int app_wifi_init(void);
int app_wifi_connect(void);
void app_wifi_task(void);
bool app_wifi_is_up(void);
bool app_wifi_has_ipv4(void);

#endif /* WIFI_MODULE_H_ */
