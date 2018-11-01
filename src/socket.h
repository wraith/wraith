#ifndef _SOCKET_H
#define _SOCKET_H

#include "types.h"

#define SOCKET_CLIENT   1
#define SOCKET_SERVER   2
#define SOCKET_BIND     4
#define SOCKET_NONBLOCK 8
#define SOCKET_TCP      16
#define SOCKET_UDP      32

int get_addr(const char *, my_addr_t *);

/* can be static if ever combined with net.h or a more integrated 1.9 is used ... */
typedef struct {
        int family;
        socklen_t len;
        union {
                struct sockaddr addr;
                struct sockaddr_in ipv4;
#ifdef USE_IPV6
                struct sockaddr_in6 ipv6;
#endif
        } u;
} sockname_t;

//#define SOCKNAME_INADDR(x) (((x).family == AF_INET6) ? x.u.ipv6.sin6_addr : x.u.ipv4.sin_addr)
//#define SOCKNAME_ADDR(x) (((x).family == AF_INET6) ? x.u.ipv6.sin6_addr.s6_addr : x.u.ipv4.sin_addr.s_addr)

int socket_name(sockname_t *name, const char *ipaddr, in_port_t port);


/* globals */

int socket_create(const char *dest_ip, int dest_port, const char *src_ip, int src_port, int flags);
//int socket_close(int sock);
int socket_set_nonblock(int desc, int value);
int socket_get_name(int sock, char **ip, int *port);
//int socket_get_peer_name(int sock, char **peer_ip, int *peer_port);
//int socket_get_error(int sock);
//int socket_accept(int sock, char **peer_ip, int *peer_port);
int is_dotted_ip(const char *ip) __attribute__((pure));
int socket_ip_to_uint(const char *ip, unsigned int *longip);
int socket_ipv6_to_dots(const char *ip, char *dots);


#endif /* !_SOCKET_H */

