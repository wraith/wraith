#include <fcntl.h>
#include "main.h"
#include "proto.h"
#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "blowfish_conf.h"
#include <sys/stat.h>
extern char netpass[];
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <setjmp.h>
#if !HAVE_GETDTABLESIZE
#ifdef FD_SETSIZE
#define getdtablesize() FD_SETSIZE
#else
#define getdtablesize() 200
#endif
#endif
extern struct dcc_t *dcc;
extern int backgrd, use_stderr, resolve_timeout, dcc_total;
extern unsigned long otraffic_irc_today, otraffic_bn_today,
  otraffic_dcc_today, otraffic_filesys_today, otraffic_trans_today,
  otraffic_unknown_today;
char hostname[121] = "";
char myip[121] = "";
char myip6[121] = "";
char hostname6[121] = "";
char firewall[121] = "";
int firewallport = 1080;
char botuser[21] = "wraith";
int dcc_sanitycheck = 0;
sock_list *socklist = NULL;
int MAXSOCKS = 0;
jmp_buf alarmret;
#define PROXY_SOCKS 1
#define PROXY_SUN 2
IP
my_atoul (char *s)
{
  IP ret = 0;
  while ((*s >= '0') && (*s <= '9'))
    {
      ret *= 10;
      ret += ((*s) - '0');
      s++;
    }
  return ret;
}

int
getprotocol (char *host)
{
#ifndef IPV6
  return AF_INET;
#else
  struct hostent *he;
  if (!setjmp (alarmret))
    {
      debug0 ("net.c:99 alarm with timeout");
      printf ("the timeout is: %d\n", resolve_timeout);
      alarm (resolve_timeout);
      debug0 ("net.c:99 alarm (Returned)");
      printf ("RESOLVING %s\n", host);
      he = gethostbyname2 (host, AF_INET6);
      debug0 ("net.c:102 alarm(0)");
      alarm (0);
    }
  else
    he = NULL;
  if (!he)
    {
      return AF_INET;
    }
  return AF_INET6;
#endif
}

void
init_net ()
{
  int i;
  for (i = 0; i < MAXSOCKS; i++)
    {
      bzero (&socklist[i], sizeof (socklist[i]));
      socklist[i].flags = SOCK_UNUSED;
    }
}
int
expmem_net ()
{
  int i, tot = 0;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED))
	{
	  if (socklist[i].inbuf != NULL)
	    tot += strlen (socklist[i].inbuf) + 1;
	  if (socklist[i].outbuf != NULL)
	    tot += socklist[i].outbuflen;
	}
    }
  return tot;
}
struct hostent *myipv6he;
char myipv6host[120];
IP
getmyip ()
{
  struct hostent *hp;
  char s[121];
  IP ip;
  struct in_addr *in;
  myipv6he = NULL;
#ifdef IPV6
  if (myip6[0])
    {
      myipv6he = gethostbyname2 (myip6, AF_INET6);
      if (myipv6he == NULL)
	fatal ("Hostname IPV6 self-lookup failed.", 0);
    }
  if (hostname6[0])
    {
      myipv6he = gethostbyname2 (hostname6, AF_INET6);
      if (myipv6he == NULL)
	fatal ("Hostname IPV6 self-lookup failed.", 0);
    }
  if (myipv6he != NULL)
    {
      inet_ntop (AF_INET6, &myipv6he, myipv6host, 119);
    }
#endif
  if (myip[0])
    {
      if ((myip[strlen (myip) - 1] >= '0')
	  && (myip[strlen (myip) - 1] <= '9'))
	return (IP) inet_addr (myip);
    }
  if (hostname[0])
    hp = gethostbyname (hostname);
  else
    {
      gethostname (s, 120);
      hp = gethostbyname (s);
    }
  if (hp == NULL && myipv6he == NULL)
    fatal ("Hostname self-lookup failed.  Set 'my-ip' in the config", 0);
  if (hp == NULL)
    return 0;
  in = (struct in_addr *) (hp->h_addr_list[0]);
  ip = (IP) (in->s_addr);
  return ip;
}

