#include "common.h"
#include "socket.h"
#include "adns.h"
#include "net.h"
#include "egg_timer.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int is_dotted_ip(const char *ip)
{
        char buf[512];

#ifdef USE_IPV6
        if (inet_pton(AF_INET6, ip, buf) > 0) 
          return AF_INET6;
#endif
        if (inet_pton(AF_INET, ip, buf) > 0)
          return AF_INET;
        return(0);
}


typedef struct connect_info {
        int dns_id;
        int timer_id;
        int idx;
        int port;
} connect_info_t;

/* If a connection times out, due to dns timeout or connect timeout. */
static int egg_connect_timeout(void *client_data)
{
        connect_info_t *connect_info = (connect_info_t *) client_data;
        int idx, dns_id;

        idx = connect_info->idx;
        dns_id = connect_info->dns_id;
        connect_info->timer_id = -1;
        if (dns_id != -1) {
                /* dns_cancel will call connect_host_resolved for us, which
                 * will filter up a "dns failed" error. */
                egg_dns_cancel(dns_id, 1);
        }
//        else {
//                detach(client_data, idx);
//                sockbuf_on_eof(idx, EGGNET_LEVEL, -1, "Connect timed out");
//        }
        return(0);
}
static connect_info_t *attach(int idx, const char *host, int port, int timeout)
{
        connect_info_t *connect_info = (connect_info_t *) calloc(1, sizeof(*connect_info));

        connect_info->port = port;
        connect_info->idx = idx;
        connect_info->dns_id = -1;
        connect_info->timer_id = -1;
//        sockbuf_attach_filter(connect_info->idx, &net_connect_filter, connect_info);
        if (timeout == 0) timeout = resolve_timeout;
        if (timeout > 0) {
                char buf[128];
                egg_timeval_t howlong;

                snprintf(buf, sizeof(buf), "idx %d to %s/%d", idx, host, port);
                howlong.sec = timeout;
                howlong.usec = 0;
                connect_info->timer_id = timer_create_complex(&howlong, buf, 
                    (Function) egg_connect_timeout, connect_info, 0);
        }
        return(connect_info);
}

static int detach(void *client_data, int idx)
{
        connect_info_t *connect_info = (connect_info_t *) client_data;

        if (connect_info->timer_id != -1) timer_destroy(connect_info->timer_id);
//        sockbuf_detach_filter(idx, &net_connect_filter, NULL);
        free(connect_info);
        return(0);
}
/*
int egg_client(int idx, const char *host, int port, const char *vip, int vport, int timeout)
{
        connect_info_t *connect_info;

        // If they don't have their own idx (-1), create one.
        if (idx < 0) idx = sockbuf_new();

        // Resolve the hostname.
        connect_info = attach(idx, host, port, timeout);
        connect_info->dns_id = egg_dns_lookup(host, -1, connect_host_resolved, connect_info);
        return(idx);
}
*/

typedef struct {
        int len;
        int family;
        union {
                struct sockaddr addr;
                struct sockaddr_in ipv4;
#ifdef USE_IPV6
                struct sockaddr_in6 ipv6;
#endif
        } u;
} sockname_t;


static int socket_name(sockname_t *name, const char *ipaddr, int port)
{
        egg_bzero(name, sizeof(*name));

        if (inet_pton(AF_INET, ipaddr, &name->u.ipv4.sin_addr) > 0) {
                name->len = sizeof(name->u.ipv4);
                name->family = PF_INET;
                name->u.ipv4.sin_port = htons(port);
                name->u.ipv4.sin_family = AF_INET;
                return(0);
        }

#ifdef USE_IPV6
        if (inet_pton(AF_INET6, ipaddr, &name->u.ipv6.sin6_addr) > 0) {
                name->len = sizeof(name->u.ipv6);
                name->family = PF_INET6;
                name->u.ipv6.sin6_port = htons(port);
                name->u.ipv6.sin6_family = AF_INET6;
                return(0);
        }
#endif

        /* Invalid name? Then use passive. */
        name->len = sizeof(name->u.ipv4);
        name->family = PF_INET;
        name->u.ipv4.sin_port = htons(port);
        name->u.ipv4.sin_family = AF_INET;
        return(0);
}

