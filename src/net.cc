/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* 
 * net.c -- handles:
 *   all raw network i/o
 * 
 */


#include <fcntl.h>
#include "common.h"
#include "net.h"
#include "socket.h"
#include "misc.h"
#include "main.h"
#include "debug.h"
#include "dccutil.h"
#include "enclink.h"
#include "egg_timer.h"
#include "traffic.h"
#include "adns.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>

#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <setjmp.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <netinet/in.h>
#include <arpa/inet.h>	
#include <errno.h>
#include <sys/stat.h>

#if HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNITSTD_H */

extern egg_traffic_t 	traffic;

union sockaddr_union cached_myip4_so;
#ifdef USE_IPV6
union sockaddr_union cached_myip6_so;
#endif /* USE_IPV6 */

bool	cached_ip = 0;		/* Set to 1 after cache_my_ip is called */
bool    identd_hack = 0;	/* identd_open() won't work on most servers, dont even bother warning. */
char	botuser[21] = ""; 	/* Username of the user running the bot    */
int	socks_total = 0;	/* total number of sockets */
sock_list *socklist = NULL;	/* Enough to be safe			    */
int	MAXSOCKS = 0;
jmp_buf	alarmret;		/* Env buffer for alarm() returns	    */

/* This *MUST* be an ip */
char   firewall[121] = "";     /* Socks server for firewall                */
in_port_t firewallport = 1080;    /* Default port of Sock4/5 firewalls        */
/* Types of proxy */
#define PROXY_SOCKS   1
#define PROXY_SUN     2
#define PROXY_HTTP    3

/* I need an UNSIGNED long for dcc type stuff
 */
unsigned long my_atoul(const char *s)
{
  unsigned long ret = 0;

  while ((*s >= '0') && (*s <= '9')) {
    ret *= 10;
    ret += ((*s) - '0');
    ++s;
  }
  return ret;
}

/* get the protocol used on a socket */
int sockprotocol(int sock)
{
  struct sockaddr sa;
  socklen_t socklen = sizeof(sa);

  bzero(&sa, socklen);
  if (getsockname(sock, &sa, &socklen))
    return -1;
  else
    return sa.sa_family;
}

/* AF_INET-independent resolving routine */
static int get_ip(char *hostname, union sockaddr_union *so, int dns_type)
{
  if (!hostname || (hostname && !hostname[0]))
    return 1;

  memset(so, 0, sizeof(union sockaddr_union));
  debug1("get_ip(%s)", hostname);

  bd::Array<bd::String> hosts = dns_lookup_block(hostname, 10, dns_type);
  if (hosts.length() == 0)
    return -1;

  my_addr_t addr;
  get_addr(bd::String(hosts[0]).c_str(), &addr);

  if (addr.family == AF_INET) {
    so->sin.sin_family = AF_INET;
    memcpy(&so->sin.sin_addr, &addr.u.addr, sizeof(addr.u.addr));
#ifdef USE_IPV6
  } else if (addr.family == AF_INET6) {
    so->sin6.sin6_family = AF_INET6;
    memcpy(&so->sin6.sin6_addr, &addr.u.addr6, sizeof(addr.u.addr6));
#endif
  }
  return 0;
}

/* Initialize the socklist
 */
void init_net()
{
  MAXSOCKS = max_dcc + 10;

  if (socklist)
    socklist = (sock_list *) realloc((void *) socklist, sizeof(sock_list) * MAXSOCKS);
  else
    socklist = (sock_list *) calloc(1, sizeof(sock_list) * MAXSOCKS);

  for (int i = 0; i < MAXSOCKS; i++) {
    bzero(&socklist[i], sizeof(socklist[i]));
    socklist[i].flags = SOCK_UNUSED;
#ifdef EGG_SSL_EXT
    socklist[i].ssl = NULL;
#endif
    socklist[i].sock = -1;
  }
}

/* Get my ipv? ip
 */
const char *myipstr(int af_type)
{
  if (cached_ip) {
#ifdef USE_IPV6
    if (af_type == AF_INET6) {
      static char s[UHOSTLEN + 1] = "";

      inet_ntop(AF_INET6, &cached_myip6_so.sin6.sin6_addr, s, 119);
      s[120] = 0;
      return s;
    } else
#endif /* USE_IPV6 */
      if (af_type == AF_INET) {
        static char s[UHOSTLEN + 1] = "";

        inet_ntop(AF_INET, &cached_myip4_so.sin.sin_addr, s, 119);
        s[120] = 0;
        return s;
      }
  }

  return "";
}

/* see if it's necessary to set inaddr_any... because if we can't resolve, we die anyway */
void cache_my_ip()
{
#ifdef no
  cached_myip6_so.sin6.sin6_family = AF_INET6;
  cached_myip6_so.sin6.sin6_addr = in6addr_any;
  cached_myip4_so.sin.sin_family = AF_INET;
  cached_myip4_so.sin.sin_addr.s_addr = INADDR_ANY;
#endif

  int error = 0;

  debug0("cache_my_ip()");
  memset(&cached_myip4_so, 0, sizeof(union sockaddr_union));

#ifdef USE_IPV6
  bool any = 0;

  memset(&cached_myip6_so, 0, sizeof(union sockaddr_union));

  if (conf.bot->net.ip6) {
    if (get_ip(conf.bot->net.ip6, &cached_myip6_so, DNS_LOOKUP_AAAA))
      any = 1;
  } else if (conf.bot->net.host6) {
    if (get_ip(conf.bot->net.host6, &cached_myip6_so, DNS_LOOKUP_AAAA))
      any = 1;
  } else
    any = 1;

  if (any) {
    cached_myip6_so.sin6.sin6_family = AF_INET6;
    cached_myip6_so.sin6.sin6_addr = in6addr_any;
  }
#endif /* USE_IPV6 */

  if (conf.bot->net.ip) {
    if (get_ip(conf.bot->net.ip, &cached_myip4_so, DNS_LOOKUP_A))
      error = 1;
  } else if (conf.bot->net.host) {
    if (get_ip(conf.bot->net.host, &cached_myip4_so, DNS_LOOKUP_A))
      error = 2;
  } else {
/*
    char s[121] = "";

    gethostname(s, sizeof(s));

    if (get_ip(s, &cached_myip4_so)) {
*/
      /* error = 3; */
      cached_myip4_so.sin.sin_family = AF_INET;
      cached_myip4_so.sin.sin_addr.s_addr = INADDR_ANY;
//    }
  }

  if (error) {
    putlog(LOG_DEBUG, "*", "Hostname self-lookup error: %d", error);
    fatal("Hostname self-lookup failed.", 0);
  }

  cached_ip = 1;
}

/* Sets/Unsets options for a specific socket.
 * 
 * Returns:  0   - on success
 *           -1  - socket not found
 *           -2  - illegal operation
 */
int sockoptions(int sock, int operation, int sock_options)
{
  int i = -1;
  if ((i = findanysnum(sock)) != -1) {
    if (operation == EGG_OPTION_SET)
      socklist[i].flags |= sock_options;
    else if (operation == EGG_OPTION_UNSET)
      socklist[i].flags &= ~sock_options;
    else
      return -2;
    return 0;
  }
  return -1;
}