void
neterror (char *s)
{
  switch (errno)
    {
    case EADDRINUSE:
      strcpy (s, "Address already in use");
      break;
    case EADDRNOTAVAIL:
      strcpy (s, "Address invalid on remote machine");
      break;
    case EAFNOSUPPORT:
      strcpy (s, "Address family not supported");
      break;
    case EALREADY:
      strcpy (s, "Socket already in use");
      break;
    case EBADF:
      strcpy (s, "Socket descriptor is bad");
      break;
    case ECONNREFUSED:
      strcpy (s, "Connection refused");
      break;
    case EFAULT:
      strcpy (s, "Namespace segment violation");
      break;
    case EINPROGRESS:
      strcpy (s, "Operation in progress");
      break;
    case EINTR:
      strcpy (s, "Timeout");
      break;
    case EINVAL:
      strcpy (s, "Invalid namespace");
      break;
    case EISCONN:
      strcpy (s, "Socket already connected");
      break;
    case ENETUNREACH:
      strcpy (s, "Network unreachable");
      break;
    case ENOTSOCK:
      strcpy (s, "File descriptor, not a socket");
      break;
    case ETIMEDOUT:
      strcpy (s, "Connection timed out");
      break;
    case ENOTCONN:
      strcpy (s, "Socket is not connected");
      break;
    case EHOSTUNREACH:
      strcpy (s, "Host is unreachable");
      break;
    case EPIPE:
      strcpy (s, "Broken pipe");
      break;
#ifdef ECONNRESET
    case ECONNRESET:
      strcpy (s, "Connection reset by peer");
      break;
#endif
#ifdef EACCES
    case EACCES:
      strcpy (s, "Permission denied");
      break;
#endif
#ifdef EMFILE
    case EMFILE:
      strcpy (s, "Too many open files");
      break;
#endif
    case 0:
      strcpy (s, "Error 0");
      break;
    default:
      sprintf (s, "Unforseen error %d", errno);
      break;
    }
}
int
sockoptions (int sock, int operation, int sock_options)
{
  int i;
  for (i = 0; i < MAXSOCKS; i++)
    if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED))
      {
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
allocsock (int sock, int options, int af_ty)
{
  int i;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (socklist[i].flags & SOCK_UNUSED)
	{
	  socklist[i].inbuf = socklist[i].outbuf = NULL;
	  socklist[i].inbuflen = socklist[i].outbuflen = 0;
	  socklist[i].flags = options;
	  socklist[i].sock = sock;
	  socklist[i].af = af_ty;
	  socklist[i].encstatus = 0;
	  socklist[i].gz = 0;
	  egg_bzero (&socklist[i].okey, sizeof (socklist[i].okey));
	  egg_bzero (&socklist[i].ikey, sizeof (socklist[i].ikey));
	  return i;
	}
    }
  fatal ("Socket table is full!", 0);
  return -1;
}

void
setsock (int sock, int options, int af_ty)
{
  int i = allocsock (sock, options, af_ty);
  int parm;
  if (((sock != STDOUT) || backgrd) && !(socklist[i].flags & SOCK_NONSOCK))
    {
      parm = 1;
      setsockopt (sock, SOL_SOCKET, SO_KEEPALIVE, (void *) &parm,
		  sizeof (int));
      parm = 0;
      setsockopt (sock, SOL_SOCKET, SO_LINGER, (void *) &parm, sizeof (int));
    }
  if (options & SOCK_LISTEN)
    {
      parm = 1;
      setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (void *) &parm,
		  sizeof (int));
    }
  fcntl (sock, F_SETFL, O_NONBLOCK);
} int
getsock (int options, int AF_DEF)
{
  int sock = socket (AF_DEF, SOCK_STREAM, 0);
  if (sock >= 0)
    setsock (sock, options, AF_DEF);
  else
    putlog (LOG_MISC, "*", "Warning: Can't create new socket!");
  return sock;
}

void
killsock (register int sock)
{
  register int i;
  if (sock < 0)
    return;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED))
	{
	  close (socklist[i].sock);
	  if (socklist[i].inbuf != NULL)
	    {
	      nfree (socklist[i].inbuf);
	      socklist[i].inbuf = NULL;
	    }
	  if (socklist[i].outbuf != NULL)
	    {
	      nfree (socklist[i].outbuf);
	      socklist[i].outbuf = NULL;
	      socklist[i].outbuflen = 0;
	    }
	  egg_bzero (&socklist[i], sizeof (socklist[i]));
	  socklist[i].flags = SOCK_UNUSED;
	  return;
	}
    }
  putlog (LOG_MISC, "*", "Attempt to kill un-allocated socket %d !!", sock);
}
static int
proxy_connect (int sock, char *host, int port, int proxy)
{
#ifdef IPV6
  unsigned char x[32];
#else
  unsigned char x[10];
#endif
  struct hostent *hp;
  char s[256];
  int af_ty;
  int i;
  af_ty = getprotocol (host);
  if (proxy == PROXY_SOCKS)
    {
      if ((host[strlen (host) - 1] >= '0' && host[strlen (host) - 1] <= '9')
	  && af_ty != AF_INET6)
	{
	  IP ip = ((IP) inet_addr (host));
	  egg_memcpy (x, &ip, 4);
	}
      else
	{
	  if (!setjmp (alarmret))
	    {
#ifdef IPV6
	      debug0 ("net.c:404 alarm");
	      alarm (resolve_timeout);
	      if (af_ty == AF_INET6)
		{
		  hp = gethostbyname (host);
		}
	      else
		{
#endif
		  hp = gethostbyname (host);
#ifdef IPV6
		}
	      debug0 ("net.c:413 alarm");
	      alarm (0);
#endif
	    }
	  else
	    {
	      hp = NULL;
	    }
	  if (hp == NULL)
	    {
	      killsock (sock);
	      return -2;
	    }
	  egg_memcpy (x, hp->h_addr, hp->h_length);
	}
      for (i = 0; i < MAXSOCKS; i++)
	if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].sock == sock)
	  socklist[i].flags |= SOCK_PROXYWAIT;
#ifdef IPV6
      if (af_ty == AF_INET6)
	egg_snprintf (s, sizeof s,
		      "\004\001%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%s",
		      (port >> 8) % 256, (port % 256), x[0], x[1], x[2], x[3],
		      x[4], x[5], x[6], x[7], x[9], x[9], x[10], x[11], x[12],
		      x[13], x[14], x[15], botuser);
      else
#endif
	egg_snprintf (s, sizeof s, "\004\001%c%c%c%c%c%c%s",
		      (port >> 8) % 256, (port % 256), x[0], x[1], x[2], x[3],
		      botuser);
      tputs (sock, s, strlen (botuser) + 9);
    }
  else if (proxy == PROXY_SUN)
    {
      egg_snprintf (s, sizeof s, "%s %d\n", host, port);
      tputs (sock, s, strlen (s));
    }
  return sock;
}

