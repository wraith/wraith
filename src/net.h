#ifndef _NET_H
#define _NET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"

#define iptolong(a)             (0xffffffff &                           \
                                 (long) (htonl((unsigned long) a)))

#ifndef MAKING_MODS
IP my_atoul(char *);
#ifdef HAVE_SSL
int ssl_cleanup();
#endif /* HAVE_SSL */
int ssl_link(int, int);
char *myipstr(int);
IP getmyip();
void cache_my_ip();
void neterror(char *);
void setsock(int, int);
int allocsock(int, int);
#ifdef USE_IPV6
int getsock(int, int);
#else
int getsock(int);
#endif /* USE_IPV6 */
int sockprotocol(int);
int hostprotocol(char *);
char *hostnamefromip(unsigned long);
void dropssl(int);
void real_killsock(int, const char *, int);
int answer(int, char *, unsigned long *, unsigned short *, int);
inline int open_listen(int *);
inline int open_listen_by_af(int *, int);
#ifdef USE_IPV6
int open_address_listen(IP, int, int *);
#else
int open_address_listen(IP, int *);
#endif /* USE_IPV6 */
int open_telnet(char *, int);
int open_telnet_dcc(int, char *, char *);
int open_telnet_raw(int, char *, int);
void tputs(int, char *, unsigned int);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
int sanitycheck_dcc(char *, char *, char *, char *);
void send_timesync(int);
int hostsanitycheck_dcc(char *, char *, IP, char *, char *);
char *iptostr(IP);
int sock_has_data(int, int);
int sockoptions(int sock, int operation, int sock_options);
int flush_inbuf(int idx);

#endif /* !MAKING_MODS */

#endif /* !_NET_H */