int
sock_read(bd::Stream& stream)
{
  int fd = -1;
  bd::String buf, type;

  while (stream.tell() < stream.length()) {
    buf = stream.getline().chomp();

    if (buf == STR("+sock"))
      return fd;

    type = newsplit(buf);
    if (type == STR("sock")) {
      int sock = atoi(newsplit(buf).c_str()), options = atoi(newsplit(buf).c_str());

      fd = allocsock(sock, options);
    }

    if (fd >= 0) {
#ifdef USE_IPV6
      if (type == STR("af"))
        socklist[fd].af = atoi(buf.c_str());
#endif
      if (type == STR("host"))
        socklist[fd].host = strdup(buf.c_str());
      if (type == STR("port"))
        socklist[fd].port = atoi(buf.c_str());
    }
  }
  return -1;
}

void 
sock_write(bd::Stream &stream, int fd)
{
  if (socklist[fd].sock > 0) {
    bd::String buf;

    stream << bd::String::printf(STR("-sock\n"));
    stream << bd::String::printf(STR("sock %d %d\n"), socklist[fd].sock, socklist[fd].flags);
#ifdef USE_IPV6
    stream << bd::String::printf(STR("af %u\n"), socklist[fd].af);
#endif
    if (socklist[fd].host)
      stream << bd::String::printf(STR("host %s\n"), socklist[fd].host);
    if (socklist[fd].port)
      stream << bd::String::printf(STR("port %d\n"), socklist[fd].port);
    stream << bd::String::printf(STR("+sock\n"));
  }    
}

/* Return a free entry in the socket entry
 */
int allocsock(int sock, int options)
{
  for (int i = 0; i < MAXSOCKS; i++) {
    if (socklist[i].flags & SOCK_UNUSED) {
      /* yay!  there is table space */
      socklist[i].inbuf = NULL;
      socklist[i].outbuf = NULL;
      socklist[i].flags = options;
      socklist[i].sock = sock;
      socklist[i].encstatus = 0;
      socklist[i].enclink = -1;
      socklist[i].gz = 0;
      bzero(&(socklist[i].okey), ENC_KEY_LEN + 1);
      bzero(&(socklist[i].ikey), ENC_KEY_LEN + 1);
      socks_total++;
      sdprintf("allocsock(%d) = %d", i, sock);
      return i;
    }
  }
  fatal("Socket table is full!", 0);
  return -1; /* Never reached */
}

/* Request a normal socket for i/o
 */
void setsock(int sock, int options)
{
  int i = allocsock(sock, options);
  bool parm;

  if (((sock != STDOUT) || backgrd) && !(socklist[i].flags & SOCK_NONSOCK)) {
    parm = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *) &parm, sizeof(int));

    parm = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *) &parm, sizeof(int));
  }
  if (options & SOCK_LISTEN) {
    /* Tris says this lets us grab the same port again next time */
    parm = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &parm, sizeof(int));
  }
  /* Yay async i/o ! */
  fcntl(sock, F_SETFL, O_NONBLOCK);
}