int
getsockproto (int sock)
{
  int i;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (socklist[i].sock == sock)
	return socklist[i].af;
    }
  return AF_INET;
}

int
open_telnet_raw (int sock, char *server, int sport)
{
  struct sockaddr_in name;
#ifdef IPV6
  struct sockaddr_in6 name6;
  int rc;
  unsigned long succ;
#endif
  struct hostent *hp;
  char host[121];
  int i, port;
  int af_ty;
  volatile int proxy;
  if (firewall[0])
    {
      if (firewall[0] == '!')
	{
	  proxy = PROXY_SUN;
	  strcpy (host, &firewall[1]);
	}
      else
	{
	  proxy = PROXY_SOCKS;
	  strcpy (host, firewall);
	}
      port = firewallport;
    }
  else
    {
      proxy = 0;
      strcpy (host, server);
      port = sport;
    }
#ifdef IPV6
  af_ty = getprotocol (host);
  if (af_ty == AF_INET6)
    {
      succ = getmyip ();
      bzero ((char *) &name6, sizeof (struct sockaddr_in6));
      name6.sin6_family = AF_INET6;
      if (myip[0])
	{
	  if (myipv6he == NULL)
	    {
	      memcpy (&name6.sin6_addr, &in6addr_any, 16);
	    }
	  else
	    {
	      memcpy (&name6.sin6_addr, myipv6he->h_addr, myipv6he->h_length);
	    }
	}
      else
	{
	}
      if (bind (sock, (struct sockaddr *) &name6, sizeof (name6)) < 0)
	{
	  killsock (sock);
	  return -1;
	}
      bzero ((char *) &name6, sizeof (struct sockaddr_in6));
      name6.sin6_family = AF_INET6;
      name6.sin6_port = htons (port);
      if (!setjmp (alarmret))
	{
	  debug0 ("net.c:530 alarm");
	  alarm (resolve_timeout);
	  hp = gethostbyname2 (host, AF_INET6);
	  debug0 ("net.c:534 alarm");
	  alarm (0);
	}
      else
	{
	  hp = NULL;
	}
      if (hp == NULL)
	{
	  killsock (sock);
	  return -2;
	}
      egg_memcpy ((char *) &name6.sin6_addr, hp->h_addr, hp->h_length);
      name6.sin6_family = hp->h_addrtype;
    }
  else
    {
#endif
      egg_bzero ((char *) &name, sizeof (struct sockaddr_in));
      name.sin_family = AF_INET;
      name.sin_addr.s_addr = (myip[0] ? getmyip () : INADDR_ANY);
      if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
	  putlog (LOG_MISC, "*", "Error binding to ip: %s", strerror (errno));
	  return -1;
	}
      egg_bzero ((char *) &name, sizeof (struct sockaddr_in));
      name.sin_family = AF_INET;
      name.sin_port = htons (port);
      if ((host[strlen (host) - 1] >= '0')
	  && (host[strlen (host) - 1] <= '9'))
	name.sin_addr.s_addr = inet_addr (host);
      else
	{
	  debug0
	    ("WARNING: open_telnet_raw() is about to block in gethostbyname()!");
	  if (!setjmp (alarmret))
	    {
	      debug0 ("net.c:567 alarm");
	      alarm (resolve_timeout);
	      hp = gethostbyname (host);
	      debug0 ("net.c:569 alarm");
	      alarm (0);
	    }
	  else
	    {
	      hp = NULL;
	    }
	  if (hp == NULL)
	    return -2;
	  egg_memcpy (&name.sin_addr, hp->h_addr, hp->h_length);
	  name.sin_family = hp->h_addrtype;
	}
#ifdef IPV6
    }
#endif
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == sock))
	socklist[i].flags =
	  (socklist[i].flags & ~SOCK_VIRTUAL) | SOCK_CONNECT;
    }
#ifdef IPV6
  if (af_ty == AF_INET6)
    rc =
      connect (sock, (struct sockaddr *) &name6,
	       sizeof (struct sockaddr_in6));
  else
#endif
    rc =
      connect (sock, (struct sockaddr *) &name, sizeof (struct sockaddr_in));
  if (rc < 0)
    {
      if (errno == EINPROGRESS)
	{
	  if (firewall[0])
	    return proxy_connect (sock, server, sport, proxy);
	  return sock;
	}
      else
	return -1;
    }
  if (firewall[0])
    return proxy_connect (sock, server, sport, proxy);
  return sock;
}

int
open_telnet (char *server, int port)
{
  int sock = getsock (0, getprotocol (server)), ret =
    open_telnet_raw (sock, server, port);
  putlog (LOG_DEBUG, "*", "net.c / open_telnet");
  if (ret < 0)
    killsock (sock);
  return ret;
}

int
open_address_listen (IP addr, int *port)
{
  int sock = 0;
  int af_def;
  unsigned long ipp;
  unsigned int addrlen;
#ifdef IPV6
  struct sockaddr_in6 name6;
#endif
  struct sockaddr_in name;
  if (firewall[0])
    {
      putlog (LOG_MISC, "*",
	      "!! Cant open a listen port (you are using a firewall)");
      return -1;
    }
  if (getmyip () > 0)
    {
      af_def = AF_INET;
      sock = getsock (SOCK_LISTEN, af_def);
      if (sock < 1)
	return -1;
      egg_bzero ((char *) &name, sizeof (struct sockaddr_in));
      name.sin_family = AF_INET;
      name.sin_port = htons (*port);
      name.sin_addr.s_addr = addr;
      if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
	  killsock (sock);
	  goto tryv6;
	}
      addrlen = sizeof (name);
      if (getsockname (sock, (struct sockaddr *) &name, &addrlen) < 0)
	{
	  killsock (sock);
	  return -1;
	}
      *port = ntohs (name.sin_port);
      if (listen (sock, 1) < 0)
	{
	  killsock (sock);
	  goto tryv6;
	}
      return sock;
    }
