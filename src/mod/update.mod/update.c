/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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
 * update.c -- part of update.mod
 *
 */


#include "src/common.h"
#include "src/users.h"
#include "src/dcc.h"
#include "src/botnet.h"
#include "src/main.h"
#include "src/userrec.h"
#include "src/botmsg.h"
#include "src/tandem.h"
#include "src/misc_file.h"
#include "src/net.h"
#include "src/binds.h"
#include "src/egg_timer.h"
#include "src/misc.h"
#include "src/EncryptedStream.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>


#include "src/mod/transfer.mod/transfer.h"
#include "src/mod/compress.mod/compress.h"


/* Prototypes */
static void start_sending_binary(int, bool);
void finish_update_stream(int idx, bd::Stream& stream);

#include "update.h"

extern struct dcc_table DCC_FORK_SEND, DCC_GET;


static bd::Stream stream_in;
int bupdating = 0;
int updated = 0;

/*
 *   Botnet commands
 */

static void update_ufno(int idx, char *par)
{
  putlog(LOG_BOTS, "*", "binary file rejected by %s: %s",
	 dcc[idx].nick, par);
  dcc[idx].status &= ~STAT_OFFEREDU;
}

static void update_ufyes(int idx, char *par)
{
  if (dcc[idx].status & STAT_OFFEREDU) {
    if (strstr(par, "stream")) {
      start_sending_binary(idx, 1);
    } else {
      start_sending_binary(idx, 0);
    }
  }
}

static void update_fileq(int idx, char *par)
{
  if (dcc[idx].status & STAT_GETTINGU || updated) 
    return;

  if (!conf.bot->hub) {
    if (!conf.bot->localhub)
      return;
  } else {
    if (isupdatehub()) {
      dprintf(idx, "sb un I am the update hub, NOT YOU.\n");
      return;
    }
  }

  dprintf(idx, "sb uy stream\n");
}

/* us <ip> <port> <length>
 */
static void update_ufsend(int idx, char *par)
{

  char *ip = NULL, *port = NULL, s[1024] = "";
  int i, sock;
  FILE *f = NULL;

  putlog(LOG_BOTS, "*", "Downloading updated binary from %s", dcc[idx].nick);
  simple_snprintf(s, sizeof s, "%s.update.%s", tempdir, conf.bot->nick);
  unlink(s); /* make sure there isnt already a new binary here.. */
  if (dcc_total == max_dcc) {
    putlog(LOG_MISC, "*", "NO MORE DCC CONNECTIONS -- can't grab new binary");
    dprintf(idx, "sb e I can't open a DCC to you; I'm full.\n");
    zapfbot(idx);
  } else if (!(f = fopen(s, "wb"))) {
    putlog(LOG_MISC, "*", "CAN'T WRITE BINARY DOWNLOAD FILE!");
    zapfbot(idx);
  } else {
    ip = newsplit(&par);
    port = newsplit(&par);
#ifdef USE_IPV6
    sock = getsock(SOCK_BINARY, AF_INET);
#else
    sock = getsock(SOCK_BINARY); /* Don't buffer this -> mark binary. */
#endif /* USE_IPV6 */
    if (sock < 0 || open_telnet_dcc(sock, ip, port) < 0) {
      killsock(sock);
      putlog(LOG_BOTS, "*", "Asynchronous connection failed!");
      dprintf(idx, "sb e Can't connect to you!\n");
      zapfbot(idx);
    } else {
      putlog(LOG_DEBUG, "*", "Connecting to %s:%s for new binary.", ip, port);
      i = new_dcc(&DCC_FORK_SEND, sizeof(struct xfer_info));
      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(port);
      strlcpy(dcc[i].nick, "*binary", NICKLEN);
      dcc[i].u.xfer->filename = strdup(s);
      dcc[i].u.xfer->origname = dcc[i].u.xfer->filename;
      dcc[i].u.xfer->length = atol(par);
      dcc[i].u.xfer->f = f;
      dcc[i].sock = sock;
      strlcpy(dcc[i].host, dcc[idx].nick, UHOSTLEN);

      dcc[idx].status |= STAT_GETTINGU;
    }
  }
}

static void update_stream_line(int idx, char *par) {
  char *size = newsplit(&par);

  stream_in << bd::base64Decode(bd::String(par, atoi(size)));
}

static void update_stream_start(int idx, char *par) {
  putlog(LOG_BOTS, "*", "Downloading updated binary from %s", dcc[idx].nick);
  dcc[idx].status |= STAT_GETTINGU;
  stream_in.clear();
}

static void update_stream_end(int idx, char *par) {
  stream_in.seek(0, SEEK_SET);
  finish_update_stream(idx, stream_in);
}

/* Note: these MUST be sorted. */
static botcmd_t C_update[] =
{
  {"l", 	update_stream_line, 0},
  {"le", 	update_stream_end, 0},
  {"ls", 	update_stream_start, 0},
  {"u?",	update_fileq, 0},
  {"un",	update_ufno, 0},
  {"us",	update_ufsend, 0},
  {"uy",	update_ufyes, 0},
  {NULL,	NULL, 0}
};