int socket_set_nonblock(int sock, int value)
{
        int oldflags = fcntl(sock, F_GETFL, 0);
        if (oldflags == -1) return -1;
        if (value != 0) oldflags |= O_NONBLOCK;
        else oldflags &= ~O_NONBLOCK;

        return fcntl(sock, F_SETFL, oldflags);
}

int socket_create(const char *dest_ip, int dest_port, const char *src_ip, int src_port, int flags)
{
        char *passive[] = {"::", "0.0.0.0"};
        int sock = -1, pfamily, try_ok;
        sockname_t dest_name, src_name;

        /* If no source ip address is given, try :: and 0.0.0.0 (passive). */
        for (try_ok = 0; try_ok < 2; try_ok++) {
                /* Resolve the ip addresses. */
                socket_name(&dest_name, dest_ip ? dest_ip : passive[try_ok], dest_port);
                socket_name(&src_name, src_ip ? src_ip : passive[try_ok], src_port);

                if (src_ip || src_port) flags |= SOCKET_BIND;

                if (flags & SOCKET_CLIENT) pfamily = dest_name.family;
                else if (flags & SOCKET_SERVER) pfamily = src_name.family;
                else {
                        errno = EADDRNOTAVAIL;
                        return(-1);
                }

                /* Create the socket. */
                if (flags & SOCKET_UDP) sock = socket(pfamily, SOCK_DGRAM, 0);
                else sock = socket(pfamily, SOCK_STREAM, 0);

                if (sock >= 0) break;
        }

        if (sock < 0) return(-2);

        allocsock(sock, 0);

        if (flags & SOCKET_NONBLOCK) socket_set_nonblock(sock, 1);

        /* Do the bind if necessary. */
        if (flags & (SOCKET_SERVER|SOCKET_BIND)) {
                int yes = 1;

                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
                if (bind(sock, &src_name.u.addr, src_name.len) != 0) return(-3);
                if (flags & SOCKET_SERVER) listen(sock, 50);
        }


        if (flags & SOCKET_CLIENT) {
          for (int i = 0; i < MAXSOCKS; i++) {
            if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == sock)) {
              socklist[i].flags = (socklist[i].flags & ~SOCK_VIRTUAL) | SOCK_CONNECT | SOCK_PASS;
              socklist[i].host = strdup(dest_ip);
              socklist[i].port = dest_port;
            }
          }

                if (connect(sock, &dest_name.u.addr, dest_name.len) != 0) {
                        if (errno != EINPROGRESS) return(-4);
                }
        }

        errno = 0;

        /* Yay, we're done. */
        return(sock);
}

int socket_ip_to_uint(const char *ip, unsigned int *longip)
{
        struct in_addr addr;

        inet_pton(AF_INET, ip, &addr);
        *longip = htonl(addr.s_addr);
        return(0);
}

/* Converts shorthand ipv6 notation (123:456::789) into long dotted-decimal
 * notation. 'dots' must be 16*4+1 = 65 bytes long. */
int socket_ipv6_to_dots(const char *ip, char *dots)
{
#ifndef USE_IPV6
        dots[0] = 0;
        return(-1);
#else
        struct in6_addr buf;

        dots[0] = 0;
        if (inet_pton(AF_INET6, ip, &buf) <= 0) return(-1);
        sprintf(dots, "%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u",
                buf.s6_addr[0], buf.s6_addr[1],
                buf.s6_addr[2], buf.s6_addr[3],
                buf.s6_addr[4], buf.s6_addr[5],
                buf.s6_addr[6], buf.s6_addr[7],
                buf.s6_addr[8], buf.s6_addr[9],
                buf.s6_addr[10], buf.s6_addr[11],
                buf.s6_addr[12], buf.s6_addr[13],
                buf.s6_addr[14], buf.s6_addr[15]
        );
        return(0);
#endif
}