tryv6:
#ifdef IPV6
  ipp = getmyip ();
  af_def = AF_INET6;
  if (af_def == AF_INET6 && myipv6he != NULL)
    {
      sock = getsock (SOCK_LISTEN, af_def);
      bzero ((char *) &name6, sizeof (name6));
      name6.sin6_family = af_def;
      name6.sin6_port = htons (*port);
      memcpy (&name6.sin6_addr, myipv6he->h_addr, myipv6he->h_length);
      if (bind (sock, (struct sockaddr *) &name6, sizeof (name6)) < 0)
	{
	  killsock (sock);
	  return -1;
	}
      addrlen = sizeof (name6);
      if (getsockname (sock, (struct sockaddr *) &name6, &addrlen) < 0)
	{
	  killsock (sock);
	  return -1;
	}
      *port = ntohs (name6.sin6_port);
      if (listen (sock, 1) < 0)
	{
	  killsock (sock);
	  return -1;
	}
    }
#endif
  return sock;
}
inline int
open_listen (int *port)
{
  return open_address_listen (myip[0] ? getmyip () : INADDR_ANY, port);
}

char *
hostnamefromip (unsigned long ip)
{
  struct hostent *hp;
  unsigned long addr = ip;
  unsigned char *p;
  static char s[UHOSTLEN];
  if (!setjmp (alarmret))
    {
      debug0 ("net.c:719 alarm");
      alarm (resolve_timeout);
      hp = gethostbyaddr ((char *) &addr, sizeof (addr), AF_INET);
      debug0 ("net.c:723 alarm");
      alarm (0);
    }
  else
    {
      hp = NULL;
    }
  if (hp == NULL)
    {
      p = (unsigned char *) &addr;
      sprintf (s, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
      return s;
    }
  strncpyz (s, hp->h_name, sizeof s);
  return s;
}

char *
iptostr (IP ip)
{
  struct in_addr a;
  a.s_addr = ip;
  return inet_ntoa (a);
}
unsigned long notalloc = 0;
int
answer (int sock, char *caller, unsigned long *ip, unsigned short *port,
	int binary)
{
  int new_sock;
  unsigned int addrlen;
  int af_ty;
#ifdef IPV6
  struct sockaddr_in6 from6;
#endif
  struct sockaddr_in from;
  af_ty = getsockproto (sock);
  addrlen = sizeof (struct sockaddr);
#ifdef IPV6
  if (af_ty == AF_INET6)
    {
      addrlen = sizeof (from6);
      new_sock = accept (sock, (struct sockaddr *) &from6, &addrlen);
    }
  else
    {
#endif
      addrlen = sizeof (struct sockaddr);
      new_sock = accept (sock, (struct sockaddr *) &from, &addrlen);
#ifdef IPV6
    }
#endif
  if (new_sock < 0)
    return -1;
  if (ip != NULL)
    {
#ifdef IPV6
      if (af_ty == AF_INET6)
	{
	  *ip = notalloc;
	  inet_ntop (AF_INET6, &from6, caller, 119);
	  caller[120] = 0;
	}
      else
	{
#endif
	  *ip = from.sin_addr.s_addr;
	  strncpyz (caller, iptostr (*ip), 121);
	  *ip = ntohl (*ip);
#ifdef IPV6
	}
#endif
    }
  if (port != NULL)
    {
#ifdef IPV6
      if (af_ty == AF_INET6)
	*port = ntohs (from6.sin6_port);
      else
#endif
	*port = ntohs (from.sin_port);
    }
  setsock (new_sock, (binary ? SOCK_BINARY : 0), af_ty);
  return new_sock;
}

int
open_telnet_dcc (int sock, char *server, char *port)
{
  int p;
  unsigned long addr;
  char sv[500];
  unsigned char c[4];
  if (port != NULL)
    p = atoi (port);
  else
    p = 2000;
#ifdef IPV6
  if (getprotocol (server) == AF_INET6)
    {
      server[0] = 0;
      if (strlen (server) < 500)
	strcpy (sv, server);
    }
  else
    {
#endif
      if (server != NULL)
	addr = my_atoul (server);
      else
	addr = 0L;
      if (addr < (1 << 24))
	return -3;
      c[0] = (addr >> 24) & 0xff;
      c[1] = (addr >> 16) & 0xff;
      c[2] = (addr >> 8) & 0xff;
      c[3] = addr & 0xff;
      sprintf (sv, "%u.%u.%u.%u", c[0], c[1], c[2], c[3]);
#ifdef IPV6
    }
#endif
  p = open_telnet_raw (sock, sv, p);
  return p;
}
static int
sockread (char *s, int *len)
{
  fd_set fd;
  int fds, i, x, fdtmp;
  struct timeval t;
  int grab = sgrab;
  fds = getdtablesize ();
#ifdef FD_SETSIZE
  if (fds > FD_SETSIZE)
    fds = FD_SETSIZE;
#endif
  t.tv_sec = 1;
  t.tv_usec = 0;
  FD_ZERO (&fd);
  for (i = 0; i < MAXSOCKS; i++)
    if (!(socklist[i].flags & (SOCK_UNUSED | SOCK_VIRTUAL)))
      {
	if ((socklist[i].sock == STDOUT) && !backgrd)
	  fdtmp = STDIN;
	else
	  fdtmp = socklist[i].sock;
	FD_SET (fdtmp, &fd);
      }
#ifdef HPUX_HACKS
#ifndef HPUX10_HACKS
  x = select (fds, (int *) &fd, (int *) NULL, (int *) NULL, &t);
#else
  x = select (fds, &fd, NULL, NULL, &t);
#endif
#else
  x = select (fds, &fd, NULL, NULL, &t);
#endif
  if (x > 0)
    {
      for (i = 0; i < MAXSOCKS; i++)
	{
	  if ((!(socklist[i].flags & SOCK_UNUSED))
	      && ((FD_ISSET (socklist[i].sock, &fd))
		  || ((socklist[i].sock == STDOUT) && (!backgrd)
		      && (FD_ISSET (STDIN, &fd)))))
	    {
	      if (socklist[i].flags & (SOCK_LISTEN | SOCK_CONNECT))
		{
		  if (socklist[i].flags & SOCK_PROXYWAIT)
		    {
		      grab = 10;
		    }
		  else if (!(socklist[i].flags & SOCK_STRONGCONN))
		    {
		      debug1 ("net: connect! sock %d", socklist[i].sock);
		      s[0] = 0;
		      *len = 0;
		      return i;
		    }
		}
	      else if (socklist[i].flags & SOCK_PASS)
		{
		  s[0] = 0;
		  *len = 0;
		  return i;
		}
	      errno = 0;
	      if ((socklist[i].sock == STDOUT) && !backgrd)
		x = read (STDIN, s, grab);
	      else
		x = read (socklist[i].sock, s, grab);
	      if (x <= 0)
		{
		  if (errno != EAGAIN)
		    {
		      *len = socklist[i].sock;
		      socklist[i].flags &= ~SOCK_CONNECT;
		      debug1 ("net: eof!(read) socket %d", socklist[i].sock);
		      return -1;
		    }
		  else
		    {
		      debug3 ("sockread EAGAIN: %d %d (%s)", socklist[i].sock,
			      errno, strerror (errno));
		      continue;
		    }
		}
	      s[x] = 0;
	      *len = x;
	      if (socklist[i].flags & SOCK_PROXYWAIT)
		{
		  debug2 ("net: socket: %d proxy errno: %d", socklist[i].sock,
			  s[1]);
		  socklist[i].flags &= ~(SOCK_CONNECT | SOCK_PROXYWAIT);
		  switch (s[1])
		    {
		    case 90:
		      s[0] = 0;
		      *len = 0;
		      return i;
		    case 91:
		      errno = ECONNREFUSED;
		      break;
		    case 92:
		    case 93:
		      errno = ENETUNREACH;
		      break;
		    }
		  *len = socklist[i].sock;
		  return -1;
		}
	      return i;
	    }
	}
    }
  else if (x == -1)
    return -2;
  else
    {
      s[0] = 0;
      *len = 0;
    }
  return -3;
}

char *
botlink_decrypt (int snum, char *src)
{
  char *line = NULL;
  int i;
  line = decrypt_string (socklist[snum].ikey, src);
  if (socklist[snum].iseed)
    {
      for (i = 0; i <= 3; i++)
	*(dword *) & socklist[snum].ikey[i * 4] =
	  prand (&socklist[snum].iseed, 0xFFFFFFFF);
      if (!socklist[snum].iseed)
	socklist[snum].iseed++;
    }
  strcpy (src, line);
  nfree (line);
  return src;
}

char *
botlink_encrypt (int snum, char *src)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  int bufpos = 0, i;
  srcbuf = nmalloc (strlen (src) + 10);
  strcpy (srcbuf, src);
  line = srcbuf;
  if (!line)
    return NULL;
  eol = strchr (line, '\n');
  i = 0;
  while (eol)
    {
      *eol++ = 0;
      eline = encrypt_string (socklist[snum].okey, line);
      if (socklist[snum].oseed)
	{
	  for (i = 0; i <= 3; i++)
	    *(dword *) & socklist[snum].okey[i * 4] =
	      prand (&socklist[snum].oseed, 0xFFFFFFFF);
	  if (!socklist[snum].oseed)
	    socklist[snum].oseed++;
	}
      buf = nrealloc (buf, bufpos + strlen (eline) + 10);
      strcpy ((char *) &buf[bufpos], eline);
      strcat (buf, "\n");
      bufpos = strlen (buf);
      line = eol;
      eol = strchr (line, '\n');
      nfree (eline);
  } if (line[0])
    {
      eline = encrypt_string (socklist[snum].okey, line);
      if (socklist[snum].oseed)
	{
	  *(dword *) & socklist[snum].okey[i * 4] =
	    prand (&socklist[snum].oseed, 0xFFFFFFFF);
	  if (!socklist[snum].oseed)
	    socklist[snum].oseed++;
	}
      buf = nrealloc (buf, bufpos + strlen (eline) + 10);
      strcpy ((char *) &buf[bufpos], eline);
      strcat (buf, "\n");
      nfree (eline);
    }
  nfree (srcbuf);
  return buf;
}

