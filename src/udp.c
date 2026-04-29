#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "udp.h"

LOG_MODULE_REGISTER(udp_module, LOG_LEVEL_DBG);

static int sock = -1;
static struct sockaddr_in remote_addr;

K_THREAD_STACK_DEFINE(udp_rx_stack, CONFIG_UDP_STACK_SIZE);
static struct k_thread udp_rx_thread;

static void udp_rx_handler(void *p1, void *p2, void *p3)
{
    uint8_t buf[CONFIG_UDP_BUF_SIZE];
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    while (1) {
        ssize_t received = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                    (struct sockaddr *)&src, &src_len);
        if (received < 0) {
            LOG_ERR("recvfrom error: %d", errno);
            k_sleep(K_MSEC(100));
            continue;
        }
        buf[received] = '\0';
        LOG_INF("UDP RX [%d bytes]: %s", (int)received, buf);

        /* TODO: dispatch buf to your application logic here */
    }
}

int udp_start(void)
{
    struct sockaddr_in local_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(CONFIG_UDP_LOCAL_PORT),
        .sin_addr   = { .s_addr = INADDR_ANY },
    };

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        LOG_ERR("socket() failed: %d", errno);
        return -errno;
    }

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        LOG_ERR("bind() failed: %d", errno);
        close(sock);
        sock = -1;
        return -errno;
    }

    /* Pre-fill remote address from Kconfig */
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port   = htons(CONFIG_UDP_REMOTE_PORT);
    net_addr_pton(AF_INET, CONFIG_UDP_REMOTE_HOST, &remote_addr.sin_addr);

    k_thread_create(&udp_rx_thread, udp_rx_stack,
                    K_THREAD_STACK_SIZEOF(udp_rx_stack),
                    udp_rx_handler, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&udp_rx_thread, "udp_rx");

    LOG_INF("UDP socket bound on port %d, remote %s:%d",
            CONFIG_UDP_LOCAL_PORT,
            CONFIG_UDP_REMOTE_HOST,
            CONFIG_UDP_REMOTE_PORT);
    return 0;
}

int udp_send(const uint8_t *buf, size_t len)
{
    if (sock < 0) {
        return -ENOTCONN;
    }
    ssize_t sent = sendto(sock, buf, len, 0,
                          (struct sockaddr *)&remote_addr,
                          sizeof(remote_addr));
    if (sent < 0) {
        LOG_ERR("sendto() failed: %d", errno);
        return -errno;
    }
    return (int)sent;
}