/*
 * dns.c -- part of dns.mod
 *   domain lookup glue code for eggdrop
 *
 * Written by Fabian Knittel <fknittel@gmx.de>
 *
 */

#include "dns.h"
#include "src/common.h"
#include "src/dccutil.h"
#include "src/main.h"
#include "src/egg_timer.h"
#include "src/types.h"

static void dns_event_success(struct resolve *rp, int type);
static void dns_event_failure(struct resolve *rp, int type);


#include "coredns.c"

/*
 *    DNS event related code
 */

static void dns_event_success(struct resolve *rp, int type)
{
  if (!rp)
    return;

  if (type == T_PTR) {
    debug2("DNS resolved %s to %s", iptostr(rp->ip), rp->hostn);
    call_hostbyip(ntohl(rp->ip), rp->hostn, 1);
  } else if (type == T_A) {
    debug2("DNS resolved %s to %s", rp->hostn, iptostr(rp->ip));
    call_ipbyhost(rp->hostn, ntohl(rp->ip), 1);
  }
}

static void dns_event_failure(struct resolve *rp, int type)
{
  if (!rp)
    return;

  if (type == T_PTR) {
    static char s[UHOSTLEN];

    debug1("DNS resolve failed for %s", iptostr(rp->ip));
    strcpy(s, iptostr(rp->ip));
    call_hostbyip(ntohl(rp->ip), s, 0);
  } else if (type == T_A) {
    debug1("DNS resolve failed for %s", rp->hostn);
    call_ipbyhost(rp->hostn, 0, 0);
  } else
    debug2("DNS resolve failed for unknown %s / %s", iptostr(rp->ip),
	   nonull(rp->hostn));
  return;
}


/*
 *    DNS Socket related code
 */

static void eof_dns_socket(int idx)
{
  putlog(LOG_MISC, "*", "DNS Error: socket closed.");
  killsock(dcc[idx].sock);
  /* Try to reopen socket */
  if (init_dns_network()) {
    putlog(LOG_MISC, "*", "DNS socket successfully reopened!");
    dcc[idx].sock = resfd;
    dcc[idx].timeval = now;
  } else
    lostdcc(idx);
}

static void dns_socket(int idx, char *buf, size_t len)
{
  dns_ack();
}

static void display_dns_socket(int idx, char *buf)
{
  strcpy(buf, "dns   (ready)");
}

static struct dcc_table DCC_DNS =
{
  "DNS",
  DCT_LISTEN,
  eof_dns_socket,
  dns_socket,
  NULL,
  NULL,
  display_dns_socket,
  NULL,
  NULL,
  NULL
};


/*
 *    DNS module related code
 */

int dns_report(int idx, int details)
{
  if (details) {
    dprintf(idx, "    DNS resolver is active.\n");
  }
  return 0;
}

void dns_init()
{
  int idx;

  idx = new_dcc(&DCC_DNS, 0);
  if (idx < 0)
    fatal("NO MORE DCC CONNECTIONS -- Can't create DNS socket.", 1);
  if (!init_dns_core()) {
    lostdcc(idx);
    fatal("DNS initialisation failed.", 1);
  }
  dcc[idx].sock = resfd;
  dcc[idx].timeval = now;
  strcpy(dcc[idx].nick, "(dns)");

  timer_create_secs(1, "dns_check_expires", (Function) dns_check_expires);
}
