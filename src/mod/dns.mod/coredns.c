#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#define BASH_SIZE 8192
#define HOSTNAMELEN 255
#define RES_RETRYDELAY 3
#define RES_MAXSENDS 4
#define RES_FAILEDDELAY 600
#define RES_MAX_TTL 86400
#define RES_ERR "DNS Resolver error: "
#define RES_MSG "DNS Resolver: "
#define RES_WRN "DNS Resolver warning: "
#define MAX_PACKETSIZE (PACKETSZ)
#define MAX_DOMAINLEN (MAXDNAME)
#define nonull(s) (s) ? s : nullstring
#define BASH_MODULO(x) ((x) & 8191)
#ifdef DEBUG_DNS
#define RESPONSECODES_COUNT 6
static char *responsecodes[RESPONSECODES_COUNT + 1] =
  { "no error", "format error in query", "server failure",
"queried domain name does not exist", "requested query type not implemented", "refused by name server", "unknown error", };
#endif
#ifdef DEBUG_DNS
#define RESOURCETYPES_COUNT 17
static const char *resourcetypes[RESOURCETYPES_COUNT + 1] =
  { "unknown type", "A: host address", "NS: authoritative name server",
"MD: mail destination (OBSOLETE)", "MF: mail forwarder (OBSOLETE)", "CNAME: name alias", "SOA: authority record",
"MB: mailbox domain name (EXPERIMENTAL)", "MG: mail group member (EXPERIMENTAL)", "MR: mail rename domain name (EXPERIMENTAL)",
"NULL: NULL RR (EXPERIMENTAL)", "WKS: well known service description", "PTR: domain name pointer", "HINFO: host information",
"MINFO: mailbox or mail list information", "MX: mail exchange", "TXT: text string", "unknown type", };
#endif
#ifdef DEBUG_DNS
#define CLASSTYPES_COUNT 5
static const char *classtypes[CLASSTYPES_COUNT + 1] =
  { "unknown class", "IN: the Internet", "CS: CSNET (OBSOLETE)", "CH: CHAOS",
"HS: Hesoid [Dyer 87]", "unknown class" };
#endif
typedef struct
{
  u_16bit_t id;
  u_8bit_t databyte_a;
  u_8bit_t databyte_b;
  u_16bit_t qdcount;
  u_16bit_t ancount;
  u_16bit_t nscount;
  u_16bit_t arcount;
} packetheader;
#ifndef HFIXEDSZ
#define HFIXEDSZ (sizeof(packetheader))
#endif
#define getheader_rd(x) (x->databyte_a & 1)
#define getheader_tc(x) ((x->databyte_a >> 1) & 1)
#define getheader_aa(x) ((x->databyte_a >> 2) & 1)
#define getheader_opcode(x) ((x->databyte_a >> 3) & 15)
#define getheader_qr(x) (x->databyte_a >> 7)
#define getheader_rcode(x) (x->databyte_b & 15)
#define getheader_pr(x) ((x->databyte_b >> 6) & 1)
#define getheader_ra(x) (x->databyte_b >> 7)
#define sucknetword(x) ((x)+=2,((u_16bit_t) (((x)[-2] << 8) | ((x)[-1] << 0))))
#define sucknetshort(x) ((x)+=2,((short) (((x)[-2] << 8) | ((x)[-1] << 0))))
#define sucknetdword(x) ((x)+=4,((dword) (((x)[-4] << 24) | ((x)[-3] << 16) | ((x)[-2] << 8) | ((x)[-1] << 0))))
#define sucknetlong(x) ((x)+=4,((long) (((x)[-4] << 24) | ((x)[-3] << 16) | ((x)[-2] << 8) | ((x)[-1] << 0))))
static u_32bit_t resrecvbuf[(MAX_PACKETSIZE + 7) >> 2];
static struct resolve *idbash[BASH_SIZE];
static struct resolve *ipbash[BASH_SIZE];
static struct resolve *hostbash[BASH_SIZE];
static struct resolve *expireresolves = NULL;
static IP localhost;
static long idseed = 0xdeadbeef;
static long aseed;
static int resfd;
static char tempstring[512];
static char namestring[1024 + 1];
static char stackstring[1024 + 1];
#ifdef DEBUG_DNS
static char sendstring[1024 + 1];
#endif
static const char nullstring[] = "";
#ifdef DEBUG_DNS
static char *
strtdiff (char *d, long signeddiff)
{
  u_32bit_t diff;
  u_32bit_t seconds, minutes, hours;
  long day;
  if ((diff = labs (signeddiff)))
    {
      seconds = diff % 60;
      diff /= 60;
      minutes = diff % 60;
      diff /= 60;
      hours = diff % 24;
      day = signeddiff / (60 * 60 * 24);
      if (day)
	sprintf (d, "%lid", day);
      else
	*d = '\0';
      if (hours)
	sprintf (d + strlen (d), "%uh", hours);
      if (minutes)
	sprintf (d + strlen (d), "%um", minutes);
      if (seconds)
	sprintf (d + strlen (d), "%us", seconds);
    }
  else
    sprintf (d, "0s");
  return d;
}
#endif
static struct resolve *
allocresolve ()
{
  struct resolve *rp;
  rp = (struct resolve *) nmalloc (sizeof (struct resolve));
  egg_bzero (rp, sizeof (struct resolve));
  return rp;
}
inline static u_32bit_t
getidbash (u_16bit_t id)
{
  return (u_32bit_t) BASH_MODULO (id);
}
inline static u_32bit_t
getipbash (IP ip)
{
  return (u_32bit_t) BASH_MODULO (ip);
}
static u_32bit_t
gethostbash (char *host)
{
  u_32bit_t bashvalue = 0;
  for (; *host; host++)
    {
      bashvalue ^= *host;
      bashvalue += (*host >> 1) + (bashvalue >> 1);
    }
  return BASH_MODULO (bashvalue);
}
static void
linkresolveid (struct resolve *addrp)
{
  struct resolve *rp;
  u_32bit_t bashnum;
  bashnum = getidbash (addrp->id);
  rp = idbash[bashnum];
  if (rp)
    {
      while ((rp->nextid) && (addrp->id > rp->nextid->id))
	rp = rp->nextid;
      while ((rp->previousid) && (addrp->id < rp->previousid->id))
	rp = rp->previousid;
      if (rp->id < addrp->id)
	{
	  addrp->previousid = rp;
	  addrp->nextid = rp->nextid;
	  if (rp->nextid)
	    rp->nextid->previousid = addrp;
	  rp->nextid = addrp;
	}
      else if (rp->id > addrp->id)
	{
	  addrp->previousid = rp->previousid;
	  addrp->nextid = rp;
	  if (rp->previousid)
	    rp->previousid->nextid = addrp;
	  rp->previousid = addrp;
	}
      else
	return;
    }
  else
    addrp->nextid = addrp->previousid = NULL;
  idbash[bashnum] = addrp;
}
static void
unlinkresolveid (struct resolve *rp)
{
  u_32bit_t bashnum;
  bashnum = getidbash (rp->id);
  if (idbash[bashnum] == rp)
    {
      if (rp->previousid)
	idbash[bashnum] = rp->previousid;
      else
	idbash[bashnum] = rp->nextid;
    }
  if (rp->nextid)
    rp->nextid->previousid = rp->previousid;
  if (rp->previousid)
    rp->previousid->nextid = rp->nextid;
}
static void
linkresolvehost (struct resolve *addrp)
{
  struct resolve *rp;
  u_32bit_t bashnum;
  int ret;
  bashnum = gethostbash (addrp->hostn);
  rp = hostbash[bashnum];
  if (rp)
    {
      while ((rp->nexthost)
	     && (egg_strcasecmp (addrp->hostn, rp->nexthost->hostn) < 0))
	rp = rp->nexthost;
      while ((rp->previoushost)
	     && (egg_strcasecmp (addrp->hostn, rp->previoushost->hostn) > 0))
	rp = rp->previoushost;
      ret = egg_strcasecmp (addrp->hostn, rp->hostn);
      if (ret < 0)
	{
	  addrp->previoushost = rp;
	  addrp->nexthost = rp->nexthost;
	  if (rp->nexthost)
	    rp->nexthost->previoushost = addrp;
	  rp->nexthost = addrp;
	}
      else if (ret > 0)
	{
	  addrp->previoushost = rp->previoushost;
	  addrp->nexthost = rp;
	  if (rp->previoushost)
	    rp->previoushost->nexthost = addrp;
	  rp->previoushost = addrp;
	}
      else
	return;
    }
  else
    addrp->nexthost = addrp->previoushost = NULL;
  hostbash[bashnum] = addrp;
}
static void
unlinkresolvehost (struct resolve *rp)
{
  u_32bit_t bashnum;
  bashnum = gethostbash (rp->hostn);
  if (hostbash[bashnum] == rp)
    {
      if (rp->previoushost)
	hostbash[bashnum] = rp->previoushost;
      else
	hostbash[bashnum] = rp->nexthost;
    }
  if (rp->nexthost)
    rp->nexthost->previoushost = rp->previoushost;
  if (rp->previoushost)
    rp->previoushost->nexthost = rp->nexthost;
  nfree (rp->hostn);
}
static void
linkresolveip (struct resolve *addrp)
{
  struct resolve *rp;
  u_32bit_t bashnum;
  bashnum = getipbash (addrp->ip);
  rp = ipbash[bashnum];
  if (rp)
    {
      while ((rp->nextip) && (addrp->ip > rp->nextip->ip))
	rp = rp->nextip;
      while ((rp->previousip) && (addrp->ip < rp->previousip->ip))
	rp = rp->previousip;
      if (rp->ip < addrp->ip)
	{
	  addrp->previousip = rp;
	  addrp->nextip = rp->nextip;
	  if (rp->nextip)
	    rp->nextip->previousip = addrp;
	  rp->nextip = addrp;
	}
      else if (rp->ip > addrp->ip)
	{
	  addrp->previousip = rp->previousip;
	  addrp->nextip = rp;
	  if (rp->previousip)
	    rp->previousip->nextip = addrp;
	  rp->previousip = addrp;
	}
      else
	return;
    }
  else
    addrp->nextip = addrp->previousip = NULL;
  ipbash[bashnum] = addrp;
}
static void
unlinkresolveip (struct resolve *rp)
{
  u_32bit_t bashnum;
  bashnum = getipbash (rp->ip);
  if (ipbash[bashnum] == rp)
    {
      if (rp->previousip)
	ipbash[bashnum] = rp->previousip;
      else
	ipbash[bashnum] = rp->nextip;
    }
  if (rp->nextip)
    rp->nextip->previousip = rp->previousip;
  if (rp->previousip)
    rp->previousip->nextip = rp->nextip;
}
static void
linkresolve (struct resolve *rp)
{
  struct resolve *irp;
  if (expireresolves)
    {
      irp = expireresolves;
      while ((irp->next) && (rp->expiretime >= irp->expiretime))
	irp = irp->next;
      if (rp->expiretime >= irp->expiretime)
	{
	  rp->next = NULL;
	  rp->previous = irp;
	  irp->next = rp;
	}
      else
	{
	  rp->previous = irp->previous;
	  rp->next = irp;
	  if (irp->previous)
	    irp->previous->next = rp;
	  else
	    expireresolves = rp;
	  irp->previous = rp;
	}
    }
  else
    {
      rp->next = NULL;
      rp->previous = NULL;
      expireresolves = rp;
    }
}
static void
untieresolve (struct resolve *rp)
{
  if (rp->previous)
    rp->previous->next = rp->next;
  else
    expireresolves = rp->next;
  if (rp->next)
    rp->next->previous = rp->previous;
}
static void
unlinkresolve (struct resolve *rp)
{
  untieresolve (rp);
  unlinkresolveid (rp);
  unlinkresolveip (rp);
  if (rp->hostn)
    unlinkresolvehost (rp);
  nfree (rp);
}
static struct resolve *
findid (u_16bit_t id)
{
  struct resolve *rp;
  int bashnum;
  bashnum = getidbash (id);
  rp = idbash[bashnum];
  if (rp)
    {
      while ((rp->nextid) && (id >= rp->nextid->id))
	rp = rp->nextid;
      while ((rp->previousid) && (id <= rp->previousid->id))
	rp = rp->previousid;
      if (id == rp->id)
	{
	  idbash[bashnum] = rp;
	  return rp;
	}
      else
	return NULL;
    }
  return rp;
}
static struct resolve *
findhost (char *hostn)
{
  struct resolve *rp;
  int bashnum;
  bashnum = gethostbash (hostn);
  rp = hostbash[bashnum];
  if (rp)
    {
      while ((rp->nexthost)
	     && (egg_strcasecmp (hostn, rp->nexthost->hostn) >= 0))
	rp = rp->nexthost;
      while ((rp->previoushost)
	     && (egg_strcasecmp (hostn, rp->previoushost->hostn) <= 0))
	rp = rp->previoushost;
      if (egg_strcasecmp (hostn, rp->hostn))
	return NULL;
      else
	{
	  hostbash[bashnum] = rp;
	  return rp;
	}
    }
  return rp;
}
static struct resolve *
findip (IP ip)
{
  struct resolve *rp;
  u_32bit_t bashnum;
  bashnum = getipbash (ip);
  rp = ipbash[bashnum];
  if (rp)
    {
      while ((rp->nextip) && (ip >= rp->nextip->ip))
	rp = rp->nextip;
      while ((rp->previousip) && (ip <= rp->previousip->ip))
	rp = rp->previousip;
      if (ip == rp->ip)
	{
	  ipbash[bashnum] = rp;
	  return rp;
	}
      else
	return NULL;
    }
  return rp;
}
static void
dorequest (char *s, int type, u_16bit_t id)
{
  packetheader *hp;
  int r, i;
  u_8bit_t buf[(MAX_PACKETSIZE / sizeof (char)) + 1];
  r = res_mkquery (QUERY, s, C_IN, type, NULL, 0, NULL, buf, MAX_PACKETSIZE);
  if (r == -1)
    {
      ddebug0 (RES_ERR "Query too large.");
      return;
    }
  hp = (packetheader *) buf;
  hp->id = id;
  for (i = 0; i < _res.nscount; i++)
    (void) sendto (resfd, buf, r, 0, (struct sockaddr *) &_res.nsaddr_list[i],
		   sizeof (struct sockaddr));
} static void
resendrequest (struct resolve *rp, int type)
{
  rp->sends++;
  rp->expiretime = now + (RES_RETRYDELAY * rp->sends);
  linkresolve (rp);
  if (type == T_A)
    {
      dorequest (rp->hostn, type, rp->id);
      ddebug1 (RES_MSG "Sent domain lookup request for \"%s\".", rp->hostn);
    }
  else if (type == T_PTR)
    {
      sprintf (tempstring, "%u.%u.%u.%u.in-addr.arpa",
	       ((u_8bit_t *) & rp->ip)[3], ((u_8bit_t *) & rp->ip)[2],
	       ((u_8bit_t *) & rp->ip)[1], ((u_8bit_t *) & rp->ip)[0]);
      dorequest (tempstring, type, rp->id);
      ddebug1 (RES_MSG "Sent domain lookup request for \"%s\".",
	       iptostr (rp->ip));
    }
}
static void
sendrequest (struct resolve *rp, int type)
{
  do
    {
      idseed =
	(((idseed + idseed) | (long) time (NULL)) + idseed -
	 0x54bad4a) ^ aseed;
      aseed ^= idseed;
      rp->id = (u_16bit_t) idseed;
    }
  while (findid (rp->id));
  linkresolveid (rp);
  resendrequest (rp, type);
}
static void
failrp (struct resolve *rp, int type)
{
  if (rp->state == STATE_FINISHED)
    return;
  rp->expiretime = now + RES_FAILEDDELAY;
  rp->state = STATE_FAILED;
  untieresolve (rp);
  linkresolve (rp);
  ddebug0 (RES_MSG "Lookup failed.");
  dns_event_failure (rp, type);
}
static void
passrp (struct resolve *rp, long ttl, int type)
{
  rp->state = STATE_FINISHED;
  if (ttl < RES_MAX_TTL)
    rp->expiretime = now + (time_t) ttl;
  else
    rp->expiretime = now + RES_MAX_TTL;
  untieresolve (rp);
  linkresolve (rp);
  ddebug1 (RES_MSG "Lookup successful: %s", rp->hostn);
  dns_event_success (rp, type);
}
static void
parserespacket (u_8bit_t * s, int l)
{
  struct resolve *rp;
  packetheader *hp;
  u_8bit_t *eob;
  u_8bit_t *c;
  long ttl;
  int r, usefulanswer;
  u_16bit_t rr, datatype, class, qdatatype, qclass;
  u_8bit_t rdatalength;
  if (l < sizeof (packetheader))
    {
      debug0 (RES_ERR "Packet smaller than standard header size.");
      return;
    }
  if (l == sizeof (packetheader))
    {
      debug0 (RES_ERR "Packet has empty body.");
      return;
    }
  hp = (packetheader *) s;
  rp = findid (hp->id);
  if (!rp)
    return;
  if ((rp->state == STATE_FINISHED) || (rp->state == STATE_FAILED))
    return;
  hp->qdcount = ntohs (hp->qdcount);
  hp->ancount = ntohs (hp->ancount);
  hp->nscount = ntohs (hp->nscount);
  hp->arcount = ntohs (hp->arcount);
  if (getheader_tc (hp))
    {
      ddebug0 (RES_ERR "Nameserver packet truncated.");
      return;
    }
  if (!getheader_qr (hp))
    {
      ddebug0 (RES_ERR
	       "Query packet received on nameserver communication socket.");
      return;
    }
  if (getheader_opcode (hp))
    {
      ddebug0 (RES_ERR "Invalid opcode in response packet.");
      return;
    }
  eob = s + l;
  c = s + HFIXEDSZ;
  switch (getheader_rcode (hp))
    {
    case NOERROR:
      if (hp->ancount)
	{
	  ddebug4 (RES_MSG
		   "Received nameserver reply. (qd:%u an:%u ns:%u ar:%u)",
		   hp->qdcount, hp->ancount, hp->nscount, hp->arcount);
	  if (hp->qdcount != 1)
	    {
	      ddebug0 (RES_ERR "Reply does not contain one query.");
	      return;
	    }
	  if (c > eob)
	    {
	      ddebug0 (RES_ERR "Reply too short.");
	      return;
	    }
	  switch (rp->state)
	    {
	    case STATE_PTRREQ:
	      sprintf (stackstring, "%u.%u.%u.%u.in-addr.arpa",
		       ((u_8bit_t *) & rp->ip)[3], ((u_8bit_t *) & rp->ip)[2],
		       ((u_8bit_t *) & rp->ip)[1],
		       ((u_8bit_t *) & rp->ip)[0]);
	      break;
	    case STATE_AREQ:
	      strncpy (stackstring, rp->hostn, 1024);
	    }
	  *namestring = '\0';
	  r = dn_expand (s, s + l, c, namestring, MAXDNAME);
	  if (r == -1)
	    {
	      ddebug0 (RES_ERR
		       "dn_expand() failed while expanding query domain.");
	      return;
	    }
	  namestring[strlen (stackstring)] = '\0';
	  if (egg_strcasecmp (stackstring, namestring))
	    {
	      ddebug2 (RES_MSG
		       "Unknown query packet dropped. (\"%s\" does not match \"%s\")",
		       stackstring, namestring);
	      return;
	    }
	  ddebug1 (RES_MSG "Queried domain name: \"%s\"", namestring);
	  c += r;
	  if (c + 4 > eob)
	    {
	      ddebug0 (RES_ERR "Query resource record truncated.");
	      return;
	    }
	  qdatatype = sucknetword (c);
	  qclass = sucknetword (c);
	  if (qclass != C_IN)
	    {
	      ddebug2 (RES_ERR "Received unsupported query class: %u (%s)",
		       qclass,
		       qclass <
		       CLASSTYPES_COUNT ? classtypes[qclass] :
		       classtypes[CLASSTYPES_COUNT]);
	    }
	  switch (qdatatype)
	    {
	    case T_PTR:
	      if (!IS_PTR (rp))
		{
		  ddebug0 (RES_WRN
			   "Ignoring response with unexpected query type \"PTR\".");
		  return;
		}
	      break;
	    case T_A:
	      if (!IS_A (rp))
		{
		  ddebug0 (RES_WRN
			   "Ignoring response with unexpected query type \"PTR\".");
		  return;
		}
	      break;
	    default:
	      ddebug2 (RES_ERR "Received unimplemented query type: %u (%s)",
		       qdatatype,
		       qdatatype <
		       RESOURCETYPES_COUNT ? resourcetypes[qdatatype] :
		       resourcetypes[RESOURCETYPES_COUNT]);
	    }
	  for (rr = hp->ancount + hp->nscount + hp->arcount; rr; rr--)
	    {
	      if (c > eob)
		{
		  ddebug0 (RES_ERR
			   "Packet does not contain all specified resouce records.");
		  return;
		}
	      *namestring = '\0';
	      r = dn_expand (s, s + l, c, namestring, MAXDNAME);
	      if (r == -1)
		{
		  ddebug0 (RES_ERR
			   "dn_expand() failed while expanding answer domain.");
		  return;
		}
	      namestring[strlen (stackstring)] = '\0';
	      if (egg_strcasecmp (stackstring, namestring))
		usefulanswer = 0;
	      else
		usefulanswer = 1;
	      ddebug1 (RES_MSG "answered domain query: \"%s\"", namestring);
	      c += r;
	      if (c + 10 > eob)
		{
		  ddebug0 (RES_ERR "Resource record truncated.");
		  return;
		}
	      datatype = sucknetword (c);
	      class = sucknetword (c);
	      ttl = sucknetlong (c);
	      rdatalength = sucknetword (c);
	      if (class != qclass)
		{
		  ddebug2 (RES_MSG "query class: %u (%s)", qclass,
			   qclass <
			   CLASSTYPES_COUNT ? classtypes[qclass] :
			   classtypes[CLASSTYPES_COUNT]);
		  ddebug2 (RES_MSG "rr class: %u (%s)", class,
			   class <
			   CLASSTYPES_COUNT ? classtypes[class] :
			   classtypes[CLASSTYPES_COUNT]);
		  ddebug0 (RES_ERR
			   "Answered class does not match queried class.");
		  return;
		}
	      if (!rdatalength)
		{
		  ddebug0 (RES_ERR "Zero size rdata.");
		  return;
		}
	      if (c + rdatalength > eob)
		{
		  ddebug0 (RES_ERR
			   "Specified rdata length exceeds packet size.");
		  return;
		}
	      if (datatype == qdatatype)
		{
		  ddebug1 (RES_MSG "TTL: %s", strtdiff (sendstring, ttl));
		  ddebug1 (RES_MSG "TYPE: %s",
			   datatype <
			   RESOURCETYPES_COUNT ? resourcetypes[datatype] :
			   resourcetypes[RESOURCETYPES_COUNT]);
		  if (usefulanswer)
		    switch (datatype)
		      {
		      case T_A:
			if (rdatalength != 4)
			  {
			    ddebug1 (RES_ERR
				     "Unsupported rdata format for \"A\" type. (%u bytes)",
				     rdatalength);
			    return;
			  }
			my_memcpy (&rp->ip, (IP *) c, sizeof (IP));
			linkresolveip (rp);
			passrp (rp, ttl, T_A);
			return;
		      case T_PTR:
			*namestring = '\0';
			r = dn_expand (s, s + l, c, namestring, MAXDNAME);
			if (r == -1)
			  {
			    ddebug0 (RES_ERR
				     "dn_expand() failed while expanding domain in rdata.");
			    return;
			  }
			ddebug1 (RES_MSG "Answered domain: \"%s\"",
				 namestring);
			if (r > HOSTNAMELEN)
			  {
			    ddebug0 (RES_ERR "Domain name too long.");
			    failrp (rp, T_PTR);
			    return;
			  }
			if (!rp->hostn)
			  {
			    rp->hostn =
			      (char *) nmalloc (strlen (namestring) + 1);
			    strcpy (rp->hostn, namestring);
			    linkresolvehost (rp);
			    passrp (rp, ttl, T_PTR);
			    return;
			  }
			break;
		      default:
			ddebug2 (RES_ERR
				 "Received unimplemented data type: %u (%s)",
				 datatype,
				 datatype <
				 RESOURCETYPES_COUNT ? resourcetypes[datatype]
				 : resourcetypes[RESOURCETYPES_COUNT]);
		      }
		}
	      else if (datatype == T_CNAME)
		{
		  *namestring = '\0';
		  r = dn_expand (s, s + l, c, namestring, MAXDNAME);
		  if (r == -1)
		    {
		      ddebug0 (RES_ERR
			       "dn_expand() failed while expanding domain in rdata.");
		      return;
		    }
		  ddebug1 (RES_MSG "answered domain is CNAME for: %s",
			   namestring);
		  strncpy (stackstring, namestring, 1024);
		}
	      else
		{
		  ddebug2 (RES_MSG "Ignoring resource type %u. (%s)",
			   datatype,
			   datatype <
			   RESOURCETYPES_COUNT ? resourcetypes[datatype] :
			   resourcetypes[RESOURCETYPES_COUNT]);
		}
	      c += rdatalength;
	    }
	}
      else
	ddebug0 (RES_ERR "No error returned but no answers given.");
      break;
    case NXDOMAIN:
      ddebug0 (RES_MSG "Host not found.");
      switch (rp->state)
	{
	case STATE_PTRREQ:
	  failrp (rp, T_PTR);
	  break;
	case STATE_AREQ:
	  failrp (rp, T_A);
	  break;
	default:
	  failrp (rp, 0);
	  break;
	}
      break;
    default:
      ddebug2 (RES_MSG "Received error response %u. (%s)",
	       getheader_rcode (hp),
	       getheader_rcode (hp) <
	       RESPONSECODES_COUNT ? responsecodes[getheader_rcode (hp)] :
	       responsecodes[RESPONSECODES_COUNT]);
    }
}
static void
dns_ack (void)
{
  struct sockaddr_in from;
  unsigned int fromlen = sizeof (struct sockaddr_in);
  int r, i;
  r =
    recvfrom (resfd, (u_8bit_t *) resrecvbuf, MAX_PACKETSIZE, 0,
	      (struct sockaddr *) &from, &fromlen);
  if (r <= 0)
    {
      ddebug1 (RES_MSG "Socket error: %s", strerror (errno));
      return;
    }
  if (from.sin_addr.s_addr == localhost)
    {
      for (i = 0; i < _res.nscount; i++)
	if ((_res.nsaddr_list[i].sin_addr.s_addr == from.sin_addr.s_addr)
	    || (!_res.nsaddr_list[i].sin_addr.s_addr))
	  break;
    }
  else
    {
      for (i = 0; i < _res.nscount; i++)
	if (_res.nsaddr_list[i].sin_addr.s_addr == from.sin_addr.s_addr)
	  break;
    }
  if (i == _res.nscount)
    {
      ddebug1 (RES_ERR "Received reply from unknown source: %s",
	       iptostr (from.sin_addr.s_addr));
    }
  else
    parserespacket ((u_8bit_t *) resrecvbuf, r);
}
static void
dns_check_expires (void)
{
  struct resolve *rp, *nextrp;
  for (rp = expireresolves; (rp) && (now >= rp->expiretime); rp = nextrp)
    {
      nextrp = rp->next;
      untieresolve (rp);
      switch (rp->state)
	{
	case STATE_FINISHED:
	case STATE_FAILED:
	  ddebug4 (RES_MSG
		   "Cache record for \"%s\" (%s) has expired. (state: %u)  Marked for expire at: %ld.",
		   nonull (rp->hostn), iptostr (rp->ip), rp->state,
		   rp->expiretime);
	  unlinkresolve (rp);
	  break;
	case STATE_PTRREQ:
	  if (rp->sends <= RES_MAXSENDS)
	    {
	      ddebug1 (RES_MSG "Resend #%d for \"PTR\" query...",
		       rp->sends - 1);
	      resendrequest (rp, T_PTR);
	    }
	  else
	    {
	      ddebug0 (RES_MSG "\"PTR\" query timed out.");
	      failrp (rp, T_PTR);
	    }
	  break;
	case STATE_AREQ:
	  if (rp->sends <= RES_MAXSENDS)
	    {
	      ddebug1 (RES_MSG "Resend #%d for \"A\" query...",
		       rp->sends - 1);
	      resendrequest (rp, T_A);
	    }
	  else
	    {
	      ddebug0 (RES_MSG "\"A\" query timed out.");
	      failrp (rp, T_A);
	    }
	  break;
	default:
	  ddebug1 (RES_WRN "Unknown request state %d. Request expired.",
		   rp->state);
	  failrp (rp, 0);
	}
    }
}
static void
dns_lookup (IP ip)
{
  struct resolve *rp;
  ip = htonl (ip);
  if ((rp = findip (ip)))
    {
      if (rp->state == STATE_FINISHED || rp->state == STATE_FAILED)
	{
	  if (rp->state == STATE_FINISHED && rp->hostn)
	    {
	      ddebug2 (RES_MSG "Used cached record: %s == \"%s\".",
		       iptostr (ip), rp->hostn);
	      dns_event_success (rp, T_PTR);
	    }
	  else
	    {
	      ddebug1 (RES_MSG "Used failed record: %s == ???", iptostr (ip));
	      dns_event_failure (rp, T_PTR);
	    }
	}
      return;
    }
  ddebug0 (RES_MSG "Creating new record");
  rp = allocresolve ();
  rp->state = STATE_PTRREQ;
  rp->sends = 1;
  rp->ip = ip;
  linkresolveip (rp);
  sendrequest (rp, T_PTR);
}
static void
dns_forward (char *hostn)
{
  struct resolve *rp;
  struct in_addr inaddr;
  if (egg_inet_aton (hostn, &inaddr))
    {
      call_ipbyhost (hostn, ntohl (inaddr.s_addr), 1);
      return;
    }
  if ((rp = findhost (hostn)))
    {
      if (rp->state == STATE_FINISHED || rp->state == STATE_FAILED)
	{
	  if (rp->state == STATE_FINISHED && rp->ip)
	    {
	      ddebug2 (RES_MSG "Used cached record: %s == \"%s\".", hostn,
		       iptostr (rp->ip));
	      dns_event_success (rp, T_A);
	    }
	  else
	    {
	      ddebug1 (RES_MSG "Used failed record: %s == ???", hostn);
	      dns_event_failure (rp, T_A);
	    }
	}
      return;
    }
  ddebug0 (RES_MSG "Creating new record");
  rp = allocresolve ();
  rp->state = STATE_AREQ;
  rp->sends = 1;
  rp->hostn = (char *) nmalloc (strlen (hostn) + 1);
  strcpy (rp->hostn, hostn);
  linkresolvehost (rp);
  sendrequest (rp, T_A);
} static int
init_dns_network (void)
{
  int option;
  struct in_addr inaddr;
  resfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (resfd == -1)
    {
      putlog (LOG_MISC, "*",
	      "Unable to allocate socket for nameserver communication: %s",
	      strerror (errno));
      return 0;
    }
  (void) allocsock (resfd, SOCK_PASS);
  option = 1;
  if (setsockopt
      (resfd, SOL_SOCKET, SO_BROADCAST, (char *) &option, sizeof (option)))
    {
      putlog (LOG_MISC, "*",
	      "Unable to setsockopt() on nameserver communication socket: %s",
	      strerror (errno));
      killsock (resfd);
      return 0;
    }
  egg_inet_aton ("127.0.0.1", &inaddr);
  localhost = inaddr.s_addr;
  return 1;
}
static int
init_dns_core (void)
{
  int i;
  res_init ();
  if (!_res.nscount)
    {
      putlog (LOG_MISC, "*", "No nameservers defined.");
      return 0;
    }
  _res.options |= RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  for (i = 0; i < _res.nscount; i++)
    _res.nsaddr_list[i].sin_family = AF_INET;
  if (!init_dns_network ())
    return 0;
  aseed = time (NULL) ^ (time (NULL) << 3) ^ (u_32bit_t) getpid ();
  for (i = 0; i < BASH_SIZE; i++)
    {
      idbash[i] = NULL;
      ipbash[i] = NULL;
      hostbash[i] = NULL;
    }
  expireresolves = NULL;
  return 1;
}