#ifdef USE_IPV6
int real_getsock(int options, int af_def, const char *fname, int line)
{
#else
int real_getsock(int options, const char *fname, int line)
{
  int af_def = AF_INET;
#endif /* USE_IPV6 */
  int sock = -1;

  if (!af_def) 
    af_def = AF_INET;
  sock = socket(af_def, SOCK_STREAM, 0);

  if (sock >= 0)
    setsock(sock, options);
  else if (!identd_hack)
    putlog(LOG_WARNING, "*", "Warning: Can't create new socket! (%s:%d): %s", fname, line, strerror(errno));
  else if (identd_hack)
    identd_hack = 0;
  return sock;
}

/* Done with a socket
 */
void real_killsock(int sock, const char *file, int line)
{
  if (sock < 0) {
    putlog(LOG_ERRORS, "*", "Attempt to kill socket -1 %s:%d", file, line);
    return;
  }

  int i = -1;
  if ((i = findanysnum(sock)) != -1) {
#ifdef EGG_SSL_EXT
    if (socklist[i].ssl) {
      SSL_shutdown(socklist[i].ssl);
      SSL_free(socklist[i].ssl);
      socklist[i].ssl = NULL;
    }
#endif
    close(socklist[i].sock);
    if (socklist[i].inbuf != NULL) {
      delete socklist[i].inbuf;
      socklist[i].inbuf = NULL;
    }
    if (socklist[i].outbuf != NULL) {
      delete socklist[i].outbuf;
      socklist[i].outbuf = NULL;
    }
    if (socklist[i].host)
      free(socklist[i].host);
    bzero(&socklist[i], sizeof(socklist[i]));
    socklist[i].flags = SOCK_UNUSED;
    socks_total--;
    sdprintf("killsock(%d, %s, %d) (socklist: %d)", sock, file, line, i);
    return;
  }
  putlog(LOG_MISC, "*", "Attempt to kill un-allocated socket %d %s:%d !!", sock, file, line);
}

/* Send connection request to proxy
 */
static int proxy_connect(int sock, const char *ip, in_port_t port, int proxy_type)
{
  sdprintf("proxy_connect(%d, %s, %d, %d)", sock, ip, port, proxy_type);
#ifdef USE_IPV6
  unsigned char x[32] = "";
  int af_ty = sockprotocol(sock);
#else
  unsigned char x[10] = "";
#endif /* USE_IPV6 */
  char s[256] = "";

  /* socks proxy */
  if (proxy_type == PROXY_SOCKS) {
    /* numeric IP? */
    if (is_dotted_ip(ip)) {
      in_addr_t ipaddr = ((in_addr_t) inet_addr(ip));
      memcpy(x, &ipaddr, 4);
    } else {	/* if not resolved, resolve it with blocking calls.. (shouldn't happen ever) */
      return -1;
    }
    int i = -1;
    if ((i = findanysnum(sock)) != -1)
      socklist[i].flags |= SOCK_PROXYWAIT;
#ifdef USE_IPV6
    if (af_ty == AF_INET6)
      simple_snprintf(s, sizeof s,
                   "\004\001%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%s",
                   (port >> 8) % 256, (port % 256), x[0], x[1], x[2], x[3],
                   x[4], x[5], x[6], x[7], x[9], x[9], x[10], x[11],  x[12],
                   x[13], x[14], x[15], botuser);
    else
#endif /* USE_IPV6 */
      simple_snprintf(s, sizeof s, "\004\001%c%c%c%c%c%c%s", (port >> 8) % 256,
                   (port % 256), x[0], x[1], x[2], x[3], botuser);
    tputs(sock, s, strlen(botuser) + 9);
  } else if (proxy_type == PROXY_SUN) {
    size_t len = simple_snprintf(s, sizeof s, "%s %d\n", ip, port);
    tputs(sock, s, len);
  } else if (proxy_type == PROXY_HTTP) {
    size_t len = simple_snprintf(s, sizeof s, "CONNECT %s:%d\n\n", ip, port);
    tputs(sock, s, len);
  }

  return sock;
}

/* FIXME: REPLACE WITH SOCK_NAME() */
void initialize_sockaddr(int af_type, const char *host, in_port_t port, union sockaddr_union *so)
{
    bzero(so, sizeof(*so));

    so->sa.sa_family = af_type;

    if (af_type == AF_INET) {
      so->sin.sin_family = AF_INET;
      if (host) {
        inet_pton(AF_INET, host, &so->sin.sin_addr);
        so->sin.sin_port = htons(port);
      } else {
        so->sin.sin_addr.s_addr = getmyip();
      }
#ifdef USE_IPV6
    } else {
      so->sin6.sin6_family = AF_INET6;
      if (host) {
        inet_pton(AF_INET6, host, &so->sin6.sin6_addr);
        so->sin6.sin6_port = htons(port);
      } else {
        memcpy(&so->sin6.sin6_addr, &cached_myip6_so.sin6.sin6_addr, 16);
       }
#endif /* USE_IPV6 */
    }
}

/* Starts a connection attempt to a socket
 * 
 * If given a normal hostname, this will be resolved to the corresponding
 * IP address first. PLEASE try to use the non-blocking dns functions
 * instead and then call this function with the IP address to avoid blocking.
 * 
 * returns <0 if connection refused:
 *   -1  strerror()/errno type error
 *   -2  can't resolve hostname
 */
int open_telnet_raw(int sock, const char *ipIn, in_port_t sport, bool proxy_on, int identd)
{
  static in_port_t port = 0;
  union sockaddr_union so;
  char ip[121] = "";
  int is_resolved = 0;
  int proxy_type = 0, proxy = proxy_on;

  /* firewall?  use socks */
  if (proxy) {
    switch (firewall[0]) {
      case '!':
        proxy_type = PROXY_SUN;
        strlcpy(ip, &firewall[1], sizeof(ip));
        break;
      case '@':
        proxy_type = PROXY_HTTP;
        strlcpy(ip, &firewall[1], sizeof(ip));
        break;
      default:
        proxy_type = PROXY_SOCKS;
        strlcpy(ip, firewall, sizeof(ip));
      break;
    }
    port = firewallport;
  } else {
    proxy_type = 0;
    strlcpy(ip, ipIn, sizeof(ip));
    port = sport;
  }

  socklen_t socklen;
  if (sport) {
    /* figure out which ip to bind to locally (v4 or v6) based on what the host ip is .. */
    if ((is_resolved = is_dotted_ip(ip))) {	/* already resolved */

      /* bind to our cached ip for v4/v6 depending on what the ip is */
      initialize_sockaddr(is_resolved, NULL, 0, &so);

      if (bind(sock, &so.sa, SIZEOF_SOCKADDR(so)) < 0) {
        putlog(LOG_DEBUG, "*", "Failed to bind to socket %d: %s", sock, strerror(errno));
        killsock(sock);
        return -1;
      }

      /* initialize so for connect using the host/port */
      initialize_sockaddr(is_resolved, ip, port, &so);
    } else {	/* if not resolved, resolve it with blocking calls.. (shouldn't happen ever) */
      sdprintf("WARNING: open_telnet_raw(%s,%d) was passed an unresolved hostname.", ip, port);
      killsock(sock);
      return -1;
    }
    socklen = SIZEOF_SOCKADDR(so);
  } else { // Unix domain socket
    so.sock_un.sun_family = AF_UNIX;
    strcpy(so.sock_un.sun_path, ip);
    socklen = SUN_LEN(&so.sock_un);
  }

  int i = -1;
  if ((i = findanysnum(sock)) != -1) {
    socklist[i].flags = (socklist[i].flags & ~SOCK_VIRTUAL) | SOCK_CONNECT;
    socklist[i].host = strdup(ipIn);
    socklist[i].port = port;
  }

  if (identd && sport) //Only open identd if not a unix domain socket
    identd_open(myipstr(is_resolved), ipIn, identd);

  int rc = -1;

  /* make the connect attempt */
  rc = connect(sock, (struct sockaddr *)&so.sa, socklen);

  if (rc < 0) {    
    if (errno == EINPROGRESS) {
      debug3("net: connect(%d, %s, %d)", sock, ipIn, sport);
      /* Firewall?  announce connect attempt to proxy */
      if (proxy)
	return proxy_connect(sock, ipIn, sport, proxy_type);
      return sock;		/* async success! */
    } else {
      sdprintf("connect(%d, %s, %d) failed: %s", sock, ip, sport, strerror(errno));
      killsock(sock);
      return -1;
    }
  }

  /* Synchronous? :/ */
  debug3("net: (BLOCKING) connect(%d, %s, %d)", sock, ipIn, sport);

  if (proxy)
    return proxy_connect(sock, ipIn, sport, proxy_type);

  return sock;
}

#ifdef EGG_SSL_EXT
int net_switch_to_ssl(int sock) {
  int i = 0;

  debug0("net_switch_to_ssl()");
  sleep(3); // Give some time to let the connect() go through.
  i = findanysnum(sock);
  if (i == MAXSOCKS) {
    debug0("Error while swithing to SSL - sock not found in list");
    return 0;
  }

  if (socklist[i].ssl) {
    debug0("Error while swithing to SSL - already in ssl");
    return 0;
  }
  socklist[i].ssl = SSL_new(ssl_ctx);
  if (!socklist[i].ssl) {
    debug0("Error while swithing to SSL - SSL_new() error");
    return 0;
  }

  SSL_set_fd(socklist[i].ssl, socklist[i].sock);
  int err = 0, timeout = 0;

  while ((err = SSL_connect(socklist[i].ssl)) <= 0) {
    if (timeout++ > 500) {
      err = 0;
      break;
    }
    int errs = SSL_get_error(socklist[i].ssl,err);
    if ((errs != SSL_ERROR_WANT_READ) && (errs != SSL_ERROR_WANT_WRITE) && (errs != SSL_ERROR_WANT_X509_LOOKUP)) {
      putlog(LOG_DEBUG, "*", "SSL_connect() = %d, %s", err, (char *)ERR_error_string(ERR_get_error(), NULL));
      goto error;
    }
    usleep(1000);
  }

  if (err == 1) {
    debug0("SSL_connect() success");
    return 1;
  }
error:
  debug0("Error while SSL_connect()");
  SSL_shutdown(socklist[i].ssl);
  SSL_free(socklist[i].ssl);
  socklist[i].ssl = NULL;
  return 0;
}
#endif

/* Ordinary non-binary connection attempt */
int open_telnet(const char *ip, in_port_t port, bool proxy, int identd)
{
  int sock = -1;
  
#ifdef USE_IPV6
  sock = getsock(0, is_dotted_ip(ip));
#else
  sock = getsock(0);
#endif /* USE_IPV6 */
  if (sock >= 0)
    return open_telnet_raw(sock, ip, port, proxy, identd);
  return -1;
}

/* Returns a socket number for a listening socket that will accept any
 * connection on a certain address -- port # is returned in port
 *
 * 'addr' is ignored if af_def is AF_INET6 -poptix (02/03/03)
 */
#ifdef USE_IPV6
int open_address_listen(const char* ip, int af_def, in_port_t *port) {
#else
int open_address_listen(const char* ip, in_port_t *port) {
   int af_def = AF_INET;
#endif /* USE_IPV6 */
//  if (firewall[0]) {
//    /* FIXME: can't do listen port thru firewall yet */
//    putlog(LOG_MISC, "*", "!! Cant open a listen port (you are using a firewall)");
//    return -1;
//  }

  int sock = 0;
  socklen_t addrlen;
  union sockaddr_union name;

#ifdef USE_IPV6
  if (af_def == AF_INET6) {
    struct sockaddr_in6 name6;
    sock = getsock(SOCK_LISTEN, af_def);

    if (sock < 0)
      return -1;

    debug2("Opening listen socket on port %d with AF_INET6, sock: %d", *port, sock);
    bzero((char *) &name6, sizeof(name6));
    name6.sin6_family = af_def;
    name6.sin6_port = htons(*port); /* 0 = just assign us a port */
    /* memcpy(&name6.sin6_addr, &in6addr_any, 16); */ /* this is the only way to get ipv6+ipv4 in 1 socket */
    memcpy(&name6.sin6_addr, &cached_myip6_so.sin6.sin6_addr, 16);
    if (bind(sock, (struct sockaddr *) &name6, sizeof(name6)) < 0) {
      if (!(identd_hack && *port == 113))
        putlog(LOG_DEBUG, "*", "Failed to bind to socket %d for listen on port %d: %s", sock, *port, strerror(errno));
      killsock(sock);
      return -1;
    }
    addrlen = sizeof(name6);
    if (getsockname(sock, (struct sockaddr *) &name6, &addrlen) < 0) {
      if (!(identd_hack && *port == 113))
        putlog(LOG_DEBUG, "*", "Failed to getsockname on socket %d for listen on port %d: %s", sock, *port, strerror(errno));
      killsock(sock);
      return -1;
    }
    *port = ntohs(name6.sin6_port);
    if (listen(sock, 1) < 0) {
      if (!(identd_hack && *port == 113))
        putlog(LOG_DEBUG, "*", "Failed to listen on socket %d for listen on port %d: %s", sock, *port, strerror(errno));
      killsock(sock);
      return -1;
    }
  } else {
    sock = getsock(SOCK_LISTEN, af_def);
#else
    sock = getsock(SOCK_LISTEN);
#endif /* USE_IPV6 */

    if (sock < 0)
      return -1;

    if (af_def == AF_UNIX)
      debug2("Opening listen socket on %s, sock: %d", ip, sock);
    else
      debug3("Opening listen socket on %s:%d with AF_INET, sock: %d", ip, *port, sock);

    bzero((char *) &name, sizeof(struct sockaddr *));
    if (af_def == AF_UNIX) {
      name.sock_un.sun_family = AF_UNIX;
      strcpy(name.sock_un.sun_path, ip);
      unlink(name.sock_un.sun_path);
      addrlen = SUN_LEN(&name.sock_un);
    } else {
      name.sin.sin_family = af_def;
      name.sin.sin_port = htons(*port); /* 0 = just assign us a port */
      name.sin.sin_addr.s_addr = inet_addr(ip);
      addrlen = sizeof(struct sockaddr_in);
    }

    if (bind(sock, (struct sockaddr *) &name, addrlen) < 0) {
      if (!(identd_hack && *port == 113)) {
        if (af_def == AF_UNIX)
          putlog(LOG_DEBUG, "*", "Failed to bind to socket %d for listen on %s: %s", sock, ip, strerror(errno));
        else
          putlog(LOG_DEBUG, "*", "Failed to bind to socket %d for listen on %s:%d: %s", sock, ip, *port, strerror(errno));
      }
      killsock(sock);
      return -1;
    }

    if (af_def != AF_UNIX) {
      /* what port are we on? */
      if (getsockname(sock, (struct sockaddr *) &name, &addrlen) < 0) {
        if (!(identd_hack && *port == 113))
          putlog(LOG_DEBUG, "*", "Failed to getsockname on socket %d for listen on port %d: %s", sock, *port, strerror(errno));
        killsock(sock);
        return -1;
      }
      *port = ntohs(name.sin.sin_port);
    }

    if (listen(sock, 1) < 0) {
      if (!(identd_hack && *port == 113) && af_def != AF_UNIX)
        putlog(LOG_DEBUG, "*", "Failed to listen on socket %d for on port %d: %s", sock, *port, strerror(errno));
      killsock(sock);
      return -1;
    }
#ifdef USE_IPV6
  }
#endif /* USE_IPV6 */

  if (af_def == AF_UNIX)
    debug2("Opened listen socket on %s, sock: %d", ip, sock);
  else
    debug3("Opened listen socket on %s:%d with AF_INET, sock: %d", ip, *port, sock);

  return sock;
}

/* Returns a socket number for a listening socket that will accept any
 * connection -- port # is returned in port
 */
int open_listen(in_port_t *port)
{
#ifdef USE_IPV6
  return open_address_listen(iptostr(getmyip()), AF_INET, port);
#else
  return open_address_listen(iptostr(getmyip()), port);
#endif /* USE_IPV6 */
}

/* Same as above, except this one can be called with an AF_ type
 * the above is being left in for compatibility, and should NOT LONGER BE USED IN THE CORE CODE.
 */

int open_listen_by_af(in_port_t *port, int af_def)
{
#ifdef USE_IPV6
  return open_address_listen(iptostr(getmyip()), af_def, port);
#else
  return -1;
#endif /* USE_IPV6 */
}

int open_listen_addr_by_af(const char *ip, in_port_t *port, int af_def)
{
  if (!ip)
    ip = iptostr(getmyip());
#ifdef USE_IPV6
  return open_address_listen(ip, af_def, port);
#else
  return -1;
#endif /* USE_IPV6 */
}

/* Returns the given network byte order IP address in the
 * dotted format - "##.##.##.##"
 */
const char *iptostr(in_addr_t ip)
{
  static char ipbuf[32];
  struct in_addr a;

  a.s_addr = ip;
  return inet_ntop(AF_INET, &a, ipbuf, sizeof(ipbuf));
}

/* Short routine to answer a connect received on a socket made previously
 * by open_listen ... returns hostname of the caller & the new socket
 * does NOT dispose of old "public" socket!
 */
int answer(int sock, char *caller, in_addr_t *ip, in_port_t *port, int binary)
{
  int new_sock;
  socklen_t addrlen;
  struct sockaddr_in from;
  int af_ty = sockprotocol(sock);
#ifdef USE_IPV6
  struct sockaddr_in6 from6;

  bzero(&from6, sizeof(struct sockaddr_in6));
  if (af_ty == AF_INET6) {
    addrlen = sizeof(from6);
    new_sock = accept(sock, (struct sockaddr *) &from6, &addrlen);
  } else {
#endif /* USE_IPV6 */
    addrlen = sizeof(struct sockaddr);
    new_sock = accept(sock, (struct sockaddr *) &from, &addrlen);
#ifdef USE_IPV6
  }
#endif /* USE_IPV6 */
  
  if (new_sock < 0)
    return -1;
  if (ip != NULL) {
#ifdef USE_IPV6
    /* Detect IPv4 in IPv6 mapped address .... */
    if (af_ty == AF_INET6 && (!IN6_IS_ADDR_V4MAPPED(&from6.sin6_addr))) {
      inet_ntop(AF_INET6, &from6.sin6_addr, caller, 119);
      caller[120] = 0;
      *ip = 0L;
    } else if (IN6_IS_ADDR_V4MAPPED(&from6.sin6_addr)) {    /* ...and convert it to plain (AF_INET) IPv4 address (openssh) */
      struct sockaddr_in *from4 = (struct sockaddr_in *)&from6;
      struct in_addr addr;

      memcpy(&addr, ((char *)&from6.sin6_addr) + 12, sizeof(addr));
      memset(&from, 0, sizeof(from));

      from4->sin_family = AF_INET;
      addrlen = sizeof(*from4);
      memcpy(&from4->sin_addr, &addr, sizeof(addr));

      *ip = from4->sin_addr.s_addr;
      strlcpy(caller, iptostr(*ip), 121);
      *ip = ntohl(*ip);
    } else {
#endif /* USE_IPV6 */
      if (af_ty == AF_UNIX) {
        struct sockaddr_un sock_un;
        socklen_t socklen = sizeof(sock_un);

        bzero(&sock_un, socklen);
        getsockname(sock, (struct sockaddr*) &sock_un, &socklen);
        strcpy(caller, sock_un.sun_path);
        *port = 0;
      } else {
        *ip = from.sin_addr.s_addr;
        strlcpy(caller, iptostr(*ip), 121);
        *ip = ntohl(*ip);
      }
#ifdef USE_IPV6 
    }
#endif /* USE_IPV6 */
  }
  if (port != NULL) {
#ifdef USE_IPV6
      if (af_ty == AF_INET6)
        *port = ntohs(from6.sin6_port);
      else if (af_ty == AF_INET)
#endif /* USE_IPV6 */
        *port = ntohs(from.sin_port);
    }
  /* Set up all the normal socket crap */
  setsock(new_sock, (binary ? SOCK_BINARY : 0));
  sdprintf("Answered socket %d: %s", new_sock, caller);
  return new_sock;
}

/* Like open_telnet, but uses ip & port specifications of dcc
   Take a longip and make into dotted form
 */
int open_telnet_dcc(int sock, char *ip, char *port)
{
  in_port_t p;
  unsigned long addr;
  char sv[100] = "";
  unsigned char c[4] = "";

#ifdef DEBUG_IPV6
  debug1("open_telnet_dcc %s", ip);
#endif /* DEBUG_IPV6 */
  if (port != NULL)
    p = atoi(port);
  else
    p = 2000;
#ifdef USE_IPV6
  if (sockprotocol(sock) == AF_INET6) {
#  ifdef DEBUG_IPV6
    debug0("open_telnet_dcc, af_inet6!");
#  endif /* DEBUG_IPV6 */
    strlcpy(sv, ip, sizeof(sv));
  } else {
#endif /* USE_IPV6 */
    if (ip != NULL)
      addr = my_atoul(ip);
    else
      addr = 0L;
    if (addr < (1 << 24))
      return -3;			/* fake address */
    c[0] = (addr >> 24) & 0xff;
    c[1] = (addr >> 16) & 0xff;
    c[2] = (addr >> 8) & 0xff;
    c[3] = addr & 0xff;
    simple_snprintf(sv, sizeof(sv), "%u.%u.%u.%u", c[0], c[1], c[2], c[3]);
#ifdef USE_IPV6
  }
  /* strcpy(sv,hostnamefromip(addr)); */
#  ifdef DEBUG_IPV6
  debug3("open_telnet_raw %s %d %d", sv, sock, p);
#  endif /* DEBUG_IPV6 */
#endif /* USE_IPV6 */
  return open_telnet_raw(sock, sv, p, 0);
}

/* Attempts to read from all the sockets in socklist
 * fills s with up to 511 bytes if available, and returns the array index
 * 
 * 		on EOF:  returns -1, with socket in len
 *     on socket error:  returns -2
 * if nothing is ready:  returns -3
 */
static int sockread(char *s, int *len)
{
  fd_set fd;
  int fds = 0, i, fdtmp, x;
  struct timeval t;
  int grab = SGRAB + 1;
  egg_timeval_t howlong;

  if (unlikely(timer_get_shortest(&howlong))) {
    /* No timer, default to 1 second. */
    t.tv_sec = 1;
    t.tv_usec = 0;
  } 
  else {
    t.tv_sec = howlong.sec;
    t.tv_usec = howlong.usec;
  }

  FD_ZERO(&fd);
  
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & (SOCK_UNUSED | SOCK_VIRTUAL))) {
      if (unlikely((socklist[i].sock == STDOUT) && !backgrd))
	fdtmp = STDIN;
      else
	fdtmp = socklist[i].sock;

      if (fdtmp > fds)
        fds = fdtmp;
      FD_SET(fdtmp, &fd);
    }
  }

  fds++;

  x = select(fds, &fd, NULL, NULL, &t);

  if (x > 0) {
    /* Something happened */
    for (i = 0; i < MAXSOCKS; i++) {
      if ((!(socklist[i].flags & SOCK_UNUSED)) && ((FD_ISSET(socklist[i].sock, &fd)) ||
#ifdef EGG_SSL_EXT
            ((socklist[i].ssl) && (SSL_pending(socklist[i].ssl))) ||
#endif
	  ((socklist[i].sock == STDOUT) && (!backgrd) && (FD_ISSET(STDIN, &fd))))) {
	if (socklist[i].flags & (SOCK_LISTEN | SOCK_CONNECT)) {
	  /* Listening socket -- don't read, just return activity */
	  /* Same for connection attempt */
	  /* (for strong connections, require a read to succeed first) */
	  if (socklist[i].flags & SOCK_PROXYWAIT) { /* drummer */
	    /* Hang around to get the return code from proxy */
	    grab = 10;
	  } else if (!(socklist[i].flags & SOCK_STRONGCONN)) {
	    debug1("net: connect! sock %d", socklist[i].sock);
	    s[0] = 0;
	    *len = 0;
	    return i;
	  }
	} else if (socklist[i].flags & SOCK_PASS) {
	  s[0] = 0;
	  *len = 0;
	  return i;
	}
	errno = 0;
	if (unlikely((socklist[i].sock == STDOUT) && !backgrd)) {
	  x = read(STDIN, s, grab);
#ifdef EGG_SSL_EXT
        } else if (socklist[i].ssl) {
            x = SSL_read(socklist[i].ssl,s,grab);
            if (x <= 0) {
              int err = SSL_get_error(socklist[i].ssl, x);
              x = -1;
              switch (err) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_X509_LOOKUP:
                  errno = EAGAIN;
                  break;
                case SSL_ERROR_SYSCALL:
                  switch (ERR_get_error()) {
                    case 0:
                      // EOF
                      break;
                    default:
                      putlog(LOG_DEBUG, "*", "SSL_read() unknown error: %s", strerror(errno));
                      break;
                  }
                  break;
                case SSL_ERROR_SSL:
                  putlog(LOG_DEBUG, "*", "SSL_read() = %d, %s", err, (char *)ERR_error_string(ERR_get_error(), NULL));
                  break;
              }
            }
#endif
        } else
          x = read(socklist[i].sock, s, grab);

	if (x <= 0) {		/* eof */
	  if (errno != EAGAIN) { /* EAGAIN happens when the operation would block 
				    on a non-blocking socket, if the socket is going
				    to die, it will die later, otherwise it will connect */
	    *len = socklist[i].sock;
	    socklist[i].flags &= ~SOCK_CONNECT;
	    debug1("net: eof!(read) socket %d", socklist[i].sock);
	    return -1;
	  } else {
	    debug3("sockread EAGAIN: %d %d (%s)", socklist[i].sock, errno, strerror(errno));
	    continue; /* EAGAIN */
	  }
	}
	s[x] = 0;
	*len = x;
	if (socklist[i].flags & SOCK_PROXYWAIT) {
	  debug2("net: socket: %d proxy errno: %d", socklist[i].sock, s[1]);
	  socklist[i].flags &= ~(SOCK_CONNECT | SOCK_PROXYWAIT);
	  switch (s[1]) {
	  case 90:		/* Success */
	    s[0] = 0;
	    *len = 0;
	    return i;
	  case 91:		/* Failed */
	    errno = ECONNREFUSED;
	    break;
	  case 92:		/* No identd */
	  case 93:		/* Identd said wrong username */
	    /* A better error message would be "socks misconfigured"
	     * or "identd not working" but this is simplest.
	     */
	    errno = ENETUNREACH;
	    break;
	  }
	  *len = socklist[i].sock;
	  return -1;
	}
	return i;
      }
    }
  } else if (x == -1)
    return -2;			/* socket error */
  else {
    s[0] = 0;
    *len = 0;
  }
  return -3;
}