int
sockgets (char *s, int *len)
{
  char xx[sgrab + 3], *p, *px;
  int ret, i, data = 0, grab = sgrab + 1;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED)
	  && !(socklist[i].flags & SOCK_BUFFER)
	  && (socklist[i].inbuf != NULL))
	{
	  if (!(socklist[i].flags & SOCK_BINARY))
	    {
	      p = strchr (socklist[i].inbuf, '\n');
	      if (p == NULL)
		p = strchr (socklist[i].inbuf, '\r');
	      if (p != NULL)
		{
		  *p = 0;
		  if (strlen (socklist[i].inbuf) > (grab - 2))
		    socklist[i].inbuf[(grab - 2)] = 0;
		  strcpy (s, socklist[i].inbuf);
		  px = (char *) nmalloc (strlen (p + 1) + 1);
		  strcpy (px, p + 1);
		  nfree (socklist[i].inbuf);
		  if (px[0])
		    socklist[i].inbuf = px;
		  else
		    {
		      nfree (px);
		      socklist[i].inbuf = NULL;
		    }
		  if (s[strlen (s) - 1] == '\r')
		    s[strlen (s) - 1] = 0;
		  if (socklist[i].encstatus)
		    botlink_decrypt (i, s);
		  *len = strlen (s);
		  return socklist[i].sock;
		}
	    }
	  else
	    {
	      if (socklist[i].inbuflen <= (grab - 2))
		{
		  *len = socklist[i].inbuflen;
		  egg_memcpy (s, socklist[i].inbuf, socklist[i].inbuflen);
		  nfree (socklist[i].inbuf);
		  socklist[i].inbuf = NULL;
		  socklist[i].inbuflen = 0;
		}
	      else
		{
		  *len = grab - 2;
		  egg_memcpy (s, socklist[i].inbuf, *len);
		  egg_memcpy (socklist[i].inbuf, socklist[i].inbuf + *len,
			      *len);
		  socklist[i].inbuflen -= *len;
		  socklist[i].inbuf =
		    nrealloc (socklist[i].inbuf, socklist[i].inbuflen);
		}
	      return socklist[i].sock;
	    }
	}
      if (!(socklist[i].flags & SOCK_UNUSED)
	  && (socklist[i].flags & SOCK_EOFD))
	{
	  s[0] = 0;
	  *len = socklist[i].sock;
	  return -1;
	}
    }
  *len = 0;
  ret = sockread (xx, len);
  if (ret < 0)
    {
      s[0] = 0;
      return ret;
    }
  if (socklist[ret].flags & SOCK_CONNECT)
    {
      if (socklist[ret].flags & SOCK_STRONGCONN)
	{
	  socklist[ret].flags &= ~SOCK_STRONGCONN;
	  socklist[ret].inbuflen = *len;
	  socklist[ret].inbuf = (char *) nmalloc (*len + 1);
	  egg_memcpy (socklist[ret].inbuf, xx, *len);
	  socklist[ret].inbuf[*len] = 0;
	}
      socklist[ret].flags &= ~SOCK_CONNECT;
      s[0] = 0;
      return socklist[ret].sock;
    }
  if (socklist[ret].flags & SOCK_BINARY)
    {
      egg_memcpy (s, xx, *len);
      return socklist[ret].sock;
    }
  if ((socklist[ret].flags & SOCK_LISTEN)
      || (socklist[ret].flags & SOCK_PASS))
    return socklist[ret].sock;
  if (socklist[ret].flags & SOCK_BUFFER)
    {
      socklist[ret].inbuf =
	(char *) nrealloc (socklist[ret].inbuf,
			   socklist[ret].inbuflen + *len + 1);
      egg_memcpy (socklist[ret].inbuf + socklist[ret].inbuflen, xx, *len);
      socklist[ret].inbuflen += *len;
      socklist[ret].inbuf[socklist[ret].inbuflen] = 0;
      return -4;
    }
  if (socklist[ret].inbuf != NULL)
    {
      p = socklist[ret].inbuf;
      socklist[ret].inbuf = (char *) nmalloc (strlen (p) + strlen (xx) + 1);
      strcpy (socklist[ret].inbuf, p);
      strcat (socklist[ret].inbuf, xx);
      nfree (p);
      if (strlen (socklist[ret].inbuf) < grab)
	{
	  strcpy (xx, socklist[ret].inbuf);
	  nfree (socklist[ret].inbuf);
	  socklist[ret].inbuf = NULL;
	  socklist[ret].inbuflen = 0;
	}
      else
	{
	  p = socklist[ret].inbuf;
	  socklist[ret].inbuflen = strlen (p) - (grab - 2);
	  socklist[ret].inbuf = (char *) nmalloc (socklist[ret].inbuflen + 1);
	  strcpy (socklist[ret].inbuf, p + (grab - 2));
	  *(p + (grab - 2)) = 0;
	  strcpy (xx, p);
	  nfree (p);
    }}
  p = strchr (xx, '\n');
  if (p == NULL)
    p = strchr (xx, '\r');
  if (p != NULL)
    {
      *p = 0;
      strcpy (s, xx);
      strcpy (xx, p + 1);
      if (s[strlen (s) - 1] == '\r')
	s[strlen (s) - 1] = 0;
      data = 1;
    }
  else
    {
      s[0] = 0;
      if (strlen (xx) >= (grab - 2))
	{
	  strcpy (s, xx);
	  xx[0] = 0;
	  data = 1;
	}
    }
  if (socklist[ret].encstatus)
    botlink_decrypt (ret, s);
  *len = strlen (s);
  if (!xx[0])
    {
      if (data)
	return socklist[ret].sock;
      else
	return -3;
    }
  if (socklist[ret].inbuf != NULL)
    {
      p = socklist[ret].inbuf;
      socklist[ret].inbuflen = strlen (p) + strlen (xx);
      socklist[ret].inbuf = (char *) nmalloc (socklist[ret].inbuflen + 1);
      strcpy (socklist[ret].inbuf, xx);
      strcat (socklist[ret].inbuf, p);
      nfree (p);
    }
  else
    {
      socklist[ret].inbuflen = strlen (xx);
      socklist[ret].inbuf = (char *) nmalloc (socklist[ret].inbuflen + 1);
      strcpy (socklist[ret].inbuf, xx);
  } if (data)
    {
      return socklist[ret].sock;
    }
  else
    {
      return -3;
    }
}
void
tputs (register int z, char *s, unsigned int len)
{
  register int i, x, idx;
  char *p;
  static int inhere = 0;
  if (z < 0)
    return;
  if (((z == STDOUT) || (z == STDERR)) && (!backgrd || use_stderr))
    {
#ifdef EUSE_COLORPUTS
      colorputs (s);
      x = len;
#else
      write (z, s, len);
#endif
      return;
    }
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == z))
	{
	  for (idx = 0; idx < dcc_total; idx++)
	    {
	      if (dcc[idx].sock == z)
		{
		  if (dcc[idx].type)
		    {
		      if (dcc[idx].type->name)
			{
			  if (!strncmp (dcc[idx].type->name, "BOT", 3))
			    {
			      otraffic_bn_today += len;
			      break;
			    }
			  else if (!strcmp (dcc[idx].type->name, "SERVER"))
			    {
			      otraffic_irc_today += len;
			      break;
			    }
			  else if (!strncmp (dcc[idx].type->name, "CHAT", 4))
			    {
			      otraffic_dcc_today += len;
			      break;
			    }
			  else if (!strncmp (dcc[idx].type->name, "FILES", 5))
			    {
			      otraffic_filesys_today += len;
			      break;
			    }
			  else if (!strcmp (dcc[idx].type->name, "SEND"))
			    {
			      otraffic_trans_today += len;
			      break;
			    }
			  else if (!strncmp (dcc[idx].type->name, "GET", 3))
			    {
			      otraffic_trans_today += len;
			      break;
			    }
			  else
			    {
			      otraffic_unknown_today += len;
			      break;
			    }
			}
		    }
		}
	    }
	  if (socklist[i].encstatus)
	    {
	      if ((!s) || (!s[0]))
		{
		  s = botlink_encrypt (i, s);
		  len = strlen (s);
		}
	      s = botlink_encrypt (i, s);
	      len = strlen (s);
	    }
	  if (socklist[i].outbuf != NULL)
	    {
	      p =
		(char *) nrealloc (socklist[i].outbuf,
				   socklist[i].outbuflen + len);
	      egg_memcpy (p + socklist[i].outbuflen, s, len);
	      socklist[i].outbuf = p;
	      socklist[i].outbuflen += len;
	      if (socklist[i].encstatus)
		nfree (s);
	      return;
	    }
	  x = write (z, s, len);
	  if (x == (-1))
	    x = 0;
	  if (x < len)
	    {
	      socklist[i].outbuf = (char *) nmalloc (len - x);
	      egg_memcpy (socklist[i].outbuf, &s[x], len - x);
	      socklist[i].outbuflen = len - x;
	    }
	  if (socklist[i].encstatus)
	    nfree (s);
	  return;
	}
    }
  if (!inhere)
    {
      inhere = 1;
      putlog (LOG_MISC, "*", "!!! writing to nonexistent socket: %d", z);
      s[strlen (s) - 1] = 0;
      putlog (LOG_MISC, "*", "!-> '%s'", s);
      inhere = 0;
    }
}
void
dequeue_sockets ()
{
  int i, x;
  int z = 0, fds;
  fd_set wfds;
  struct timeval tv;
  fds = getdtablesize ();
#ifdef FD_SETSIZE
  if (fds > FD_SETSIZE)
    fds = FD_SETSIZE;
#endif
  FD_ZERO (&wfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].outbuf != NULL)
	{
	  FD_SET (socklist[i].sock, &wfds);
	  z = 1;
	}
    }
  if (!z)
    {
      return;
    }
