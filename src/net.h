#ifndef _NET_H
#define _NET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <setjmp.h>


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

#define SGRAB 2010         /* How much data to allow through sockets. */

enum {
  EGG_OPTION_SET        = 1,    /* Set option(s).               */
  EGG_OPTION_UNSET      = 2     /* Unset option(s).             */
};

/* Socket flags:
 */
#define SOCK_UNUSED     BIT0  /* empty socket                         */
#define SOCK_BINARY     BIT1  /* do not buffer input                  */
#define SOCK_LISTEN     BIT2  /* listening port                       */
#define SOCK_CONNECT    BIT3  /* connection attempt                   */
#define SOCK_NONSOCK    BIT4  /* used for file i/o on debug           */
#define SOCK_STRONGCONN BIT5  /* don't report success until sure      */
#define SOCK_EOFD       BIT6  /* it EOF'd recently during a write     */
#define SOCK_PROXYWAIT  BIT7  /* waiting for SOCKS traversal          */
#define SOCK_PASS       BIT8  /* passed on; only notify in case of traffic */
#define SOCK_VIRTUAL    BIT9  /* not-connected socket (dont read it!) */
#define SOCK_BUFFER     BIT10 /* buffer data; don't notify dcc funcs  */

/* Flags to sock_has_data
 */
enum {
  SOCK_DATA_OUTGOING,           /* Data in out-queue?                   */
  SOCK_DATA_INCOMING            /* Data in in-queue?                    */
};


#define iptolong(a)             (0xffffffff &                           \
                                 (long) (htonl((unsigned long) a)))
#define CONNECT_SSL 1
#define ACCEPT_SSL 2

#ifdef USE_IPV6
#define SIZEOF_SOCKADDR(so) ((so).sa.sa_family == AF_INET6 ? sizeof(so.sin6) : sizeof(so.sin))
#else
#define SIZEOF_SOCKADDR(so) (sizeof(so.sin))
#endif /* USE_IPV6 */

#if !defined(IN6_IS_ADDR_V4MAPPED)
# define IN6_IS_ADDR_V4MAPPED(a) \
        ((((u_int32_t *) (a))[0] == 0) && (((u_int32_t *) (a))[1] == 0) && \
         (((u_int32_t *) (a))[2] == htonl (0xffff)))
#endif /* !defined(IN6_IS_ADDR_V4MAPPED) */

union sockaddr_union {
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef USE_IPV6
  struct sockaddr_in6 sin6;
#endif /* USE_IPV6 */
};

/* This is used by the net module to keep track of sockets and what's
 * queued on them
 */
typedef struct {
  size_t outbuflen;             /* Outbuf could be binary data  */
  size_t inbuflen;              /* Inbuf could be binary data   */
#ifdef USE_IPV6
  unsigned int af;
#endif /* USE_IPV6 */
  sock_t sock;
  int encstatus;                        /* encrypted botlink */
  int oseed;                            /* botlink out seed */
  int iseed;                            /* botlink in seed */
  int gz; /* gzip compression */
#ifdef HAVE_SSL
  SSL           *ssl;
#endif /* HAVE_SSL */
  char *inbuf;
  char *outbuf;
  char *host;
  port_t port;
  short          flags;
  char okey[33];                        /* botlink enckey: out */
  char ikey[33];                        /* botlink enckey: in  */
} sock_list;


# define killsock(x)     	real_killsock((x),__FILE__,__LINE__)

unsigned long my_atoul(char *);
#ifdef HAVE_SSL
int ssl_cleanup();
int ssl_link(int, int);
#endif /* HAVE_SSL */
char *myipstr(int);
in_addr_t getmyip();
void cache_my_ip();
void setsock(sock_t, int);
sock_t allocsock(sock_t, int);

#ifdef USE_IPV6
#define getsock(opt, af) real_getsock(opt, af, __FILE__, __LINE__)
sock_t real_getsock(int, int, char *, int);
#else
#define getsock(opt) real_getsock(opt, __FILE__, __LINE__)
sock_t real_getsock(int, char *, int);
#endif /* USE_IPV6 */


int sockprotocol(sock_t);
int hostprotocol(char *);
void real_killsock(sock_t, const char *, int);
sock_t answer(sock_t, char *, in_addr_t *, port_t *, int);
int findanyidx(register sock_t);
sock_t findanysnum(register sock_t);
sock_t open_listen(port_t *);
sock_t open_listen_by_af(port_t *, int);
#ifdef USE_IPV6
sock_t open_address_listen(in_addr_t, int, port_t *);
#else
sock_t open_address_listen(in_addr_t, port_t *);
#endif /* USE_IPV6 */
sock_t open_telnet(char *, port_t);
sock_t open_telnet_dcc(sock_t, char *, char *);
sock_t open_telnet_raw(sock_t, char *, port_t);
void tputs(sock_t, char *, size_t);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
char *iptostr(in_addr_t);
bool sock_has_data(int, sock_t);
int sockoptions(sock_t sock, int operation, int sock_options);
ssize_t flush_inbuf(int idx);

extern union sockaddr_union 		cached_myip4_so;
#ifdef USE_IPV6
extern union sockaddr_union 		cached_myip6_so;
extern unsigned long			notalloc;
#endif /* USE_IPV6 */

extern char				firewall[], botuser[];
extern int				resolve_timeout, MAXSOCKS;
extern bool				identd_hack;
extern port_t				firewallport;
extern jmp_buf				alarmret;
extern sock_list			*socklist;

#endif /* !_NET_H */
