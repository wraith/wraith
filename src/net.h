#ifndef _NET_H
#define _NET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"


#ifdef HAVE_OPENSSL_SSL_H
# ifdef USE_SSL
#  include <openssl/ssl.h>
#  include <openssl/rand.h>
#  include <openssl/err.h>
#  undef HAVE_SSL
# endif /* USE_SSL */
/* #define HAVE_SSL 1 */
#endif /* HAVE_OPENSSL_SSL_H */

/*
 * Enable IPv6 debugging?
 */
#define DEBUG_IPV6 1
#define HAVE_IPV6 1

/* IPv6 sanity checks. */
#ifdef USE_IPV6
#  ifndef HAVE_IPV6
#    undef USE_IPV6
#  endif
#  ifndef HAVE_GETHOSTBYNAME2
#    ifndef HAVE_GETIPNODEBYNAME
#      undef USE_IPV6
#    endif
#  endif
#endif

#define SGRAB 2011         /* How much data to allow through sockets. */


#define iptolong(a)             (0xffffffff &                           \
                                 (long) (htonl((unsigned long) a)))


#ifndef MAKING_MODS

# define killsock(x)     	real_killsock((x),__FILE__,__LINE__)

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