static void got_nu(char *botnick, char *code, char *par)
{
  if (!par || !*par || updated) 
    return;
  if (!conf.bot->hub) {
    if (!conf.bot->localhub)
      return;

    if (!conf.bot->u || !userlist || !get_user_by_handle(userlist, botnick))	/* probably still getting userfile */
      return;

    if (tandbot && tandbot->bot && !strcmp(tandbot->bot, botnick)) /* dont listen to our uplink.. use normal upate system.. */
      return;
  }

/* needupdate? curver */
   time_t newbuildts = atol(newsplit(&par)); //newts
   newsplit(&par); //Not used (old svn revision)
   const char *newcommit = newsplit(&par);

   if (newbuildts > buildts) {
     if (!conf.bot->hub) {
       dont_restructure = 1;
       putlog(LOG_MISC, "*", "Linking to %s for binary update.", botnick);
       botunlink(-2, tandbot->bot, "Restructure for update.");
       usleep(1000 * 500);
       botlink("", -3, botnick);
     } else 
       putlog(LOG_MISC, "*", "I need to be updated with %li/%s", (long) newbuildts, newcommit);
   }  
}

static cmd_t update_bot[] = {
  {"nu?",    "", (Function) got_nu, NULL, 0}, 
  {NULL, NULL, NULL, NULL, 0}
};


void updatein(int idx, char *msg)
{
  char *code = newsplit(&msg);
  const botcmd_t *cmd = search_botcmd_t((const botcmd_t*)&C_update, code, (sizeof(C_update)/sizeof(botcmd_t)) - 1);
  if (cmd) {
    /* Found a match */
    (cmd->func)(idx, msg);
  }
}

void finish_update_stream(int idx, bd::Stream& stream)
{
  char buf[1024] = "", *buf2 = NULL;

  char rand[7] = "";
  make_rand_str(rand, sizeof(rand) - 1, 0);
  simple_snprintf(buf, sizeof(buf), "%s/.update-%s", conf.binpath, rand);

  stream.writeFile(buf);
  fixmod(buf);

  buf2 = strrchr(buf, '/') + 1;

  putlog(LOG_DEBUG, "*", "Update binary is %zu bytes.", stream.length());
  putlog(LOG_MISC, "*", "Updating with binary: %s", buf2);
  
  if (updatebin(0, buf2, 120))
    putlog(LOG_MISC, "*", "Failed to update to new binary..");
  else
    updated = 1;
}

static void
ulsend(int idx, const char* data, size_t datalen)
{
  char buf[1400] = "";

  size_t len = simple_snprintf(buf, sizeof(buf), "sb l %d %s", datalen-1, data);/* -1 for newline */
  tputs(dcc[idx].sock, buf, len);
}

void finish_update(int idx) {
  bd::Stream stream;
  stream.loadFile(dcc[idx].u.xfer->filename);
  unlink(dcc[idx].u.xfer->filename);
  finish_update_stream(idx, stream);
}

static void start_sending_binary(int idx, bool streamable)
{
  /* module_entry *me; */
  char update_file[51] = "", update_fpath[DIRMAX] = "", *sysname = NULL;
  int i = 1, j = -1;

  dcc[idx].status &= ~(STAT_OFFEREDU | STAT_SENDINGU);

  if (bupdating) return;
  bupdating = 1;

  dcc[idx].status |= STAT_SENDINGU;

  putlog(LOG_BOTS, "*", "Sending binary send request to %s", dcc[idx].nick);

  sysname = (char *) get_user(&USERENTRY_OS, dcc[idx].user);

  if (!sysname || !sysname[0] || !strcmp("*", sysname)) {
    putlog(LOG_MISC, "*", "Cannot update \002%s\002 automatically, `uname` not returning os name.", dcc[idx].nick);
    return;
  }

  simple_snprintf(update_file, sizeof update_file, "wraith.%s-%s", sysname, egg_version);

  simple_snprintf(update_fpath, sizeof update_fpath, "%s/bins/%s", conf.binpath, update_file);
  if (!can_stat(update_fpath))
    simple_snprintf(update_fpath, sizeof update_fpath, "%s/%s", conf.binpath, update_file);

  if (!can_stat(update_fpath)) {
    putlog(LOG_MISC, "*", "Need to update \002%s\002 with %s but there was an error: %s", dcc[idx].nick, update_fpath,
                           strerror(errno));
    dcc[idx].status &= ~(STAT_SENDINGU);
    bupdating = 0;
    return;
  }

  if (streamable) {
    dcc[idx].status |= STAT_SENDINGU;

    bd::Stream stream;
    stream.loadFile(update_fpath);
    dprintf(idx, "sb ls\n");
    bd::String buf;
    while (stream.tell() < stream.length()) {
      buf = bd::base64Encode(stream.gets(1024));
      ulsend(idx, buf.c_str(), buf.length());
    }
    dprintf(idx, "sb le\n");
    putlog(LOG_BOTS, "*", "Completed binary file send to %s", dcc[idx].nick);
    dcc[idx].status &= ~STAT_SENDINGU;
    dcc[idx].status |= STAT_UPDATED;
    bupdating = 0;
  } else {
    if ((i = raw_dcc_send(update_fpath, "*binary", "(binary)", &j)) > 0) {
      putlog(LOG_BOTS, "*", "%s -- can't send new binary",
             i == DCCSEND_FULL   ? "NO MORE DCC CONNECTIONS" :
             i == DCCSEND_NOSOCK ? "CAN'T OPEN A LISTENING SOCKET" :
             i == DCCSEND_BADFN  ? "BAD FILE" :
             i == DCCSEND_FEMPTY ? "EMPTY FILE" : "UNKNOWN REASON!");
      dcc[idx].status &= ~(STAT_SENDINGU);
      bupdating = 0;
    } else {
      dcc[idx].status |= STAT_SENDINGU;
      strlcpy(dcc[j].host, dcc[idx].nick, UHOSTLEN);		/* Store bot's nick */
      dprintf(idx, "sb us %lu %hd %lu\n", iptolong(getmyip()), dcc[j].port, dcc[j].u.xfer->length);
    }
  }
}