/* sockgets: buffer and read from sockets
 * 
 * Attempts to read from all registered sockets for up to one second.  if
 * after one second, no complete data has been received from any of the
 * sockets, 's' will be empty, 'len' will be 0, and sockgets will return -3.
 * if there is returnable data received from a socket, the data will be
 * in 's' (null-terminated if non-binary), the length will be returned
 * in len, and the socket number will be returned.
 * normal sockets have their input buffered, and each call to sockgets
 * will return one line terminated with a '\n'.  binary sockets are not
 * buffered and return whatever coems in as soon as it arrives.
 * listening sockets will return an empty string when a connection comes in.
 * connecting sockets will return an empty string on a successful connect,
 * or EOF on a failed connect.
 * if an EOF is detected from any of the sockets, that socket number will be
 * put in len, and -1 will be returned.
 * the maximum length of the string returned is 512 (including null)
 *
 * Returns -4 if we handled something that shouldn't be handled by the
 * dcc functions. Simply ignore it.
 */

int sockgets(char *s, int *len)
{
  char xx[SGRAB + 4] = "", *p = NULL;
  int ret;
  size_t newline_index = size_t(-1);
  bool was_crlf = 0;

  for (int i = 0; i < MAXSOCKS; i++) {
    /* Check for stored-up data waiting to be processed */
    if (!(socklist[i].flags & SOCK_UNUSED) && !(socklist[i].flags & SOCK_BUFFER) && (socklist[i].inbuf != NULL)) {
      if (!(socklist[i].flags & SOCK_BINARY)) {
	/* Find EOL */
        newline_index = socklist[i].inbuf->find('\n');
        if (newline_index != size_t(-1) && newline_index != 0 && socklist[i].inbuf->charAt(newline_index - 1) == '\r') {
          --newline_index;
          was_crlf = 1;
        }

	if (newline_index != size_t(-1)) {
          // Split off a line
          bd::String line(socklist[i].inbuf->substring(0, newline_index));
          strlcpy(s, line.c_str(), SGRAB + 1);
          *(socklist[i].inbuf) += newline_index + 1;
          if (was_crlf)
           *(socklist[i].inbuf) += static_cast<size_t>(1);

          if (s[0] && socklist[i].encstatus)
            link_read(i, s);
            
          *len = strlen(s);

	  return socklist[i].sock;
	}
      } else {
        if (!socklist[i].inbuf)
          socklist[i].inbuf = new bd::String();
        /* i dont think any of this is *ever* called */
	/* Handling buffered binary data (must have been SOCK_BUFFER before). */
	if (socklist[i].inbuf->length() <= SGRAB) {
	  *len = socklist[i].inbuf->length();
	  memcpy(s, socklist[i].inbuf->data(), socklist[i].inbuf->length());
	  delete socklist[i].inbuf;
          socklist[i].inbuf = NULL;
	} else {
	  /* Split up into chunks of SGRAB bytes. */
	  *len = SGRAB;
	  memcpy(s, socklist[i].inbuf->data(), *len);
          *(socklist[i].inbuf) += static_cast<size_t>(*len);
	}
	return socklist[i].sock;
      }
    }
    /* Also check any sockets that might have EOF'd during write */
    if (!(socklist[i].flags & SOCK_UNUSED)
	&& (socklist[i].flags & SOCK_EOFD)) {
      s[0] = 0;
      *len = socklist[i].sock;
      return -1;
    }
  }
  /* No pent-up data of any worth -- down to business */
  *len = 0;
  ret = sockread(xx, len);
  if (ret < 0) {
    s[0] = 0;
    return ret;
  }
  /* Binary, listening and passed on sockets don't get buffered. */
  if (socklist[ret].flags & SOCK_CONNECT) {
    if (socklist[ret].flags & SOCK_STRONGCONN) {
      socklist[ret].flags &= ~SOCK_STRONGCONN;
      /* Buffer any data that came in, for future read. */
      socklist[ret].inbuf = new bd::String(xx, *len);
    }
    socklist[ret].flags &= ~SOCK_CONNECT;
    s[0] = 0;
    return socklist[ret].sock;
  }
  if (socklist[ret].flags & SOCK_BINARY) {
    memcpy(s, xx, *len);
    return socklist[ret].sock;
  }
  if ((socklist[ret].flags & SOCK_LISTEN) || (socklist[ret].flags & SOCK_PASS))
    return socklist[ret].sock;
  if (socklist[ret].flags & SOCK_BUFFER) {
    if (!socklist[ret].inbuf)
      socklist[ret].inbuf = new bd::String(xx, *len);
    else
      *(socklist[ret].inbuf) += bd::String(xx, *len);
    return -4;			/* Ignore this one. */
  }
  /* Might be necessary to prepend stored-up data! */
  if (socklist[ret].inbuf != NULL) {
    *(socklist[ret].inbuf) += bd::String(xx);
    if (socklist[ret].inbuf->length() < (SGRAB + 2)) {
      strlcpy(xx, socklist[ret].inbuf->c_str(), sizeof(xx));
      delete socklist[ret].inbuf;
      socklist[ret].inbuf = NULL;
    } else {
      // Take out an SGRAB sized chunk and advance the buffer
      strlcpy(xx, socklist[ret].inbuf->c_str(), SGRAB + 1);
      *(socklist[ret].inbuf) += static_cast<size_t>(SGRAB);
      /* (leave the rest to be post-pended later) */
    }
  }

  bool data = 0;

  /* Look for EOL marker; if it's there, i have something to show */
  p = strchr(xx, '\n');
  if (p == NULL)
    p = strchr(xx, '\r');
  if (p != NULL) {
    *p = 0;

    strlcpy(s, xx, SGRAB + 10); //buf@main.c
    memmove(xx, p + 1, strlen(p + 1) + 1);

/*    if (s[0] && strlen(s) && (s[strlen(s) - 1] == '\r')) */
    if (strlen(s) && s[strlen(s) - 1] == '\r')
      s[strlen(s) - 1] = 0;
    data = 1;			/* DCC_CHAT may now need to process a blank line */
/* NO! */
/* if (!s[0]) strcpy(s," ");  */
  } else {
    s[0] = 0;
    if (strlen(xx) >= SGRAB) {
      /* String is too long, so just insert fake \n */
      strlcpy(s, xx, SGRAB + 10); //buf@main.c
      xx[0] = 0;
      data = 1;
    }
  }
  if (s[0] && socklist[ret].encstatus)
    link_read(ret, s);

  *len = strlen(s);

  /* Anything left that needs to be saved? */
  if (!xx[0]) {
    if (data)
      return socklist[ret].sock;
    else
      return -3;
  }
  /* Prepend old data back */
  if (socklist[ret].inbuf != NULL) {
    socklist[ret].inbuf->insert(0, xx);
  } else {
    socklist[ret].inbuf = new bd::String(xx);
  }
  if (data) {
    return socklist[ret].sock;
  } else {
    return -3;
  }
}
/* Dump something to a socket
 * 
 * NOTE: Do NOT put Contexts in here if you want DEBUG to be meaningful!!
 */
