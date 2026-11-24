#ifndef CRUNCH_NETINET_IN_H
#define CRUNCH_NETINET_IN_H
#include <sys/socket.h>

typedef unsigned int in_addr_t;
typedef unsigned short in_port_t;
struct in_addr {
    in_addr_t s_addr;
};
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};
#macro INADDR_ANY 0U
#macro INADDR_LOOPBACK 0x7f000001U
#macro IPPROTO_TCP 6
#macro IPPROTO_UDP 17
#endif
