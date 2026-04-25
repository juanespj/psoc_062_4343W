#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

#define LED0_NODE DT_NODELABEL(user_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static struct net_mgmt_event_callback net_cb;
static struct net_if *wifi_iface;

#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT | \
		     NET_EVENT_IPV4_ADDR_ADD)

static void on_net_event(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (iface != wifi_iface) {
		return;
	}

	if (event == NET_EVENT_WIFI_CONNECT_RESULT) {
		const struct wifi_status *status = cb->info;

		if (status && status->status) {
			LOG_ERR("Wi-Fi connect failed: %d", status->status);
		} else {
			LOG_INF("Wi-Fi connected, waiting for DHCP IPv4");
		}
	} else if (event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		const struct wifi_status *status = cb->info;

		LOG_INF("Wi-Fi disconnected (reason=%d)", status ? status->status : 0);
	} else if (event == NET_EVENT_IPV4_ADDR_ADD) {
		char addr_buf[NET_IPV4_ADDR_LEN];
		struct net_if_ipv4 *ipv4 = NULL;

		if (net_if_config_ipv4_get(iface, &ipv4) == 0 && ipv4 && ipv4->unicast[0].ipv4.is_used) {
			LOG_INF("IPv4 address: %s",
				net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr,
					      addr_buf, sizeof(addr_buf)));
		}
	}
}

static int wifi_connect(void)
{
	struct wifi_connect_req_params params = { 0 };
	int ret;
	int attempt;

	if (strlen(CONFIG_WIFI_SSID) == 0U) {
		LOG_ERR("CONFIG_WIFI_SSID is empty. Set it in prj.conf/overlay.");
		return -EINVAL;
	}

	params.ssid = (const uint8_t *)CONFIG_WIFI_SSID;
	params.ssid_length = strlen(CONFIG_WIFI_SSID);
	params.psk = (const uint8_t *)CONFIG_WIFI_PSK;
	params.psk_length = strlen(CONFIG_WIFI_PSK);
	params.security = params.psk_length > 0U ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_2_4_GHZ;

	LOG_INF("Wi-Fi connect request: ssid_len=%u psk_len=%u security=%d band=2.4GHz",
		params.ssid_length, params.psk_length, params.security);

	for (attempt = 1; attempt <= 3; attempt++) {
		ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params, sizeof(params));
		if (ret == 0) {
			LOG_INF("Connecting to SSID: %s", CONFIG_WIFI_SSID);
			return 0;
		}

		if (ret != -EAGAIN || attempt == 3) {
			LOG_ERR("NET_REQUEST_WIFI_CONNECT failed (%d) on attempt %d/3", ret, attempt);
			return ret;
		}

		LOG_WRN("Connect busy (-EAGAIN), retrying (%d/3)...", attempt);
		k_sleep(K_SECONDS(2));
	}

	return -EAGAIN;
}

int main(void)
{
	int err;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	wifi_iface = net_if_get_wifi_sta();
	if (!wifi_iface) {
		wifi_iface = net_if_get_default();
	}
	if (!wifi_iface) {
		LOG_ERR("No default network interface");
		return -ENODEV;
	}

	net_mgmt_init_event_callback(&net_cb, on_net_event, WIFI_EVENTS);
	net_mgmt_add_event_callback(&net_cb);

	err = wifi_connect();
	if (err) {
		LOG_ERR("Wi-Fi init failed (%d)", err);
	}

	while (1) {
		static uint32_t hb;

		gpio_pin_toggle_dt(&led);
		if ((hb++ % 25U) == 0U) {
			printk("app heartbeat (wifi iface up=%d)\n", net_if_is_up(wifi_iface));
		}
		k_msleep(CONFIG_BLINK_SLEEP_MS);
	}

	return 0;
}