#ifdef HPUX_HACKS
#ifndef HPUX10_HACKS
  select (fds, (int *) NULL, (int *) &wfds, (int *) NULL, &tv);
#else
  select (fds, NULL, &wfds, NULL, &tv);
#endif
#else
  select (fds, NULL, &wfds, NULL, &tv);
#endif
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].outbuf != NULL)
	  && (FD_ISSET (socklist[i].sock, &wfds)))
	{
	  errno = 0;
	  x =
	    write (socklist[i].sock, socklist[i].outbuf,
		   socklist[i].outbuflen);
	  if ((x < 0) && (errno != EAGAIN)
#ifdef EBADSLT
	      && (errno != EBADSLT)
#endif
#ifdef ENOTCONN
	      && (errno != ENOTCONN)
#endif
	    )
	    {
	      debug3 ("net: eof!(write) socket %d (%s,%d)", socklist[i].sock,
		      strerror (errno), errno);
	      socklist[i].flags |= SOCK_EOFD;
	    }
	  else if (x == socklist[i].outbuflen)
	    {
	      nfree (socklist[i].outbuf);
	      socklist[i].outbuf = NULL;
	      socklist[i].outbuflen = 0;
	    }
	  else if (x > 0)
	    {
	      char *p = socklist[i].outbuf;
	      socklist[i].outbuf =
		(char *) nmalloc (socklist[i].outbuflen - x);
	      egg_memcpy (socklist[i].outbuf, p + x,
			  socklist[i].outbuflen - x);
	      socklist[i].outbuflen -= x;
	      nfree (p);
	    }
	  else
	    {
	      debug3 ("dequeue_sockets(): errno = %d (%s) on %d", errno,
		      strerror (errno), socklist[i].sock);
	    }
	  if (!socklist[i].outbuf)
	    {
	      int idx = findanyidx (socklist[i].sock);
	      if (idx > 0 && dcc[idx].type && dcc[idx].type->outdone)
		dcc[idx].type->outdone (idx);
	    }
	}
    }
}
void
tell_netdebug (int idx)
{
  int i;
  char s[80];
  dprintf (idx, "Open sockets:");
  for (i = 0; i < MAXSOCKS; i++)
    {
      if (!(socklist[i].flags & SOCK_UNUSED))
	{
	  sprintf (s, " %d", socklist[i].sock);
	  if (socklist[i].flags & SOCK_BINARY)
	    strcat (s, " (binary)");
	  if (socklist[i].flags & SOCK_LISTEN)
	    strcat (s, " (listen)");
	  if (socklist[i].flags & SOCK_PASS)
	    strcat (s, " (passed on)");
	  if (socklist[i].flags & SOCK_CONNECT)
	    strcat (s, " (connecting)");
	  if (socklist[i].flags & SOCK_STRONGCONN)
	    strcat (s, " (strong)");
	  if (socklist[i].flags & SOCK_NONSOCK)
	    strcat (s, " (file)");
	  if (socklist[i].inbuf != NULL)
	    sprintf (&s[strlen (s)], " (inbuf: %04X)",
		     strlen (socklist[i].inbuf));
	  if (socklist[i].outbuf != NULL)
	    sprintf (&s[strlen (s)], " (outbuf: %06lX)",
		     socklist[i].outbuflen);
	  strcat (s, ",");
	  dprintf (idx, "%s", s);
	}
    }
  dprintf (idx, " done.\n");
}

