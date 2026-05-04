#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "mqtt_module.h"
#include "wifi_module.h"
#include "uart_rx.h"

#define LED0_NODE DT_NODELABEL(user_led)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_USER_LED 1
#else
#define HAS_USER_LED 0
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
	int err;

#if HAS_USER_LED
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
#endif

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

	err = uart_rx_init();
	if (err) {
		LOG_ERR("UART RX init failed (%d)", err);
	}

	while (1) {
		app_wifi_task();
		app_mqtt_task(app_wifi_has_ipv4());
#if HAS_USER_LED
		gpio_pin_toggle_dt(&led);
#endif
		k_msleep(CONFIG_BLINK_SLEEP_MS);
	}

	return 0;
}
