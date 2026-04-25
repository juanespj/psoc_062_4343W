#include "mqtt_module.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(app_mqtt, LOG_LEVEL_INF);

#define MQTT_RX_BUFFER_SIZE 256
#define MQTT_TX_BUFFER_SIZE 256
#define MQTT_PAYLOAD_SIZE   128

static struct mqtt_client client;
static struct sockaddr_storage broker_storage;
static uint8_t rx_buffer[MQTT_RX_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_TX_BUFFER_SIZE];
static char payload_buffer[MQTT_PAYLOAD_SIZE];
static uint16_t message_id;
static bool mqtt_connected;
static int64_t next_connect_ms;
static int64_t next_publish_ms;

static void mqtt_event_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	ARG_UNUSED(c);

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			mqtt_connected = true;
			next_publish_ms = k_uptime_get();
			LOG_INF("MQTT connected");
		} else {
			LOG_ERR("MQTT connack error: %d", evt->result);
		}
		break;
	case MQTT_EVT_DISCONNECT:
		mqtt_connected = false;
		LOG_WRN("MQTT disconnected (%d)", evt->result);
		break;
	case MQTT_EVT_PUBACK:
		LOG_INF("MQTT publish ack id=%u", evt->param.puback.message_id);
		break;
	default:
		break;
	}
}

static int resolve_broker(struct sockaddr_storage *storage)
{
	struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res = NULL;
	char port_str[8];
	int ret;

	/* Allow literal IPv4 broker values and skip DNS in that case. */
	if (zsock_inet_pton(AF_INET, CONFIG_MQTT_BROKER_HOST, &addr4->sin_addr) == 1) {
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);
		return 0;
	}

	snprintk(port_str, sizeof(port_str), "%d", CONFIG_MQTT_BROKER_PORT);
	ret = zsock_getaddrinfo(CONFIG_MQTT_BROKER_HOST, port_str, &hints, &res);
	if (ret != 0 || !res) {
		if (ret == -EAGAIN) {
			LOG_WRN("MQTT DNS not ready yet for %s:%s (%d)", CONFIG_MQTT_BROKER_HOST,
				port_str, ret);
			return -EAGAIN;
		}
		LOG_ERR("MQTT DNS resolve failed for %s:%s (%d)", CONFIG_MQTT_BROKER_HOST, port_str,
			ret);
		return ret ? ret : -ENOENT;
	}

	memcpy(storage, res->ai_addr, res->ai_addrlen);
	zsock_freeaddrinfo(res);
	return 0;
}

static int mqtt_try_connect(void)
{
	int ret;

	ret = resolve_broker(&broker_storage);
	if (ret) {
		return ret;
	}

	mqtt_client_init(&client);
	client.broker = &broker_storage;
	client.evt_cb = mqtt_event_handler;
	client.client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
	client.client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
	client.password = NULL;
	client.user_name = NULL;
	client.protocol_version = MQTT_VERSION_3_1_1;
	client.transport.type = MQTT_TRANSPORT_NON_SECURE;
	client.rx_buf = rx_buffer;
	client.rx_buf_size = sizeof(rx_buffer);
	client.tx_buf = tx_buffer;
	client.tx_buf_size = sizeof(tx_buffer);
	client.keepalive = 30U;

	ret = mqtt_connect(&client);
	if (ret) {
		LOG_ERR("mqtt_connect failed (%d)", ret);
		return ret;
	}

	LOG_INF("MQTT connecting to %s:%d", CONFIG_MQTT_BROKER_HOST, CONFIG_MQTT_BROKER_PORT);
	return 0;
}

static void mqtt_disconnect_cleanup(void)
{
	if (client.transport.type == MQTT_TRANSPORT_NON_SECURE && client.transport.tcp.sock >= 0) {
		(void)zsock_close(client.transport.tcp.sock);
		client.transport.tcp.sock = -1;
	}
	mqtt_connected = false;
}

static int mqtt_publish_debug(bool net_ready)
{
	struct mqtt_publish_param param;
	int len;
	int ret;

	len = snprintk(payload_buffer, sizeof(payload_buffer),
		       "{\"uptime_ms\":%lu,\"wifi_up\":%d}",
		       (unsigned long)k_uptime_get(), net_ready ? 1 : 0);
	if (len <= 0 || len >= sizeof(payload_buffer)) {
		return -ENOMEM;
	}

	memset(&param, 0, sizeof(param));
	param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
	param.message.topic.topic.utf8 = (uint8_t *)CONFIG_MQTT_DEBUG_TOPIC;
	param.message.topic.topic.size = strlen(CONFIG_MQTT_DEBUG_TOPIC);
	param.message.payload.data = payload_buffer;
	param.message.payload.len = len;
	param.message_id = ++message_id;
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	ret = mqtt_publish(&client, &param);
	if (ret) {
		LOG_ERR("MQTT publish failed (%d)", ret);
		return ret;
	}

	LOG_INF("MQTT debug published topic=%s", CONFIG_MQTT_DEBUG_TOPIC);
	return 0;
}

int app_mqtt_init(void)
{
	memset(&client, 0, sizeof(client));
	client.transport.type = MQTT_TRANSPORT_NON_SECURE;
	client.transport.tcp.sock = -1;
	mqtt_connected = false;
	message_id = 0U;
	next_connect_ms = k_uptime_get();
	next_publish_ms = next_connect_ms + (CONFIG_MQTT_PUBLISH_INTERVAL_SEC * MSEC_PER_SEC);
	return 0;
}

void app_mqtt_task(bool net_ready)
{
	int ret;
	int64_t now = k_uptime_get();

	if (!net_ready) {
		if (mqtt_connected) {
			(void)mqtt_disconnect(&client, NULL);
		}
		mqtt_disconnect_cleanup();
		next_connect_ms = now + 3000;
		return;
	}

	if (!mqtt_connected && now >= next_connect_ms) {
		ret = mqtt_try_connect();
		if (ret) {
			next_connect_ms = now + 5000;
			return;
		}
		next_connect_ms = now + 5000;
	}

	if (client.transport.type == MQTT_TRANSPORT_NON_SECURE && client.transport.tcp.sock >= 0) {
		(void)mqtt_input(&client);
		(void)mqtt_live(&client);
	}

	if (mqtt_connected && now >= next_publish_ms) {
		ret = mqtt_publish_debug(net_ready);
		if (ret) {
			mqtt_disconnect_cleanup();
			next_connect_ms = now + 5000;
		}
		next_publish_ms = now + (CONFIG_MQTT_PUBLISH_INTERVAL_SEC * MSEC_PER_SEC);
	}
}