void tputs(int z, const char *s, size_t len)
{
  if (z < 0)			/* um... HELLO?!  sanity check please! */
    return;			

  if (((z == STDOUT) || (z == STDERR)) && (!backgrd || use_stderr)) {
    if (write(z, s, len) == -1) {
      ;
    }
    return;
  }

  int x, idx;

  int i = -1;
  if ((i = findanysnum(z)) != -1) {
    if ((idx = findanyidx(z)) != -1 && dcc[idx].type->name) {
      if (!strncmp(dcc[idx].type->name, "BOT", 3))
        traffic.out_today.bn += len;
      else if (!strcmp(dcc[idx].type->name, "SERVER"))
        traffic.out_today.irc += len;
      else if (!strncmp(dcc[idx].type->name, "CHAT", 4))
        traffic.out_today.dcc += len;
      else if (!strncmp(dcc[idx].type->name, "FILES", 5))
        traffic.out_today.filesys += len;
      else if (!strcmp(dcc[idx].type->name, "SEND"))
        traffic.out_today.trans += len;
      else if (!strncmp(dcc[idx].type->name, "GET", 3))
        traffic.out_today.trans += len;
      else
        traffic.out_today.unknown += len;
    }

    if (len && socklist[i].encstatus)
      s = link_write(i, s, &len);

    if (socklist[i].outbuf != NULL) {
      /* Already queueing: just add it */
      *(socklist[i].outbuf) += bd::String(s, len);
      return;
    }
#ifdef EGG_SSL_EXT
    if (socklist[i].ssl) {
      x = SSL_write(socklist[i].ssl,s,len);
      if (x <= 0) {
        int err = SSL_get_error(socklist[i].ssl, x);
        x = -1;
        switch (err) {
          case SSL_ERROR_WANT_READ:
          case SSL_ERROR_WANT_WRITE:
          case SSL_ERROR_WANT_X509_LOOKUP:
            errno = EAGAIN;
            break;
          case SSL_ERROR_SYSCALL:
            switch (ERR_get_error()) {
              case 0:
                // EOF
                break;
              default:
                putlog(LOG_DEBUG, "*", "SSL_write() unknown error: %s", strerror(errno));
                break;
            }
            break;
          case SSL_ERROR_SSL:
            putlog(LOG_DEBUG, "*", "SSL_write() = %d, %s", err, (char *)ERR_error_string(ERR_get_error(), NULL));
            break;
        }
      }
    } else
#endif
      /* Try. */
      x = write(z, s, len);
    if (x == -1)
      x = 0;
    if ((size_t) x < len) {
      /* Socket is full, queue it */
      socklist[i].outbuf = new bd::String(&s[x], len - x);
    }
    //      if (socklist[i].encstatus && s)
    //        free(s);
    return;
  }

  /* Make sure we don't cause a crash by looping here */
  static int inhere = 0;

  if (unlikely(!inhere)) {
    char *tmp;

    inhere = 1;

    tmp = strldup(s, len); /* To null-terminate */
    putlog(LOG_MISC, "*", "!!! writing to nonexistent socket: %d", z);
    putlog(LOG_MISC, "*", "!-> '%s'", tmp);
    free(tmp);

    inhere = 0;
  }

/*  if (socklist[i].encstatus > 0)
    free(s); 
*/
}