int cnt = 0;
static void check_updates()
{
  if (isupdatehub()) {
    int i;
    char buf[1024] = "";
    tand_t *bot = NULL;

    cnt++;
    if ((cnt > 5) && bupdating)  bupdating = 0; /* 2 minutes should be plenty. */
    if (bupdating) return;
    cnt = 0;

    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && dcc[i].type->flags & DCT_BOT && (dcc[i].status & STAT_SHARE) &&
          !(dcc[i].status & STAT_SENDINGU) && !(dcc[i].status & STAT_OFFEREDU) &&
          !(dcc[i].status & STAT_UPDATED)) { /* only offer binary to bots that are sharing */
        bot = findbot(dcc[i].nick);

        dcc[i].status &= ~(STAT_GETTINGU | STAT_SENDINGU | STAT_OFFEREDU);

        if (bot && (bot->buildts < buildts)) {
          putlog(LOG_DEBUG, "@", "Bot: %s has build %li/%s, offering them %li/%s", dcc[i].nick, (long)bot->buildts, bot->commit, (long) buildts, commit);
          dprintf(i, "sb u?\n");
          dcc[i].status |= STAT_OFFEREDU;
        }
      }
    }
    /* send out notice to update remote bots ... */
    /* 9999 is a hack to force all bots from old svn-revisions to upgrade to new git style */
    simple_snprintf(buf, sizeof buf, "nu? %li 9999 %s", (long) buildts, commit);
    putallbots(buf);
  }
}

void update_report(int idx, int details)
{
  int i, j;

  if (details) {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type && dcc[i].type == &DCC_BOT) {
	if (dcc[i].status & STAT_GETTINGU) {
	  int ok = 0;

	  for (j = 0; j < dcc_total; j++)
	    if (dcc[j].type && ((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == (DCT_FILETRAN | DCT_FILESEND)) &&
		!strcasecmp(dcc[j].host, dcc[i].nick)) {
	      dprintf(idx, "Downloading binary from %s (%d%% done)\n",
		      dcc[i].nick,
		      (int) (100.0 * ((float) dcc[j].status) /
			     ((float) dcc[j].u.xfer->length)));
	      ok = 1;
	      break;
	    }
	  if (!ok)
	    dprintf(idx, "Download binary from %s (negotiating "
		    "botentries)\n", dcc[i].nick);
	} else if (dcc[i].status & STAT_SENDINGU) {
	  for (j = 0; j < dcc_total; j++) {
	    if (dcc[j].type && ((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == DCT_FILETRAN)
		&& !strcasecmp(dcc[j].host, dcc[i].nick)) {
	      if (dcc[j].type == &DCC_GET)
		dprintf(idx, "Sending binary to %s (%d%% done)\n",
			dcc[i].nick,
			(int) (100.0 * ((float) dcc[j].status) /
			       ((float) dcc[j].u.xfer->length)));
	      else
		dprintf(idx,
			"Sending binary to %s (waiting for connect)\n",
			dcc[i].nick);
	    }
	  }
	}
      }
  }
}

static void cmd_bupdate(int idx, char *par)
{
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && !strcasecmp(dcc[i].nick, par)) {
      dprintf(i, "sb u?\n");
      dcc[i].status &= ~(STAT_SENDINGU | STAT_UPDATED);
      dcc[i].status |= STAT_OFFEREDU;
    }
  }
}

cmd_t update_cmds[] =
{
  {"bupdate",		"a",	(Function) cmd_bupdate,		NULL, HUB},
  {NULL,		NULL,	NULL,				NULL, 0}
};

void update_init()
{
  add_builtins("bot", update_bot);
  add_builtins("dcc", update_cmds);
  if (conf.bot->hub) {
    timer_create_secs(30, "check_updates", (Function) check_updates);
  }
}
