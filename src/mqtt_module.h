#ifndef MQTT_MODULE_H_
#define MQTT_MODULE_H_

#include <stdbool.h>

int app_mqtt_init(void);
void app_mqtt_task(bool net_ready);

#endif /* MQTT_MODULE_H_ */