int findanysnum(int sock)
{
  int i = 0;

  if (sock != -1)
    for (i = 0; i < MAXSOCKS; i++)
      if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED))
        return i;

  return -1;
}

int findanyidx(int sock)
{
  if (sock != -1)
    for (int idx = 0; idx < dcc_total; ++idx)
      if (dcc[idx].type && dcc[idx].sock == sock)
        return idx;

  return -1;
}


/* tputs might queue data for sockets, let's dump as much of it as
 * possible.
 */
void dequeue_sockets()
{
  int i, x, z = 0, fds = 0;
  fd_set wfds;
  struct timeval tv;

/* ^-- start poptix test code, this should avoid writes to sockets not ready to be written to. */
  FD_ZERO(&wfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0; 		/* we only want to see if it's ready for writing, no need to actually wait.. */
  for (i = 0; i < MAXSOCKS; i++) { 
    if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].outbuf != NULL) {
      FD_SET(socklist[i].sock, &wfds);
      if (socklist[i].sock > fds)
        fds = socklist[i].sock;
      z = 1; 
    }
  }
  if (!z)
    return; 			/* nothing to write */
  fds++;
  select(fds, NULL, &wfds, NULL, &tv);

/* end poptix */

  for (i = 0; i < MAXSOCKS; i++) { 
    if (!(socklist[i].flags & SOCK_UNUSED) &&
	(socklist[i].outbuf != NULL) && (FD_ISSET(socklist[i].sock, &wfds))) {
      /* Trick tputs into doing the work */
      errno = 0;
#ifdef EGG_SSL_EXT
      if (socklist[i].ssl) {
           x = SSL_write(socklist[i].ssl, socklist[i].outbuf->data(), socklist[i].outbuf->length());
           if (x <= 0) {
             int err = SSL_get_error(socklist[i].ssl, x);
             x = -1;
             switch (err) {
               case SSL_ERROR_WANT_READ:
               case SSL_ERROR_WANT_WRITE:
               case SSL_ERROR_WANT_X509_LOOKUP:
                 errno = EAGAIN;
                 break;
               case SSL_ERROR_SYSCALL:
                 switch (ERR_get_error()) {
                   case 0:
                     // EOF
                     break;
                   default:
                     putlog(LOG_DEBUG, "*", "SSL_write() unknown error: %s", strerror(errno));
                     break;
                 }
                 break;
               case SSL_ERROR_SSL:
                 putlog(LOG_DEBUG, "*", "SSL_write()/dequeue_sockets() = %d, %s", err, (char *)ERR_error_string(ERR_get_error(), NULL));
                 break;
             }
           }
      } else
#endif
        x = write(socklist[i].sock, socklist[i].outbuf->data(), socklist[i].outbuf->length());
      if ((x < 0) && (errno != EAGAIN)
#ifdef EBADSLT
	  && (errno != EBADSLT)
#endif /* EBADSLT */
#ifdef ENOTCONN
	  && (errno != ENOTCONN)
#endif /* EBADSLT */
	) {
	/* This detects an EOF during writing */
	debug3("net: eof!(write) socket %d (%s,%d)", socklist[i].sock, strerror(errno), errno);
	socklist[i].flags |= SOCK_EOFD;
      } else if ((size_t) x == socklist[i].outbuf->length()) {
	/* If the whole buffer was sent, nuke it */
	delete socklist[i].outbuf;
	socklist[i].outbuf = NULL;
      } else if (x > 0) {
        *(socklist[i].outbuf) += static_cast<size_t>(x);
      } else {
	debug3("dequeue_sockets(): errno = %d (%s) on %d", errno, strerror(errno), socklist[i].sock);
      }
      /* All queued data was sent. Call handler if one exists and the
       * dcc entry wants it.
       */
      if (!socklist[i].outbuf) {
	int idx = findanyidx(socklist[i].sock);

	if (idx >= 0 && dcc[idx].type && dcc[idx].type->outdone)
	  dcc[idx].type->outdone(idx);
      }
    }
  }
}
 