int
sanitycheck_dcc (char *nick, char *from, char *ipaddy, char *port)
{
  char badaddress[16];
  IP ip = my_atoul (ipaddy);
  int prt = atoi (port);
  if (!dcc_sanitycheck)
    return 1;
#ifdef IPV6
  if (getprotocol (ipaddy) == AF_INET6)
    {
      return 1;
    }
#endif
  if (prt < 1)
    {
      putlog (LOG_MISC, "*",
	      "ALERT: (%s!%s) specified an impossible port of %u!", nick,
	      from, prt);
      return 0;
    }
  sprintf (badaddress, "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
	   (ip >> 8) & 0xff, ip & 0xff);
  if (ip < (1 << 24))
    {
      putlog (LOG_MISC, "*",
	      "ALERT: (%s!%s) specified an impossible IP of %s!", nick, from,
	      badaddress);
      return 0;
    }
  return 1;
}

int
hostsanitycheck_dcc (char *nick, char *from, IP ip, char *dnsname, char *prt)
{
  char hostn[256], badaddress[16];
  if (!dcc_sanitycheck)
    return 1;
  sprintf (badaddress, "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
	   (ip >> 8) & 0xff, ip & 0xff);
  strncpyz (hostn, extracthostname (from), sizeof hostn);
  if (!egg_strcasecmp (hostn, dnsname))
    {
      putlog (LOG_DEBUG, "*", "DNS information for submitted IP checks out.");
      return 1;
    }
  if (!strcmp (badaddress, dnsname))
    putlog (LOG_MISC, "*",
	    "ALERT: (%s!%s) sent a DCC request with bogus IP "
	    "information of %s port %s. %s does not resolve to %s!", nick,
	    from, badaddress, prt, from, badaddress);
  else
    return 1;
  return 0;
}

