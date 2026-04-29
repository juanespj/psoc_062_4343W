#ifndef UDP_H
#define UDP_H

#include <zephyr/kernel.h>

/**
 * Start the UDP receive thread and open the socket.
 * Call this after WiFi + DHCP are up.
 */
int udp_start(void);

/**
 * Send a datagram to the configured remote host/port.
 * @param buf  data to send
 * @param len  number of bytes
 * @return     bytes sent, or negative errno
 */
int udp_send(const uint8_t *buf, size_t len);

#endif /* UDP_H */