/*
 *      Debugging stuff
 */

void tell_netdebug(int idx)
{
  char s[80] = "";

  dprintf(idx, "Open sockets:");
  for (int i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED)) {
      simple_snprintf(s, sizeof(s), " %d", socklist[i].sock);
      if (socklist[i].flags & SOCK_BINARY)
	strlcat(s, " (binary)", sizeof(s));
      if (socklist[i].flags & SOCK_LISTEN)
	strlcat(s, " (listen)", sizeof(s));
      if (socklist[i].flags & SOCK_PASS)
	strlcat(s, " (passed on)", sizeof(s));
      if (socklist[i].flags & SOCK_CONNECT)
	strlcat(s, " (connecting)", sizeof(s));
      if (socklist[i].flags & SOCK_STRONGCONN)
	strlcat(s, " (strong)", sizeof(s));
      if (socklist[i].flags & SOCK_NONSOCK)
	strlcat(s, " (file)", sizeof(s));
      if (socklist[i].inbuf != NULL)
	simple_sprintf(&s[strlen(s)], " (inbuf: %zu)", socklist[i].inbuf ? socklist[i].inbuf->length() : 0);
      if (socklist[i].outbuf != NULL)
	simple_sprintf(&s[strlen(s)], " (outbuf: %zu)", socklist[i].outbuf ? socklist[i].outbuf->length() : 0);
      if (socklist[i].host)
        simple_sprintf(&s[strlen(s)], " (%s:%d)", socklist[i].host, socklist[i].port);
      strlcat(s, ",", sizeof(s));
      dprintf(idx, "%s", s);
    }
  }
  dprintf(idx, " done.\n");
}

