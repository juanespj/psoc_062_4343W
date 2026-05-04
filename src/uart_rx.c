#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_rx, LOG_LEVEL_INF);

#define UART_DEVICE_NODE DT_NODELABEL(uart5)
#define RX_BUF_SIZE 64

static const struct device *uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
static uint8_t rx_buf[RX_BUF_SIZE];
static bool rx_enabled;

void uart_cb(const struct device *dev, void *user_data)
{
	int len;

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		len = uart_fifo_read(dev, rx_buf, RX_BUF_SIZE);
		if (len > 0) {
			for (int i = 0; i < len; i++) {
				LOG_INF("Received: 0x%02x ('%c')", rx_buf[i],
					(rx_buf[i] >= 32 && rx_buf[i] < 127) ? rx_buf[i] : '.');
			}
		}
	}
}

int uart_rx_init(void)
{
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	uart_irq_callback_set(uart_dev, uart_cb);
	uart_irq_rx_enable(uart_dev);
	rx_enabled = true;

	LOG_INF("UART RX initialized on %s", uart_dev->name);
	return 0;
}