int
sock_has_data (int type, int sock)
{
  int ret = 0, i;
  for (i = 0; i < MAXSOCKS; i++)
    if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].sock == sock)
      break;
  if (i < MAXSOCKS)
    {
      switch (type)
	{
	case SOCK_DATA_OUTGOING:
	  ret = (socklist[i].outbuf != NULL);
	  break;
	case SOCK_DATA_INCOMING:
	  ret = (socklist[i].inbuf != NULL);
	  break;
	}
    }
  else
    debug1 ("sock_has_data: could not find socket #%d, returning false.",
	    sock);
  return ret;
}

int
flush_inbuf (int idx)
{
  int i, len;
  char *inbuf;
  Assert ((idx >= 0) && (idx < dcc_total));
  for (i = 0; i < MAXSOCKS; i++)
    {
      if ((dcc[idx].sock == socklist[i].sock)
	  && !(socklist[i].flags & SOCK_UNUSED))
	{
	  len = socklist[i].inbuflen;
	  if ((len > 0) && socklist[i].inbuf)
	    {
	      if (dcc[idx].type && dcc[idx].type->activity)
		{
		  inbuf = socklist[i].inbuf;
		  socklist[i].inbuf = NULL;
		  dcc[idx].type->activity (idx, inbuf, len);
		  nfree (inbuf);
		  return len;
		}
	      else
		return -2;
	    }
	  else
	    return 0;
	}
    }
  return -1;
}