/* Checks wether the referenced socket has data queued.
 *
 * Returns true if the incoming/outgoing (depending on 'type') queues
 * contain data, otherwise false.
 */
bool sock_has_data(int type, int sock)
{
  bool ret = 0;
  int i = -1;

  if ((i = findanysnum(sock)) != -1) {
    switch (type) {
      case SOCK_DATA_OUTGOING:
        ret = (socklist[i].outbuf != NULL);
        break;
      case SOCK_DATA_INCOMING:
        ret = (socklist[i].inbuf != NULL);
        break;
    }
  } else
    debug1("sock_has_data: could not find socket #%d, returning false.", sock);
  return ret;
}

bool socket_run() {
  static int socket_cleanup = 0;
  char buf[SGRAB + 10] = "";
  int i = 0, idx = 0;

  /* Only do this every so often. */
  if (!socket_cleanup) {
    socket_cleanup = 5;

    /* Check for server or dcc activity. */
    dequeue_sockets();
  } else
    --socket_cleanup;

  int xx = sockgets(buf, &i);

  if (xx >= 0) {		/* Non-error */
    if ((idx = findanyidx(xx)) != -1) {
      if (likely(dcc[idx].type->activity)) {
        /* Traffic stats */
        if (dcc[idx].type->name) {
          ContextNote(dcc[idx].type->name, buf);

          if (!strncmp(dcc[idx].type->name, "BOT", 3))
            traffic.in_today.bn += i + 1;
          else if (!strcmp(dcc[idx].type->name, "SERVER"))
            traffic.in_today.irc += i + 1;
          else if (!strncmp(dcc[idx].type->name, "CHAT", 4))
            traffic.in_today.dcc += i + 1;
          else if (!strncmp(dcc[idx].type->name, "FILES", 5))
            traffic.in_today.dcc += i + 1;
          else if (!strcmp(dcc[idx].type->name, "SEND"))
            traffic.in_today.trans += i + 1;
          else if (!strncmp(dcc[idx].type->name, "GET", 3))
            traffic.in_today.trans += i + 1;
          else
            traffic.in_today.unknown += i + 1;
        }
        dcc[idx].type->activity(idx, buf, (size_t) i);
      } else
        putlog(LOG_MISC, "*",
            STR("!!! untrapped dcc activity: type %s, sock %d"),
            dcc[idx].type->name, dcc[idx].sock);
    }
  } else if (unlikely(xx == -1)) {	/* EOF from someone */
    if (unlikely(i == STDOUT && !backgrd))
      fatal(STR("END OF FILE ON TERMINAL"), 0);
    if ((idx = findanyidx(i)) != -1) {
      sdprintf(STR("EOF on '%s' idx: %d"), dcc[idx].type ? dcc[idx].type->name : "unknown", idx);
      if (likely(dcc[idx].type->eof))
        dcc[idx].type->eof(idx);
      else {
        putlog(LOG_MISC, "*",
            STR("*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"),
            i, dcc[idx].type ? dcc[idx].type->name : "*UNKNOWN*");
        killsock(i);
        lostdcc(idx);
      }
    } else if (unlikely(idx == -1)) {
      putlog(LOG_MISC, "*", STR("(@) EOF socket %d, not a dcc socket, not anything."), i);
      close(i);
      killsock(i);
    }
  } else if (unlikely(xx == -2 && errno != EINTR)) {	/* select() error */
    putlog(LOG_MISC, "*", STR("* Socket error #%d; recovering."), errno);
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && dcc[i].sock != -1 && (fcntl(dcc[i].sock, F_GETFD, 0) == -1) && (errno = EBADF)) {
        putlog(LOG_MISC, "*",
            STR("DCC socket %d (type %s, name '%s') expired -- pfft"),
            dcc[i].sock, dcc[i].type->name, dcc[i].nick);
        killsock(dcc[i].sock);
        lostdcc(i);
        i--;
      }
    }
  } else if (xx == -3) {                      /* Idle */
    socket_cleanup = 0;	/* If we've been idle, cleanup & flush */
    return 1;
  }
  return 0;
}
/* vim: set sts=2 sw=2 ts=8 et: */
