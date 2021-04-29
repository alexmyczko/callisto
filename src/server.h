#ifndef CALLISTO_SERVER_H
#define CALLISTO_SERVER_H

#include <inttypes.h>

int server_init(uint16_t port, int ipv4, int ipv6);
void server_start();

#endif
