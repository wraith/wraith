/*
 * dns.c -- handles:
 *   DNS resolve calls and events
 *   provides the code used by the bot if the DNS module is not loaded
 *   DNS Tcl commands
 *
 */

#include "main.h"
#include <netdb.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dns.h"

extern struct dcc_t	*dcc;
extern int		 dcc_total;
extern int		 resolve_timeout;
extern time_t		 now;
extern jmp_buf		 alarmret;
extern Tcl_Interp	*interp;

devent_t	*dns_events = NULL;


/*
 *   DCC functions
 */

void dcc_dnswait(int idx, char *buf, int len)
{
  /* Ignore anything now. */
}

void eof_dcc_dnswait(int idx)
{
  putlog(LOG_MISC, "*", "Lost connection while resolving hostname [%s/%d]",
	 iptostr(htonl(dcc[idx].addr)), dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void display_dcc_dnswait(int idx, char *buf)
{
  sprintf(buf, "dns   waited %lus", now - dcc[idx].timeval);
}

static void kill_dcc_dnswait(int idx, void *x)
{
  register struct dns_info *p = (struct dns_info *) x;

  if (p) {
    if (p->host)
      free(p->host);
    if (p->cbuf)
      free(p->cbuf);
    free(p);
  }
}

struct dcc_table DCC_DNSWAIT =
{
  "DNSWAIT",
  DCT_VALIDIDX,
  eof_dcc_dnswait,
  dcc_dnswait,
  0,
  0,
  display_dcc_dnswait,
  kill_dcc_dnswait,
  0
};


/*
 *   DCC events
 */

/* Walk through every dcc entry and look for waiting DNS requests
 * of RES_HOSTBYIP for our IP address.
 */
static void dns_dcchostbyip(IP ip, char *hostn, int ok, void *other)
{
  int idx;

  for (idx = 0; idx < dcc_total; idx++) {
    if ((dcc[idx].type == &DCC_DNSWAIT) &&
        (dcc[idx].u.dns->dns_type == RES_HOSTBYIP) &&
        (dcc[idx].u.dns->ip == ip)) {
      if (dcc[idx].u.dns->host)
        free(dcc[idx].u.dns->host);
      dcc[idx].u.dns->host = get_data_ptr(strlen(hostn) + 1);
      strcpy(dcc[idx].u.dns->host, hostn);
      if (ok)
        dcc[idx].u.dns->dns_success(idx);
      else
        dcc[idx].u.dns->dns_failure(idx);
    }
  }
}

/* Walk through every dcc entry and look for waiting DNS requests
 * of RES_IPBYHOST for our hostname.
 */
static void dns_dccipbyhost(IP ip, char *hostn, int ok, void *other)
{
  int idx;

  for (idx = 0; idx < dcc_total; idx++) {
    if ((dcc[idx].type == &DCC_DNSWAIT) &&
        (dcc[idx].u.dns->dns_type == RES_IPBYHOST) &&
        !egg_strcasecmp(dcc[idx].u.dns->host, hostn)) {
      dcc[idx].u.dns->ip = ip;
      if (ok)
        dcc[idx].u.dns->dns_success(idx);
      else
        dcc[idx].u.dns->dns_failure(idx);
    }
  }
}

devent_type DNS_DCCEVENT_HOSTBYIP = {
  "DCCEVENT_HOSTBYIP",
  dns_dcchostbyip
};

devent_type DNS_DCCEVENT_IPBYHOST = {
  "DCCEVENT_IPBYHOST",
  dns_dccipbyhost
};

void dcc_dnsipbyhost(char *hostn)
{
  devent_t *de;

  for (de = dns_events; de; de = de->next) {
    if (de->type && (de->type == &DNS_DCCEVENT_IPBYHOST) &&
	(de->lookup == RES_IPBYHOST)) {
      if (de->res_data.hostname &&
	  !egg_strcasecmp(de->res_data.hostname, hostn))
	/* No need to add anymore. */
	return;
    }
  }

  de = malloc(sizeof(devent_t));
  egg_bzero(de, sizeof(devent_t));

  /* Link into list. */
  de->next = dns_events;
  dns_events = de;

  de->type = &DNS_DCCEVENT_IPBYHOST;
  de->lookup = RES_IPBYHOST;
  de->res_data.hostname = malloc(strlen(hostn) + 1);
  strcpy(de->res_data.hostname, hostn);

  /* Send request. */
  dns_ipbyhost(hostn);
}

void dcc_dnshostbyip(IP ip)
{
  devent_t *de;

  for (de = dns_events; de; de = de->next) {
    if (de->type && (de->type == &DNS_DCCEVENT_HOSTBYIP) &&
	(de->lookup == RES_HOSTBYIP)) {
      if (de->res_data.ip_addr == ip)
	/* No need to add anymore. */
	return;
    }
  }

  de = malloc(sizeof(devent_t));
  egg_bzero(de, sizeof(devent_t));

  /* Link into list. */
  de->next = dns_events;
  dns_events = de;

  de->type = &DNS_DCCEVENT_HOSTBYIP;
  de->lookup = RES_HOSTBYIP;
  de->res_data.ip_addr = ip;

  /* Send request. */
  dns_hostbyip(ip);
}


/*
 *   Tcl events
 */

static void dns_tcl_iporhostres(IP ip, char *hostn, int ok, void *other)
{
  devent_tclinfo_t *tclinfo = (devent_tclinfo_t *) other;

  if (Tcl_VarEval(interp, tclinfo->proc, " ", iptostr(htonl(ip)), " ",
		  hostn, ok ? " 1" : " 0", tclinfo->paras, NULL) == TCL_ERROR)
    putlog(LOG_MISC, "*", DCC_TCLERROR, tclinfo->proc, interp->result);

  /* Free the memory. It will be unused after this event call. */
  free(tclinfo->proc);
  if (tclinfo->paras)
    free(tclinfo->paras);
  free(tclinfo);
}

devent_type DNS_TCLEVENT_HOSTBYIP = {
  "TCLEVENT_HOSTBYIP",
  dns_tcl_iporhostres
};

devent_type DNS_TCLEVENT_IPBYHOST = {
  "TCLEVENT_IPBYHOST",
  dns_tcl_iporhostres
};


/*
 *    Event functions
 */

void call_hostbyip(IP ip, char *hostn, int ok)
{
  devent_t *de = dns_events, *ode = NULL, *nde = NULL;

  while (de) {
    nde = de->next;
    if ((de->lookup == RES_HOSTBYIP) &&
	(!de->res_data.ip_addr || (de->res_data.ip_addr == ip))) {
      /* Remove the event from the list here, to avoid conflicts if one of
       * the event handlers re-adds another event. */
      if (ode)
	ode->next = de->next;
      else
	dns_events = de->next;

      if (de->type && de->type->event)
	de->type->event(ip, hostn, ok, de->other);
      else
	putlog(LOG_MISC, "*", "(!) Unknown DNS event type found: %s",
	       (de->type && de->type->name) ? de->type->name : "<empty>");
      free(de);
      de = ode;
    }
    ode = de;
    de = nde;
  }
}

void call_ipbyhost(char *hostn, IP ip, int ok)
{
  devent_t *de = dns_events, *ode = NULL, *nde = NULL;

  while (de) {
    nde = de->next;
    if ((de->lookup == RES_IPBYHOST) &&
	(!de->res_data.hostname ||
	 !egg_strcasecmp(de->res_data.hostname, hostn))) {
      /* Remove the event from the list here, to avoid conflicts if one of
       * the event handlers re-adds another event. */
      if (ode)
	ode->next = de->next;
      else
	dns_events = de->next;

      if (de->type && de->type->event)
	de->type->event(ip, hostn, ok, de->other);
      else
	putlog(LOG_MISC, "*", "(!) Unknown DNS event type found: %s",
	       (de->type && de->type->name) ? de->type->name : "<empty>");

      if (de->res_data.hostname)
	free(de->res_data.hostname);
      free(de);
      de = ode;
    }
    ode = de;
    de = nde;
  }
}


/*
 *    Async DNS emulation functions
 */

void block_dns_hostbyip(IP ip)
{
  struct hostent *hp;
  unsigned long addr = htonl(ip);
  static char s[UHOSTLEN];

  if (!setjmp(alarmret)) {
    alarm(resolve_timeout);
    hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    alarm(0);
    if (hp) {
      strncpyz(s, hp->h_name, sizeof s);
    } else
      strcpy(s, iptostr(addr));
  } else {
    hp = NULL;
    strcpy(s, iptostr(addr));
  }
  /* Call hooks. */
  call_hostbyip(ip, s, hp ? 1 : 0);
}

void block_dns_ipbyhost(char *host)
{
  struct in_addr inaddr;

  /* Check if someone passed us an IP address as hostname
   * and return it straight away */
  if (egg_inet_aton(host, &inaddr)) {
    call_ipbyhost(host, ntohl(inaddr.s_addr), 1);
    return;
  }
  if (!setjmp(alarmret)) {
    struct hostent *hp;
    struct in_addr *in;
    IP ip = 0;
    alarm(resolve_timeout);
    hp = gethostbyname(host);
    alarm(0);

    if (hp) {
      in = (struct in_addr *) (hp->h_addr_list[0]);
      ip = (IP) (in->s_addr);
      call_ipbyhost(host, ntohl(ip), 1);
      return;
    }
    /* Fall through. */
  }
  call_ipbyhost(host, 0, 0);
}

