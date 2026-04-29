#include "wifi_module.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include "udp.h"
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(app_wifi, LOG_LEVEL_INF);

static struct net_mgmt_event_callback net_cb;
static struct net_if *wifi_iface;
static bool wifi_link_logged_up;
static bool wifi_prev_up;

static void log_ipv4_network_details(struct net_if *iface, struct net_if_ipv4 *ipv4)
{
	char ip_buf[NET_IPV4_ADDR_LEN];
	char gw_buf[NET_IPV4_ADDR_LEN];
#if defined(CONFIG_NET_DHCPV4)
	char mask_buf[NET_IPV4_ADDR_LEN];
#endif
	struct dns_resolve_context *dns_ctx;
	int if_index;

	if_index = net_if_get_by_iface(iface);
	(void)net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr, ip_buf, sizeof(ip_buf));
	(void)net_addr_ntop(AF_INET, &ipv4->gw, gw_buf, sizeof(gw_buf));
	LOG_INF("IPv4 details: if=%d ip=%s gw=%s", if_index, ip_buf, gw_buf);

#if defined(CONFIG_NET_DHCPV4)
	if (net_if_get_config(iface))
	{
		(void)net_addr_ntop(AF_INET, &net_if_get_config(iface)->dhcpv4.netmask, mask_buf,
							sizeof(mask_buf));
		LOG_INF("IPv4 details: netmask=%s", mask_buf);
	}
#endif

	dns_ctx = dns_resolve_get_default();
	if (!dns_ctx)
	{
		LOG_WRN("DNS context is not available");
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(dns_ctx->servers); i++)
	{
		const struct sockaddr *sa = &dns_ctx->servers[i].dns_server;
		char dns_buf[NET_IPV6_ADDR_LEN];

		if (sa->sa_family == AF_UNSPEC)
		{
			continue;
		}

		if (sa->sa_family == AF_INET)
		{
			const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

			(void)net_addr_ntop(AF_INET, &sin->sin_addr, dns_buf, sizeof(dns_buf));
			LOG_INF("DNS server[%u]: %s if=%d source=%d", (unsigned int)i, dns_buf,
					dns_ctx->servers[i].if_index, dns_ctx->servers[i].source);
		}
		else if (sa->sa_family == AF_INET6)
		{
			const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;

			(void)net_addr_ntop(AF_INET6, &sin6->sin6_addr, dns_buf, sizeof(dns_buf));
			LOG_INF("DNS server[%u]: %s if=%d source=%d", (unsigned int)i, dns_buf,
					dns_ctx->servers[i].if_index, dns_ctx->servers[i].source);
		}
	}
}

#define WIFI_EVENTS                                                     \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT | \
	 NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IF_UP)

static void on_net_event(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
	if (iface && iface != wifi_iface)
	{
		return;
	}

	if (event == NET_EVENT_WIFI_CONNECT_RESULT)
	{
		const struct wifi_status *status = cb->info;

		if (status && status->status)
		{
			LOG_ERR("Wi-Fi connect failed: %d", status->status);
		}
		else
		{
			LOG_INF("Wi-Fi connected, waiting for DHCP IPv4");
		}
	}
	else if (event == NET_EVENT_WIFI_DISCONNECT_RESULT)
	{
		const struct wifi_status *status = cb->info;

		LOG_INF("Wi-Fi disconnected (reason=%d)", status ? status->status : 0);
	}
	else if (event == NET_EVENT_IF_UP)
	{
		LOG_INF("Network interface is up");
		wifi_link_logged_up = true;
	}
	else if (event == NET_EVENT_IPV4_ADDR_ADD)
	{
		char addr_buf[NET_IPV4_ADDR_LEN];
		struct net_if_ipv4 *ipv4 = NULL;
		struct net_if *active_iface = iface ? iface : wifi_iface;

		if (net_if_config_ipv4_get(active_iface, &ipv4) == 0 && ipv4 &&
			ipv4->unicast[0].ipv4.is_used)
		{
			LOG_INF("IPv4 address: %s",
					net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr,
								  addr_buf, sizeof(addr_buf)));
			log_ipv4_network_details(active_iface, ipv4);
		}
	}
	// else if (event == NET_EVENT_IPV4_DHCP_BOUND)
	// {
	// 	LOG_INF("DHCP bound — starting UDP");
	// 	udp_start();

	// 	/* Example: send a hello datagram */
	// 	const char *msg = "hello from psoc6";
	// 	udp_send((const uint8_t *)msg, strlen(msg));
	// }
}

int app_wifi_init(void)
{
	wifi_iface = net_if_get_wifi_sta();
	if (!wifi_iface)
	{
		wifi_iface = net_if_get_default();
	}
	if (!wifi_iface)
	{
		LOG_ERR("No default network interface");
		return -ENODEV;
	}

	wifi_link_logged_up = false;
	wifi_prev_up = net_if_is_up(wifi_iface);

	net_mgmt_init_event_callback(&net_cb, on_net_event, WIFI_EVENTS);
	net_mgmt_add_event_callback(&net_cb);

	return 0;
}

int app_wifi_connect(void)
{
	struct wifi_connect_req_params params = {0};
	int ret;
	int attempt;

	if (!wifi_iface)
	{
		LOG_ERR("Wi-Fi module not initialized");
		return -EINVAL;
	}

	if (strlen(CONFIG_WIFI_SSID) == 0U)
	{
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

	for (attempt = 1; attempt <= 3; attempt++)
	{
		ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params, sizeof(params));
		if (ret == 0)
		{
			LOG_INF("Wi-Fi join request accepted for SSID: %s", CONFIG_WIFI_SSID);
			return 0;
		}

		if (ret != -EAGAIN || attempt == 3)
		{
			LOG_ERR("NET_REQUEST_WIFI_CONNECT failed (%d) on attempt %d/3", ret, attempt);
			return ret;
		}

		LOG_WRN("Connect busy (-EAGAIN), retrying (%d/3)...", attempt);
		k_sleep(K_SECONDS(2));
	}

	return -EAGAIN;
}

void app_wifi_task(void)
{
	bool now_up;

	if (!wifi_iface)
	{
		return;
	}

	now_up = net_if_is_up(wifi_iface);
	if (now_up && !wifi_prev_up && !wifi_link_logged_up)
	{
		LOG_INF("Wi-Fi link is up");
		wifi_link_logged_up = true;
	}
	else if (!now_up && wifi_prev_up)
	{
		LOG_WRN("Wi-Fi link went down");
		wifi_link_logged_up = false;
	}
	wifi_prev_up = now_up;
}

bool app_wifi_is_up(void)
{
	if (!wifi_iface)
	{
		return false;
	}

	return net_if_is_up(wifi_iface);
}

bool app_wifi_has_ipv4(void)
{
	struct net_if_ipv4 *ipv4 = NULL;

	if (!wifi_iface || !net_if_is_up(wifi_iface))
	{
		return false;
	}

	if (net_if_config_ipv4_get(wifi_iface, &ipv4) != 0 || !ipv4)
	{
		return false;
	}

	return ipv4->unicast[0].ipv4.is_used;
}
