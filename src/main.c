#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "mqtt_module.h"
#include "wifi_module.h"

#define LED0_NODE DT_NODELABEL(user_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
	int err;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	err = app_wifi_init();
	if (err) {
		LOG_ERR("Wi-Fi module init failed (%d)", err);
	}

	err = app_wifi_connect();
	if (err) {
		LOG_ERR("Wi-Fi init failed (%d)", err);
	}

	err = app_mqtt_init();
	if (err) {
		LOG_ERR("MQTT module init failed (%d)", err);
	}

	while (1) {
		static uint32_t hb;
		bool wifi_up;
		bool net_ready;

		gpio_pin_toggle_dt(&led);
		app_wifi_task();
		wifi_up = app_wifi_is_up();
		net_ready = app_wifi_has_ipv4();
		app_mqtt_task(net_ready);

		if ((hb++ % 25U) == 0U) {
			printk("app heartbeat (wifi up=%d ipv4=%d)\n", wifi_up, net_ready);
		}
		k_msleep(CONFIG_BLINK_SLEEP_MS);
	}

	return 0;
}
