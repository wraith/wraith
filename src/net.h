#ifndef _NET_H
#define _NET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <setjmp.h>
#include <bdlib/src/String.h>

#include "openssl.h"

namespace bd {
  class Stream;
}

#define ENC_KEY_LEN 32


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
#ifdef USE_IPV6
#define SIZEOF_SOCKADDR(so) ((so).sa.sa_family == AF_INET6 ? sizeof(so.sin6) : sizeof(so.sin))
#else
#define SIZEOF_SOCKADDR(so) (sizeof(so.sin))
#endif /* USE_IPV6 */

#if !defined(IN6_IS_ADDR_V4MAPPED)
# define IN6_IS_ADDR_V4MAPPED(a) \
        ((((uint32_t *) (a))[0] == 0) && (((uint32_t *) (a))[1] == 0) && \
         (((uint32_t *) (a))[2] == htonl (0xffff)))
#endif /* !defined(IN6_IS_ADDR_V4MAPPED) */

union sockaddr_union {			/* replaced by sockname_t */
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef USE_IPV6
  struct sockaddr_in6 sin6;
#endif /* USE_IPV6 */
  struct sockaddr_un sock_un;
};

/* This is used by the net module to keep track of sockets and what's
 * queued on them
 */
typedef struct {
#ifdef USE_IPV6
  unsigned int af;
#endif /* USE_IPV6 */
  int sock;
  int encstatus;                        /* encrypted botlink */
  int oseed;                            /* botlink out seed */
  int iseed;                            /* botlink in seed */
  int gz; /* gzip compression */
  int enclink;				/* new encrypted botlink */
#ifdef EGG_SSL_EXT
  SSL *ssl;
#endif
  bd::String* inbuf;
  bd::String* outbuf;
  char *host;
  in_port_t port;
  short          flags;
  char okey[ENC_KEY_LEN + 1];                        /* botlink enckey: out */
  char ikey[ENC_KEY_LEN + 1];                        /* botlink enckey: in  */
} sock_list;


# define killsock(x)     	real_killsock((x),__FILE__,__LINE__)

unsigned long my_atoul(const char *) __attribute__((pure));
const char *myipstr(int);
void cache_my_ip();
void setsock(int, int);
int allocsock(int, int);

#ifdef USE_IPV6
#define getsock(opt, af) real_getsock(opt, af, __FILE__, __LINE__)
int real_getsock(int, int, const char *, int);
#else
#define getsock(opt) real_getsock(opt, __FILE__, __LINE__)
int real_getsock(int, const char *, int);
#endif /* USE_IPV6 */


int sockprotocol(int) __attribute__((pure));
void real_killsock(int, const char *, int);
int answer(int, char *, in_addr_t *, in_port_t *, int);
int findanysnum(int) __attribute__((pure));
int findanyidx(int sock) __attribute__((pure));
int open_listen(in_port_t *);
int open_listen_by_af(in_port_t *, int);
int open_listen_addr_by_af(const char*, in_port_t *, int);
#ifdef USE_IPV6
int open_address_listen(const char*, int, in_port_t *);
#else
int open_address_listen(const char*, in_port_t *);
#endif /* USE_IPV6 */
int open_telnet(const char *, in_port_t, bool proxy = 0, int identd = 0);
int open_telnet_dcc(int, char *, char *);
int open_telnet_raw(int, const char *, in_port_t, bool, int = 0);
void tputs(int, const char *, size_t);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
const char *iptostr(in_addr_t);
bool sock_has_data(int, int) __attribute__((pure));
int sockoptions(int sock, int operation, int sock_options);
void init_net(void);
int sock_read(bd::Stream&);
void sock_write(bd::Stream&, int);
bool socket_run();
int net_switch_to_ssl(int sock);

extern union sockaddr_union 		cached_myip4_so;
#ifdef USE_IPV6
extern union sockaddr_union 		cached_myip6_so;
extern unsigned long			notalloc;
#endif /* USE_IPV6 */

extern char				firewall[], botuser[21];
extern int				MAXSOCKS, socks_total;
extern bool				identd_hack, cached_ip;
extern in_port_t				firewallport;
extern jmp_buf				alarmret;
extern sock_list			*socklist;

static inline in_addr_t
__attribute__((pure))
getmyip() {
  return cached_myip4_so.sin.sin_addr.s_addr;
}

#endif /* !_NET_H */
