#include "main.h"
#include <ctype.h>
#include <errno.h>
#include "modules.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#include "tandem.h"
#include "blowfish_conf.h"
#include <sys/stat.h>
extern char netpass[9];
#include "md5/md5.h"
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern Tcl_Interp *interp;
extern time_t now;
extern int egg_numver, connect_timeout, conmask, backgrd, max_dcc,
  default_flags, debug_output, ignore_time, par_telnet_flood;
extern char botnetnick[], ver[], origbotname[], notify_new[];
extern sock_list *socklist;
extern int MAXSOCKS;
int timesync = 0;
struct dcc_t *dcc = 0;
int dcc_total = 0;
int allow_new_telnets = 0;
int use_telnet_banner = 0;
char network[41] = "EFnet";
int password_timeout = 30;
int bot_timeout = 25;
int identtimeout = 5;
int dupwait_timeout = 5;
int protect_telnet = 0;
int flood_telnet_thr = 10;
int flood_telnet_time = 5;
char bannerfile[121] = "";
extern char dcc_prefix[];
struct portmap *root = NULL;
static void dcc_telnet_hostresolved (int);
static void dcc_telnet_got_ident (int, char *);
static void dcc_telnet_pass (int, int);
char *
rand_dccresp ()
{
  switch (random () % 10)
    {
    case 0:
      return "sup\n";
    case 1:
      return "a/s/l?\ni'm 17/f/ca ;)\n";
    case 2:
      return "who are you?\n";
    case 3:
      return "uhhh do i know you?\n";
    case 4:
      return "what?\n";
    case 5:
      return "wtf do you want?\n";
    case 6:
      return "hold on a second, I am sort of busy..\n";
    case 7:
      return
	"mIRC v6.03 File Server\n\nUse: cd dir ls get read help exit\n[\\]\n";
    case 8:
      return "got any porn?\n";
    default:
      return "";
    }
}
char *
rand_dccresppass ()
{
  switch (random () % 10)
    {
    case 0:
      return "what?\n";
    case 1:
      return "huh?\n";
    case 2:
      return "no.\n";
    case 3:
      return "thats great..\n";
    case 4:
      return
	"hmm, ok..I've got a better idea, check this out\nhttp://peeldmonkeys.8k.com/images/keza-_middle_finger.jpg\n";
    case 5:
      return "I don't remember caring..\n";
    case 6:
      return "good for you.\n";
    case 7:
      return "I'm going to report you to the RIAA!!!\n";
    default:
      return "";
    }
}
char *
rand_dccrespbye ()
{
  switch (random () % 10)
    {
    case 0:
      return "stop wasting my time.\n";
    case 1:
      return "gtg\n";
    case 2:
      return "go away\n";
    case 3:
      return "fuck off already\n";
    case 4:
      return "ehh..no, bye.\n";
    case 5:
      return "hey I'm late for a date with your mom, cya..\n";
    case 6:
      return "you're still here?\n";
    case 7:
      return "jesus loves you, but I ain't jesus.\nNOW \002GO AWAY\002\n";
    default:
      return "";
    }
}
static void
strip_telnet (int sock, char *buf, int *len)
{
  unsigned char *p = (unsigned char *) buf, *o = (unsigned char *) buf;
  int mark;
  while (*p != 0)
    {
      while ((*p != TLN_IAC) && (*p != 0))
	{
	  if (*p == 0xA0)
	    {
	      *o++ = 32;
	      p++;
	    }
	  else
	    *o++ = *p++;
	}
      if (*p == TLN_IAC)
	{
	  p++;
	  mark = 2;
	  if (!*p)
	    mark = 1;
	  if ((*p >= TLN_WILL) && (*p <= TLN_DONT))
	    {
	      mark = 3;
	      if (!*(p + 1))
		mark = 2;
	    }
	  if (*p == TLN_WILL)
	    {
	      if (*(p + 1) != TLN_ECHO)
		{
		  write (sock, TLN_IAC_C TLN_DONT_C, 2);
		  write (sock, p + 1, 1);
		}
	    }
	  if (*p == TLN_DO)
	    {
	      if (*(p + 1) != TLN_ECHO)
		{
		  write (sock, TLN_IAC_C TLN_WONT_C, 2);
		  write (sock, p + 1, 1);
		}
	    }
	  if (*p == TLN_AYT)
	    {
	      write (sock, "\r\nHell, yes!\r\n", 14);
	    }
	  p += mark - 1;
	  *len = *len - mark;
	}
    }
  *o = *p;
}

void
send_timesync (idx)
{
  if (idx >= 0)
    dprintf (idx, "ts %li\n", (timesync + now));
  else
    {
#ifdef HUB
      char s[30];
      int i;
      sprintf (s, STR ("ts %li\n"), (timesync + now));
      for (i = 0; i < dcc_total; i++)
	{
	  if ((dcc[i].type == &DCC_BOT) && (bot_aggressive_to (dcc[i].user)))
	    {
	      dprintf (i, s);
	    }
	}
#else
      putlog (LOG_ERRORS, "*", "I'm a leaf - where should i send timesync?");
#endif
    }
}
static void
greet_new_bot (int idx)
{
  int i;
  char *sysname = NULL;
#ifdef HAVE_UNAME
  struct utsname un;
#endif
  dcc[idx].timeval = now;
  dcc[idx].u.bot->version[0] = 0;
  dcc[idx].u.bot->sysname[0] = 0;
  dcc[idx].u.bot->numver = 0;
#ifdef HUB
  if (dcc[idx].user && (!(dcc[idx].user->flags & USER_OP)))
    {
      putlog (LOG_BOTS, "*", "Rejecting link from %s", dcc[idx].nick);
      dprintf (idx, "error You are being rejected.\n");
      dprintf (idx, "bye\n");
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
#endif
  if (0 == 999)
    dcc[idx].status |= STAT_LEAF;
  dcc[idx].status |= STAT_LINKING;
#ifdef HAVE_UNAME
  if (uname (&un) < 0)
    sysname = "*";
  else
    sysname = un.sysname;
#endif
  Context;
  dprintf (idx, "v %d %d %s <%s>\n", egg_numver, HANDLEN, ver, network);
  Context;
  Context;
  dprintf (idx, "vs %s\n", sysname);
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_FORK_BOT)
      {
	killsock (dcc[i].sock);
	lostdcc (i);
      }
}
static void
bot_version (int idx, char *par)
{
  char x[1024];
  int l;
  dcc[idx].timeval = now;
  if (in_chain (dcc[idx].nick))
    {
      dprintf (idx, "error Sorry, already connected.\n");
      dprintf (idx, "bye\n");
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  if ((par[0] >= '0') && (par[0] <= '9'))
    {
      char *work;
      work = newsplit (&par);
      dcc[idx].u.bot->numver = atoi (work);
    }
  else
    dcc[idx].u.bot->numver = 0;
  dprintf (idx, "tb %s\n", botnetnick);
  l = atoi (newsplit (&par));
  if (l != HANDLEN)
    {
      putlog (LOG_BOTS, "*",
	      "Non-matching handle lengths with %s, they use %d characters.",
	      dcc[idx].nick, l);
      dprintf (idx, "error Non-matching handle length: mine %d, yours %d\n",
	       HANDLEN, l);
      dprintf (idx, "bye %s\n", "bad handlen");
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  strncpyz (dcc[idx].u.bot->version, par, 120);
  putlog (LOG_BOTS, "*", DCC_LINKED, dcc[idx].nick);
  chatout ("*** Linked to %s\n", dcc[idx].nick);
  botnet_send_nlinked (idx, dcc[idx].nick, botnetnick, '!',
		       dcc[idx].u.bot->numver);
  touch_laston (dcc[idx].user, "linked", now);
  dump_links (idx);
  dcc[idx].type = &DCC_BOT;
  addbot (dcc[idx].nick, dcc[idx].nick, botnetnick, '-',
	  dcc[idx].u.bot->numver);
  check_tcl_link (dcc[idx].nick, botnetnick);
  egg_snprintf (x, sizeof x, "v %d", dcc[idx].u.bot->numver);
  bot_shareupdate (idx, x);
  bot_share (idx, x);
  dprintf (idx, "el\n");
}

void
failed_link (int idx)
{
  char s[81], s1[512];
  if (dcc[idx].u.bot->linker[0])
    {
      egg_snprintf (s, sizeof s, "Couldn't link to %s.", dcc[idx].nick);
      strcpy (s1, dcc[idx].u.bot->linker);
      add_note (s1, botnetnick, s, -2, 0);
    }
  if (dcc[idx].u.bot->numver >= (-1))
    putlog (LOG_BOTS, "*", DCC_LINKFAIL, dcc[idx].nick);
  killsock (dcc[idx].sock);
  strcpy (s, dcc[idx].nick);
  lostdcc (idx);
  autolink_cycle (s);
}

int norestruct = 0;
static void
cont_link (int idx, char *buf, int ii)
{
  struct sockaddr_in sa;
  char tmp[256];
  MD5_CTX ctx;
  int i;
  int snum = -1;
  for (i = 0; i < MAXSOCKS; i++)
    {
      if ((socklist[i].sock == dcc[idx].sock)
	  && !(socklist[i].flags & SOCK_UNUSED))
	{
	  snum = i;
	}
    }
  if (snum >= 0)
    {
      int i;
      if (!norestruct)
	{
	  for (i = 0; i < dcc_total; i++)
	    {
	      if ((dcc[i].type == &DCC_BOT)
		  && (!bot_aggressive_to (dcc[i].user)))
		{
		  putlog (LOG_BOTS, "*", STR ("Unlinking %s - restructure"),
			  dcc[i].nick);
		  botnet_send_unlinked (i, dcc[i].nick, STR ("Restructure"));
		  killsock (dcc[i].sock);
		  lostdcc (i);
		  usleep (1000 * 500);
		  break;
		}
	    }
	}
      dcc[idx].type = &DCC_BOT_NEW;
      dcc[idx].u.bot->numver = 0;
      dprintf (idx, "%s\n", botnetnick);
      i = sizeof (sa);
      getsockname (socklist[snum].sock, (struct sockaddr *) &sa, &i);
      sprintf (tmp, "%8x@%4x@%s@%s", getmyip (), sa.sin_port, dcc[idx].nick,
	       botnetnick);
      MD5_Init (&ctx);
      MD5_Update (&ctx, tmp, strlen (tmp));
      MD5_Final (socklist[snum].ikey, &ctx);
      socklist[snum].encstatus = 1;
    }
  else
    {
      lostdcc (idx);
      killsock (dcc[idx].sock);
    }
  return;
}
static void
dcc_bot_new (int idx, char *buf, int x)
{
  char *code;
  int i;
  strip_telnet (dcc[idx].sock, buf, &x);
  code = newsplit (&buf);
  if (!egg_strcasecmp (code, "goodbye!"))
    {
      greet_new_bot (idx);
    }
  else if (!egg_strcasecmp (code, "v"))
    {
      bot_version (idx, buf);
    }
  else if (!strcasecmp (code, "elink"))
    {
      int snum = -1;
      for (i = 0; i < MAXSOCKS; i++)
	{
	  if (!(socklist[i].flags & SOCK_UNUSED)
	      && (socklist[i].sock == dcc[idx].sock))
	    {
	      snum = i;
	      break;
	    }
	}
      if (snum >= 0)
	{
	  char *tmp, *p;
	  p = newsplit (&buf);
	  tmp = decrypt_string (netpass, p);
	  strncpy0 (socklist[snum].okey, tmp, 17);
	  strcpy (socklist[snum].ikey, socklist[snum].okey);
	  nfree (tmp);
	  socklist[snum].iseed = atoi (buf);
	  socklist[snum].oseed = atoi (buf);
	  dprintf (idx, "elinkdone\n");
	  putlog (LOG_BOTS, "*", "Handshake with %s succeeded, we're linked.",
		  dcc[idx].nick);
	}
    }
  else if (!strcasecmp (code, "error"))
    {
      putlog (LOG_MISC, "*", "ERROR linking %s: %s", dcc[idx].nick, buf);
      killsock (dcc[idx].sock);
      lostdcc (idx);
    }
}
static void
eof_dcc_bot_new (int idx)
{
  putlog (LOG_BOTS, "*", DCC_LOSTBOT, dcc[idx].nick, dcc[idx].port);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
timeout_dcc_bot_new (int idx)
{
  putlog (LOG_BOTS, "*", DCC_TIMEOUT, dcc[idx].nick, dcc[idx].host,
	  dcc[idx].port);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
display_dcc_bot_new (int idx, char *buf)
{
  sprintf (buf, "bot*  waited %lus", now - dcc[idx].timeval);
} static int
expmem_dcc_bot_ (void *x)
{
  return sizeof (struct bot_info);
} static void
free_dcc_bot_ (int n, void *x)
{
  if (dcc[n].type == &DCC_BOT)
    {
      unvia (n, findbot (dcc[n].nick));
      rembot (dcc[n].nick);
    }
  nfree (x);
}
struct dcc_table DCC_BOT_NEW =
  { "BOT_NEW", 0, eof_dcc_bot_new, dcc_bot_new, &bot_timeout,
timeout_dcc_bot_new, display_dcc_bot_new, expmem_dcc_bot_, free_dcc_bot_, NULL };
extern botcmd_t C_bot[];
static void
dcc_bot (int idx, char *code, int i)
{
  char *msg;
  int f;
  strip_telnet (dcc[idx].sock, code, &i);
  if (debug_output)
    {
      if (code[0] == 's')
	putlog (LOG_BOTSHARE, "@", "{%s} %s", dcc[idx].nick, code + 2);
      else
	putlog (LOG_BOTNET, "@", "[%s] %s", dcc[idx].nick, code);
    }
  msg = strchr (code, ' ');
  if (msg)
    {
      *msg = 0;
      msg++;
    }
  else
    msg = "";
  for (f = i = 0; C_bot[i].name && !f; i++)
    {
      int y = egg_strcasecmp (code, C_bot[i].name);
      if (!y)
	{
	  (C_bot[i].func) (idx, msg);
	  f = 1;
	}
      else if (y < 0)
	return;
    }
}
static void
eof_dcc_bot (int idx)
{
  char x[1024];
  int bots, users;
  bots = bots_in_subtree (findbot (dcc[idx].nick));
  users = users_in_subtree (findbot (dcc[idx].nick));
  egg_snprintf (x, sizeof x, "Lost bot: %s (lost %d bot%s and %d user%s)",
		dcc[idx].nick, bots, (bots != 1) ? "s" : "", users,
		(users != 1) ? "s" : "");
  putlog (LOG_BOTS, "*", "%s.", x);
  chatout ("*** %s\n", x);
  botnet_send_unlinked (idx, dcc[idx].nick, x);
  killsock (dcc[idx].sock);
  lostdcc (idx);
}
static void
display_dcc_bot (int idx, char *buf)
{
  int i = simple_sprintf (buf, "bot   flags: ");
  buf[i++] = b_status (idx) & STAT_PINGED ? 'P' : 'p';
  buf[i++] = b_status (idx) & STAT_SHARE ? 'U' : 'u';
  buf[i++] = b_status (idx) & STAT_CALLED ? 'C' : 'c';
  buf[i++] = b_status (idx) & STAT_OFFERED ? 'O' : 'o';
  buf[i++] = b_status (idx) & STAT_SENDING ? 'S' : 's';
  buf[i++] = b_status (idx) & STAT_GETTING ? 'G' : 'g';
  buf[i++] = b_status (idx) & STAT_WARNED ? 'W' : 'w';
  buf[i++] = b_status (idx) & STAT_LEAF ? 'L' : 'l';
  buf[i++] = b_status (idx) & STAT_LINKING ? 'I' : 'i';
  buf[i++] = b_status (idx) & STAT_AGGRESSIVE ? 'a' : 'A';
  buf[i++] = b_status (idx) & STAT_OFFEREDU ? 'B' : 'b';
  buf[i++] = b_status (idx) & STAT_SENDINGU ? 'D' : 'd';
  buf[i++] = b_status (idx) & STAT_GETTINGU ? 'E' : 'e';
  buf[i++] = 0;
} static void
display_dcc_fork_bot (int idx, char *buf)
{
  sprintf (buf, "conn  bot");
} struct dcc_table DCC_BOT =
  { "BOT", DCT_BOT, eof_dcc_bot, dcc_bot, NULL, NULL, display_dcc_bot,
expmem_dcc_bot_, free_dcc_bot_, NULL };
struct dcc_table DCC_FORK_BOT =
  { "FORK_BOT", 0, failed_link, cont_link, &connect_timeout, failed_link,
display_dcc_fork_bot, expmem_dcc_bot_, free_dcc_bot_, NULL };
static void
dcc_chat_pass (int idx, char *buf, int atr)
{
  if (!atr)
    return;
  strip_telnet (dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;
  if (atr & USER_BOT)
    {
      if (!strcasecmp (buf, "elinkdone"))
	{
	  nfree (dcc[idx].u.chat);
	  dcc[idx].type = &DCC_BOT_NEW;
	  dcc[idx].u.bot = get_data_ptr (sizeof (struct bot_info));
	  dcc[idx].status = STAT_CALLED;
	  dprintf (idx, "goodbye!\n");
	  greet_new_bot (idx);
	  send_timesync (idx);
	}
      else
	{
	  putlog (LOG_MISC, "*", "%s failed encrypted link handshake",
		  dcc[idx].nick);
	  putlog (LOG_DEBUG, "*", "Expected elinkdone, got %s", buf);
	  killsock (dcc[idx].sock);
	  lostdcc (idx);
	}
      return;
    }
  if (u_pass_match (dcc[idx].user, buf))
    {
      putlog (LOG_MISC, "*", DCC_LOGGEDIN, dcc[idx].nick, dcc[idx].host,
	      dcc[idx].port);
      if (dcc[idx].u.chat->away)
	{
	  nfree (dcc[idx].u.chat->away);
	  dcc[idx].u.chat->away = NULL;
	}
      dcc[idx].type = &DCC_CHAT;
      dcc[idx].status &= ~STAT_CHAT;
      dcc[idx].u.chat->channel = -2;
      if (dcc[idx].status & STAT_TELNET)
	dprintf (idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
      stats_add (dcc[idx].user, 1, 0);
      dcc_chatter (idx);
    }
  else
    {
      dprintf (idx, "%s", rand_dccrespbye ());
      putlog (LOG_MISC, "*", DCC_BADLOGIN, dcc[idx].nick, dcc[idx].host,
	      dcc[idx].port);
      if (dcc[idx].u.chat->away)
	{
	  if (dcc[idx].status & STAT_TELNET)
	    dprintf (idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
	  dcc[idx].user =
	    get_user_by_handle (userlist, dcc[idx].u.chat->away);
	  strcpy (dcc[idx].nick, dcc[idx].u.chat->away);
	  nfree (dcc[idx].u.chat->away);
	  nfree (dcc[idx].u.chat->su_nick);
	  dcc[idx].u.chat->away = NULL;
	  dcc[idx].u.chat->su_nick = NULL;
	  dcc[idx].type = &DCC_CHAT;
	  if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	    botnet_send_join_idx (idx, -1);
	  chanout_but (-1, dcc[idx].u.chat->channel, DCC_JOIN, dcc[idx].nick);
	}
      else
	{
	  killsock (dcc[idx].sock);
	  lostdcc (idx);
	}
    }
}
static void
eof_dcc_general (int idx)
{
  putlog (LOG_MISC, "*", DCC_LOSTDCC, dcc[idx].nick, dcc[idx].host,
	  dcc[idx].port);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
tout_dcc_chat_pass (int idx)
{
  putlog (LOG_MISC, "*", DCC_PWDTIMEOUT, dcc[idx].nick, dcc[idx].host);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
display_dcc_chat_pass (int idx, char *buf)
{
  sprintf (buf, "pass  waited %lus", now - dcc[idx].timeval);
} static int
expmem_dcc_general (void *x)
{
  register struct chat_info *p = (struct chat_info *) x;
  int tot = sizeof (struct chat_info);
  if (p->away)
    tot += strlen (p->away) + 1;
  if (p->buffer)
    {
      struct msgq *q = p->buffer;
      while (q)
	{
	  tot += sizeof (struct list_type);
	  tot += q->len + 1;
	  q = q->next;
    }}
  if (p->su_nick)
    tot += strlen (p->su_nick) + 1;
  return tot;
}
static void
kill_dcc_general (int idx, void *x)
{
  register struct chat_info *p = (struct chat_info *) x;
  if (p)
    {
      if (p->buffer)
	{
	  struct msgq *r, *q;
	  for (r = dcc[idx].u.chat->buffer; r; r = q)
	    {
	      q = r->next;
	      nfree (r->msg);
	      nfree (r);
	    }
	}
      if (p->away)
	{
	  nfree (p->away);
	}
      nfree (p);
    }
}
static void
strip_mirc_codes (int flags, char *text)
{
  char *dd = text;
  while (*text)
    {
      switch (*text)
	{
	case 2:
	  if (flags & STRIP_BOLD)
	    {
	      text++;
	      continue;
	    }
	  break;
	case 3:
	  if (flags & STRIP_COLOR)
	    {
	      if (isdigit (text[1]))
		{
		  text += 2;
		  if (isdigit (*text))
		    text++;
		  if (*text == ',')
		    {
		      if (isdigit (text[1]))
			text += 2;
		      if (isdigit (*text))
			text++;
		    }
		}
	      else
		text++;
	      continue;
	    }
	  break;
	case 7:
	  if (flags & STRIP_BELLS)
	    {
	      text++;
	      continue;
	    }
	  break;
	case 0x16:
	  if (flags & STRIP_REV)
	    {
	      text++;
	      continue;
	    }
	  break;
	case 0x1f:
	  if (flags & STRIP_UNDER)
	    {
	      text++;
	      continue;
	    }
	  break;
	case 033:
	  if (flags & STRIP_ANSI)
	    {
	      text++;
	      if (*text == '[')
		{
		  text++;
		  while ((*text == ';') || isdigit (*text))
		    text++;
		  if (*text)
		    text++;
		}
	      continue;
	    }
	  break;
	}
      *dd++ = *text++;
    }
  *dd = 0;
}
static void
append_line (int idx, char *line)
{
  int l = strlen (line);
  struct msgq *p, *q;
  struct chat_info *c =
    (dcc[idx].type == &DCC_CHAT) ? dcc[idx].u.chat : dcc[idx].u.file->chat;
  if (c->current_lines > 1000)
    {
      for (p = c->buffer; p; p = q)
	{
	  q = p->next;
	  nfree (p->msg);
	  nfree (p);
	}
      c->buffer = 0;
      dcc[idx].status &= ~STAT_PAGE;
      do_boot (idx, botnetnick, "too many pages - senq full");
      return;
    }
  if ((c->line_count < c->max_line) && (c->buffer == NULL))
    {
      c->line_count++;
      tputs (dcc[idx].sock, line, l);
    }
  else
    {
      c->current_lines++;
      if (c->buffer == NULL)
	q = NULL;
      else
	for (q = c->buffer; q->next; q = q->next);
      p = get_data_ptr (sizeof (struct msgq));
      p->len = l;
      p->msg = get_data_ptr (l + 1);
      p->next = NULL;
      strcpy (p->msg, line);
      if (q == NULL)
	c->buffer = p;
      else
	q->next = p;
    }
}
static void
out_dcc_general (int idx, char *buf, void *x)
{
  register struct chat_info *p = (struct chat_info *) x;
  char *y = buf;
  strip_mirc_codes (p->strip_flags, buf);
  if (dcc[idx].status & STAT_TELNET)
    y = add_cr (buf);
  if (dcc[idx].status & STAT_PAGE)
    append_line (idx, y);
  else
    tputs (dcc[idx].sock, y, strlen (y));
}
struct dcc_table DCC_CHAT_PASS =
  { "CHAT_PASS", 0, eof_dcc_general, dcc_chat_pass, &password_timeout,
tout_dcc_chat_pass, display_dcc_chat_pass, expmem_dcc_general, kill_dcc_general, out_dcc_general };
static void
eof_dcc_chat (int idx)
{
  putlog (LOG_MISC, "*", DCC_LOSTDCC, dcc[idx].nick, dcc[idx].host,
	  dcc[idx].port);
  if (dcc[idx].u.chat->channel >= 0)
    {
      chanout_but (idx, dcc[idx].u.chat->channel, "*** %s lost dcc link.\n",
		   dcc[idx].nick);
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	botnet_send_part_idx (idx, "lost dcc link");
      check_tcl_chpt (botnetnick, dcc[idx].nick, dcc[idx].sock,
		      dcc[idx].u.chat->channel);
    }
  check_tcl_chof (dcc[idx].nick, dcc[idx].sock);
  killsock (dcc[idx].sock);
  lostdcc (idx);
}
static void
dcc_chat (int idx, char *buf, int i)
{
  int nathan = 0, fixed = 0;
  char *v, *d;
  strip_telnet (dcc[idx].sock, buf, &i);
  if (buf[0] && (buf[0] != '.')
      && detect_dcc_flood (&dcc[idx].timeval, dcc[idx].u.chat, idx))
    return;
  dcc[idx].timeval = now;
  if (buf[0])
    strcpy (buf, check_tcl_filt (idx, buf));
  if (buf[0])
    {
      v = buf;
      d = buf;
      while (*v)
	switch (*v)
	  {
	  case 7:
	    nathan++;
	    if (nathan > 3)
	      v++;
	    else
	      *d++ = *v++;
	    break;
	  case 8:
	    if (d > buf)
	      {
		d--;
	      }
	    v++;
	    break;
	  case '\r':
	    v++;
	    break;
	  default:
	    *d++ = *v++;
	  }
      if (fixed)
	strcpy (d, "\033[0m");
      else
	*d = 0;
      if (buf[0])
	{
	  if ((!strncmp (buf, dcc_prefix, strlen (dcc_prefix)))
	      || (dcc[idx].u.chat->channel < 0))
	    {
	      if (!strncmp (buf, dcc_prefix, strlen (dcc_prefix)))
		buf++;
	      v = newsplit (&buf);
	      rmspace (buf);
	      if (check_tcl_dcc (v, idx, buf))
		{
		  if (dcc[idx].u.chat->channel >= 0)
		    check_tcl_chpt (botnetnick, dcc[idx].nick, dcc[idx].sock,
				    dcc[idx].u.chat->channel);
		  check_tcl_chof (dcc[idx].nick, dcc[idx].sock);
		  flush_lines (idx, dcc[idx].u.chat);
		  putlog (LOG_MISC, "*", DCC_CLOSED, dcc[idx].nick,
			  dcc[idx].host);
		  if (dcc[idx].u.chat->channel >= 0)
		    {
		      chanout_but (-1, dcc[idx].u.chat->channel,
				   "*** %s left the party line%s%s\n",
				   dcc[idx].nick, buf[0] ? ": " : ".", buf);
		      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
			botnet_send_part_idx (idx, buf);
		    }
		  if (dcc[idx].u.chat->su_nick)
		    {
		      dcc[idx].user =
			get_user_by_handle (userlist,
					    dcc[idx].u.chat->su_nick);
		      strcpy (dcc[idx].nick, dcc[idx].u.chat->su_nick);
		      dcc[idx].type = &DCC_CHAT;
		      dprintf (idx, "Returning to real nick %s!\n",
			       dcc[idx].u.chat->su_nick);
		      nfree (dcc[idx].u.chat->su_nick);
		      dcc[idx].u.chat->su_nick = NULL;
		      dcc_chatter (idx);
		      if (dcc[idx].u.chat->channel < GLOBAL_CHANS
			  && dcc[idx].u.chat->channel >= 0)
			botnet_send_join_idx (idx, -1);
		      return;
		    }
		  else if ((dcc[idx].sock != STDOUT) || backgrd)
		    {
		      killsock (dcc[idx].sock);
		      lostdcc (idx);
		      return;
		    }
		  else
		    {
		      dprintf (DP_STDOUT, "\n### SIMULATION RESET\n\n");
		      dcc_chatter (idx);
		      return;
		    }
		}
	    }
	  else if (buf[0] == ',')
	    {
	      int me = 0;
	      if ((buf[1] == 'm') && (buf[2] == 'e') && buf[3] == ' ')
		me = 1;
	      for (i = 0; i < dcc_total; i++)
		{
		  int ok = 0;
		  if (dcc[i].type->flags & DCT_MASTER)
		    {
		      if ((dcc[i].type != &DCC_CHAT)
			  || (dcc[i].u.chat->channel >= 0))
			if ((i != idx) || (dcc[idx].status & STAT_ECHO))
			  ok = 1;
		    }
		  if (ok)
		    {
		      struct userrec *u =
			get_user_by_handle (userlist, dcc[i].nick);
		      if (u && (u->flags & USER_MASTER))
			{
			  if (me)
			    dprintf (i, "-> %s%s\n", dcc[idx].nick, buf + 3);
			  else
			    dprintf (i, "-%s-> %s\n", dcc[idx].nick, buf + 1);
			}
		    }
		}
	    }
	  else if (buf[0] == '\'')
	    {
	      int me = 0;
	      if ((buf[1] == 'm') && (buf[2] == 'e')
		  && ((buf[3] == ' ') || (buf[3] == '\'') || (buf[3] == ',')))
		me = 1;
	      for (i = 0; i < dcc_total; i++)
		{
		  if (dcc[i].type->flags & DCT_CHAT)
		    {
		      if (me)
			dprintf (i, "=> %s%s\n", dcc[idx].nick, buf + 3);
		      else
			dprintf (i, "=%s=> %s\n", dcc[idx].nick, buf + 1);
		    }
		}
	    }
	  else
	    {
	      if (dcc[idx].u.chat->away != NULL)
		not_away (idx);
	      if (dcc[idx].status & STAT_ECHO)
		chanout_but (-1, dcc[idx].u.chat->channel, "<%s> %s\n",
			     dcc[idx].nick, buf);
	      else
		chanout_but (idx, dcc[idx].u.chat->channel, "<%s> %s\n",
			     dcc[idx].nick, buf);
	      botnet_send_chan (-1, botnetnick, dcc[idx].nick,
				dcc[idx].u.chat->channel, buf);
	      check_tcl_chat (dcc[idx].nick, dcc[idx].u.chat->channel, buf);
	    }
	}
    }
  if (dcc[idx].type == &DCC_CHAT)
    if (dcc[idx].status & STAT_PAGE)
      flush_lines (idx, dcc[idx].u.chat);
}
static void
display_dcc_chat (int idx, char *buf)
{
  int i = simple_sprintf (buf, "chat  flags: ");
  buf[i++] = dcc[idx].status & STAT_CHAT ? 'C' : 'c';
  buf[i++] = dcc[idx].status & STAT_PARTY ? 'P' : 'p';
  buf[i++] = dcc[idx].status & STAT_TELNET ? 'T' : 't';
  buf[i++] = dcc[idx].status & STAT_ECHO ? 'E' : 'e';
  buf[i++] = dcc[idx].status & STAT_PAGE ? 'P' : 'p';
  buf[i++] = dcc[idx].status & STAT_COLORM ? 'M' : 'm';
  buf[i++] = dcc[idx].status & STAT_COLORA ? 'A' : 'a';
  simple_sprintf (buf + i, "/%d", dcc[idx].u.chat->channel);
} struct dcc_table DCC_CHAT =
  { "CHAT",
DCT_CHAT | DCT_MASTER | DCT_SHOWWHO | DCT_VALIDIDX | DCT_SIMUL | DCT_CANBOOT | DCT_REMOTEWHO,
eof_dcc_chat, dcc_chat, NULL, NULL, display_dcc_chat, expmem_dcc_general, kill_dcc_general,
out_dcc_general };
static int lasttelnets;
static char lasttelnethost[81];
static time_t lasttelnettime;
static int
detect_telnet_flood (char *floodhost)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  get_user_flagrec (get_user_by_host (floodhost), &fr, NULL);
  if (!flood_telnet_thr || (glob_friend (fr) && !par_telnet_flood))
    return 0;
  if (egg_strcasecmp (lasttelnethost, floodhost))
    {
      strcpy (lasttelnethost, floodhost);
      lasttelnettime = now;
      lasttelnets = 0;
      return 0;
    }
  if (lasttelnettime < now - flood_telnet_time)
    {
      lasttelnettime = now;
      lasttelnets = 0;
      return 0;
    }
  lasttelnets++;
  if (lasttelnets >= flood_telnet_thr)
    {
      lasttelnets = 0;
      lasttelnettime = 0;
      lasttelnethost[0] = 0;
      putlog (LOG_MISC, "*", IRC_TELNETFLOOD, floodhost);
      addignore (floodhost, origbotname, "Telnet connection flood",
		 now + (60 * ignore_time));
      return 1;
    }
  return 0;
}
static void
dcc_telnet (int idx, char *buf, int i)
{
  unsigned long ip;
  unsigned short port;
  int j = 0, sock;
  char s[UHOSTLEN + 1];
  if (dcc_total + 1 > max_dcc)
    {
      j = answer (dcc[idx].sock, s, &ip, &port, 0);
      if (j != -1)
	{
	  dprintf (-j, "Sorry, too many connections already.\r\n");
	  killsock (j);
	}
      return;
    }
  sock = answer (dcc[idx].sock, s, &ip, &port, 0);
  while ((sock == -1) && (errno == EAGAIN))
    sock = answer (sock, s, &ip, &port, 0);
  if (sock < 0)
    {
      neterror (s);
      putlog (LOG_MISC, "*", DCC_FAILED, s);
      return;
    }
  sockoptions (sock, EGG_OPTION_SET, SOCK_BUFFER);
  if (port < 1024 || port > 65535)
    {
      putlog (LOG_BOTS, "*", DCC_BADSRC, s, port);
      killsock (sock);
      return;
    }
  i = new_dcc (&DCC_DNSWAIT, sizeof (struct dns_info));
  dcc[i].sock = sock;
  dcc[i].addr = ip;
  dcc[i].port = port;
  dcc[i].timeval = now;
  strcpy (dcc[i].nick, "*");
  dcc[i].u.dns->ip = ip;
  dcc[i].u.dns->dns_success = dcc_telnet_hostresolved;
  dcc[i].u.dns->dns_failure = dcc_telnet_hostresolved;
  dcc[i].u.dns->dns_type = RES_HOSTBYIP;
  dcc[i].u.dns->ibuf = dcc[idx].sock;
  dcc[i].u.dns->type = &DCC_IDENTWAIT;
  dcc_dnshostbyip (ip);
} static void
dcc_telnet_hostresolved (int i)
{
  int idx;
  int j = 0, sock;
  char s[UHOSTLEN], s2[UHOSTLEN + 20];
  strncpyz (dcc[i].host, dcc[i].u.dns->host, UHOSTLEN);
  for (idx = 0; idx < dcc_total; idx++)
    if ((dcc[idx].type == &DCC_TELNET)
	&& (dcc[idx].sock == dcc[i].u.dns->ibuf))
      {
	break;
      }
  if (dcc_total == idx)
    {
      putlog (LOG_BOTS, "*", "Lost listening socket while resolving %s",
	      dcc[i].host);
      killsock (dcc[i].sock);
      lostdcc (i);
      return;
    }
  if (dcc[idx].host[0] == '@')
    {
      if (!wild_match (dcc[idx].host + 1, dcc[i].host))
	{
	  putlog (LOG_BOTS, "*", DCC_BADHOST, s);
	  killsock (dcc[i].sock);
	  lostdcc (i);
	  return;
	}
    }
  sprintf (s2, "-telnet!telnet@%s", dcc[i].host);
  if (match_ignore (s2) || detect_telnet_flood (s2))
    {
      killsock (dcc[i].sock);
      lostdcc (i);
      return;
    }
  changeover_dcc (i, &DCC_IDENTWAIT, 0);
  dcc[i].timeval = now;
  dcc[i].u.ident_sock = dcc[idx].sock;
  sock = open_telnet (iptostr (htonl (dcc[i].addr)), 113);
  putlog (LOG_MISC, "*", DCC_TELCONN, dcc[i].host, dcc[i].port);
  s[0] = 0;
  if (sock < 0)
    {
      if (sock == -2)
	strcpy (s, "DNS lookup failed for ident");
      else
	neterror (s);
    }
  else
    {
      j = new_dcc (&DCC_IDENT, 0);
      if (j < 0)
	{
	  killsock (sock);
	  strcpy (s, "No Free DCC's");
	}
    }
  if (s[0])
    {
      putlog (LOG_MISC, "*", DCC_IDENTFAIL, dcc[i].host, s);
      sprintf (s, "telnet@%s", dcc[i].host);
      dcc_telnet_got_ident (i, s);
      return;
    }
  dcc[j].sock = sock;
  dcc[j].port = 113;
  dcc[j].addr = dcc[i].addr;
  strcpy (dcc[j].host, dcc[i].host);
  strcpy (dcc[j].nick, "*");
  dcc[j].u.ident_sock = dcc[i].sock;
  dcc[j].timeval = now;
  dprintf (j, "%d, %d\n", dcc[i].port, dcc[idx].port);
}
static void
eof_dcc_telnet (int idx)
{
  putlog (LOG_MISC, "*", DCC_PORTDIE, dcc[idx].port);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
display_telnet (int idx, char *buf)
{
  sprintf (buf, "lstn  %d%s", dcc[idx].port,
	   (dcc[idx].status & LSTN_PUBLIC) ? " pub" : "");
} struct dcc_table DCC_TELNET =
  { "TELNET", DCT_LISTEN, eof_dcc_telnet, dcc_telnet, NULL, NULL,
display_telnet, NULL, NULL, NULL };
static void
eof_dcc_dupwait (int idx)
{
  putlog (LOG_BOTS, "*", DCC_LOSTDUP, dcc[idx].host);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
dcc_dupwait (int idx, char *buf, int i)
{
  return;
}
static void
timeout_dupwait (int idx)
{
  char x[100];
  if (in_chain (dcc[idx].nick))
    {
      egg_snprintf (x, sizeof x, "%s!%s", dcc[idx].nick, dcc[idx].host);
      dprintf (idx, "error Already connected.\n");
      putlog (LOG_BOTS, "*", DCC_DUPLICATE, x);
      killsock (dcc[idx].sock);
      lostdcc (idx);
    }
  else
    {
      dcc_telnet_pass (idx, dcc[idx].u.dupwait->atr);
    }
}
static void
display_dupwait (int idx, char *buf)
{
  sprintf (buf, "wait  duplicate?");
} static int
expmem_dupwait (void *x)
{
  register struct dupwait_info *p = (struct dupwait_info *) x;
  int tot = sizeof (struct dupwait_info);
  if (p && p->chat && DCC_CHAT.expmem)
    tot += DCC_CHAT.expmem (p->chat);
  return tot;
}
static void
kill_dupwait (int idx, void *x)
{
  register struct dupwait_info *p = (struct dupwait_info *) x;
  if (p)
    {
      if (p->chat && DCC_CHAT.kill)
	DCC_CHAT.kill (idx, p->chat);
      nfree (p);
    }
}
struct dcc_table DCC_DUPWAIT =
  { "DUPWAIT", DCT_VALIDIDX, eof_dcc_dupwait, dcc_dupwait, &dupwait_timeout,
timeout_dupwait, display_dupwait, expmem_dupwait, kill_dupwait, NULL };
void
dupwait_notify (char *who)
{
  register int idx;
  Assert (who);
  for (idx = 0; idx < dcc_total; idx++)
    if ((dcc[idx].type == &DCC_DUPWAIT)
	&& !egg_strcasecmp (dcc[idx].nick, who))
      {
	dcc_telnet_pass (idx, dcc[idx].u.dupwait->atr);
	break;
      }
}
static void
dcc_telnet_id (int idx, char *buf, int atr)
{
  int ok = 0;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  strip_telnet (dcc[idx].sock, buf, &atr);
  buf[HANDLEN] = 0;
  if ((dcc[idx].nick[0] != '@') && (!wild_match (dcc[idx].nick, buf)))
    {
      dprintf (idx, "Sorry, that nickname format is invalid.\n");
      putlog (LOG_BOTS, "*", DCC_BADNICK, dcc[idx].host);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  dcc[idx].user = get_user_by_handle (userlist, buf);
  get_user_flagrec (dcc[idx].user, &fr, NULL);
  if ((dcc[idx].status & STAT_BOTONLY) && !glob_bot (fr))
    {
      dprintf (idx, "This telnet port is for bots only.\n");
      putlog (LOG_BOTS, "*", DCC_NONBOT, dcc[idx].host);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  if ((dcc[idx].status & STAT_USRONLY) && glob_bot (fr))
    {
      dprintf (idx, "error Only users may connect at this port.\n");
      putlog (LOG_BOTS, "*", DCC_NONUSER, dcc[idx].host);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  dcc[idx].status &= ~(STAT_BOTONLY | STAT_USRONLY);
  ok = 1;
#ifdef HUB
  if (!glob_huba (fr))
    ok = 0;
#endif
#ifdef LEAF
  if (ischanhub () && !glob_chuba (fr))
    ok = 0;
  if (issechub () && !glob_huba (fr))
    ok = 0;
  if (!ischanhub () && !issechub ())
    ok = 0;
#endif
  if (!ok && glob_bot (fr))
    ok = 1;
  if (!ok)
    {
      putlog (LOG_BOTS, "*", DCC_INVHANDLE, dcc[idx].host, buf);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  correct_handle (buf);
  strcpy (dcc[idx].nick, buf);
  if (glob_bot (fr))
    {
      if (!egg_strcasecmp (botnetnick, dcc[idx].nick))
	{
	  dprintf (idx, "error You cannot link using my botnetnick.\n");
	  putlog (LOG_BOTS, "*", DCC_MYBOTNETNICK, dcc[idx].host);
	  killsock (dcc[idx].sock);
	  lostdcc (idx);
	  return;
	}
      else if (in_chain (dcc[idx].nick))
	{
	  struct chat_info *ci;
	  ci = dcc[idx].u.chat;
	  dcc[idx].type = &DCC_DUPWAIT;
	  dcc[idx].u.dupwait = get_data_ptr (sizeof (struct dupwait_info));
	  dcc[idx].u.dupwait->chat = ci;
	  dcc[idx].u.dupwait->atr = atr;
	  return;
	}
    }
  dcc_telnet_pass (idx, atr);
}
static void
dcc_telnet_pass (int idx, int atr)
{
  int ok = 0, i, ok2;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  get_user_flagrec (dcc[idx].user, &fr, NULL);
  if (!glob_bot (fr) && (u_pass_match (dcc[idx].user, "-")))
    {
      dprintf (idx, "Can't telnet until you have a password set.\r\n");
      putlog (LOG_MISC, "*", DCC_NOPASS, dcc[idx].nick, dcc[idx].host);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return;
    }
  ok = 0;
  if (dcc[idx].type == &DCC_DUPWAIT)
    {
      struct chat_info *ci;
      ci = dcc[idx].u.dupwait->chat;
      nfree (dcc[idx].u.dupwait);
      dcc[idx].u.chat = ci;
    }
  dcc[idx].type = &DCC_CHAT_PASS;
  dcc[idx].timeval = now;
  ok2 = 1;
#ifdef HUB
  if (!glob_huba (fr))
    ok2 = 0;
#endif
#ifdef LEAF
  if (issechub () && !glob_huba (fr))
    ok2 = 0;
  if (ischanhub () && !glob_chuba (fr))
    ok2 = 0;
#endif
  if (ok2)
    {
      ok = 1;
      dcc[idx].status |= STAT_PARTY;
    }
  if (glob_bot (fr))
    ok = 1;
  if (!ok)
    {
      struct chat_info *ci;
      ci = dcc[idx].u.chat;
      dcc[idx].u.file = get_data_ptr (sizeof (struct file_info));
      dcc[idx].u.file->chat = ci;
    }
  if (glob_bot (fr))
    {
      int snum = -1;
      for (i = 0; i < MAXSOCKS; i++)
	{
	  if (!(socklist[i].flags & SOCK_UNUSED)
	      && (socklist[i].sock == dcc[idx].sock))
	    {
	      snum = i;
	      break;
	    }
	}
      if (snum >= 0)
	{
	  char initkey[17], *tmp2;
	  char tmp[256];
	  MD5_CTX ctx;
	  sprintf (tmp, "%8x@%4x@%s@%s", htonl (dcc[idx].addr),
		   htons (dcc[idx].port), botnetnick, dcc[idx].nick);
	  MD5_Init (&ctx);
	  MD5_Update (&ctx, tmp, strlen (tmp));
	  MD5_Final (socklist[snum].okey, &ctx);
	  *(dword *) & initkey[0] = rand ();
	  *(dword *) & initkey[4] = rand ();
	  *(dword *) & initkey[8] = rand ();
	  *(dword *) & initkey[12] = rand ();
	  for (i = 0; i <= 15; i++)
	    {
	      if (!socklist[snum].okey[i])
		socklist[snum].okey[i] = 1;
	      if (!initkey[i])
		initkey[i] = 1;
	    }
	  socklist[snum].okey[16] = 0;
	  socklist[snum].oseed = rand ();
	  socklist[snum].iseed = socklist[snum].oseed;
	  initkey[16] = 0;
	  tmp2 = encrypt_string (netpass, initkey);
	  putlog (LOG_BOTS, "*", "Sending encrypted link handshake to %s...",
		  dcc[idx].nick);
	  socklist[snum].encstatus = 1;
	  dprintf (idx, "elink %s %lu\n", tmp2, socklist[snum].oseed);
	  strcpy (socklist[snum].okey, initkey);
	  strcpy (socklist[snum].ikey, initkey);
	  nfree (tmp2);
	}
      else
	{
	  putlog (LOG_MISC, "*",
		  "Couldn't find socket for %s connection?? Shouldn't happen :/",
		  dcc[idx].nick);
	  killsock (dcc[idx].sock);
	  lostdcc (idx);
	}
    }
  else
    {
#ifdef HUB
      dprintf (idx, "\n%s" TLN_IAC_C TLN_WILL_C TLN_ECHO_C "\n",
	       DCC_ENTERPASS);
#else
      dprintf (idx, "%s" TLN_IAC_C TLN_WILL_C TLN_ECHO_C,
	       rand_dccresppass ());
#endif
    }
}
static void
eof_dcc_telnet_id (int idx)
{
  putlog (LOG_MISC, "*", DCC_LOSTCON, dcc[idx].host, dcc[idx].port);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
timeout_dcc_telnet_id (int idx)
{
  putlog (LOG_MISC, "*", DCC_TTIMEOUT, dcc[idx].host);
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
display_dcc_telnet_id (int idx, char *buf)
{
  sprintf (buf, "t-in  waited %lus", now - dcc[idx].timeval);
} struct dcc_table DCC_TELNET_ID =
  { "TELNET_ID", 0, eof_dcc_telnet_id, dcc_telnet_id, &password_timeout,
timeout_dcc_telnet_id, display_dcc_telnet_id, expmem_dcc_general, kill_dcc_general, out_dcc_general };
static int
call_tcl_func (char *name, int idx, char *args)
{
  char s[11];
  sprintf (s, "%d", idx);
  Tcl_SetVar (interp, "_n", s, 0);
  Tcl_SetVar (interp, "_a", args, 0);
  if (Tcl_VarEval (interp, name, " $_n $_a", NULL) == TCL_ERROR)
    {
      putlog (LOG_MISC, "*", DCC_TCLERROR, name, interp->result);
      return -1;
    }
  return (atoi (interp->result));
}
static void
dcc_script (int idx, char *buf, int len)
{
  long oldsock;
  strip_telnet (dcc[idx].sock, buf, &len);
  if (!len)
    return;
  dcc[idx].timeval = now;
  oldsock = dcc[idx].sock;
  if (call_tcl_func (dcc[idx].u.script->command, dcc[idx].sock, buf))
    {
      void *old_other = NULL;
      if (dcc[idx].sock != oldsock || idx > max_dcc)
	return;
      old_other = dcc[idx].u.script->u.other;
      dcc[idx].type = dcc[idx].u.script->type;
      nfree (dcc[idx].u.script);
      dcc[idx].u.other = old_other;
      if (dcc[idx].type == &DCC_SOCKET)
	{
	  killsock (dcc[idx].sock);
	  lostdcc (idx);
	  return;
	}
      if (dcc[idx].type == &DCC_CHAT)
	{
	  if (dcc[idx].u.chat->channel >= 0)
	    {
	      chanout_but (-1, dcc[idx].u.chat->channel, DCC_JOIN,
			   dcc[idx].nick);
	      if (dcc[idx].u.chat->channel < 10000)
		botnet_send_join_idx (idx, -1);
	      check_tcl_chjn (botnetnick, dcc[idx].nick,
			      dcc[idx].u.chat->channel, geticon (idx),
			      dcc[idx].sock, dcc[idx].host);
	    }
	  check_tcl_chon (dcc[idx].nick, dcc[idx].sock);
	}
    }
}
static void
eof_dcc_script (int idx)
{
  void *old;
  int oldflags;
  Context;
  oldflags = dcc[idx].type->flags;
  dcc[idx].type->flags &= ~(DCT_VALIDIDX);
  call_tcl_func (dcc[idx].u.script->command, dcc[idx].sock, "");
  dcc[idx].type->flags = oldflags;
  Context;
  old = dcc[idx].u.script->u.other;
  dcc[idx].type = dcc[idx].u.script->type;
  nfree (dcc[idx].u.script);
  dcc[idx].u.other = old;
  if (dcc[idx].type && dcc[idx].type->eof)
    dcc[idx].type->eof (idx);
  else
    {
      putlog (LOG_DEBUG, "*",
	      "*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED",
	      dcc[idx].sock, dcc[idx].type->name);
      killsock (dcc[idx].sock);
      lostdcc (idx);
    }
}
static void
display_dcc_script (int idx, char *buf)
{
  sprintf (buf, "scri  %s", dcc[idx].u.script->command);
} static int
expmem_dcc_script (void *x)
{
  register struct script_info *p = (struct script_info *) x;
  int tot = sizeof (struct script_info);
  if (p->type && p->u.other)
    tot += p->type->expmem (p->u.other);
  return tot;
}
static void
kill_dcc_script (int idx, void *x)
{
  register struct script_info *p = (struct script_info *) x;
  if (p->type && p->u.other)
    p->type->kill (idx, p->u.other);
  nfree (p);
}
static void
out_dcc_script (int idx, char *buf, void *x)
{
  register struct script_info *p = (struct script_info *) x;
  if (p && p->type && p->u.other)
    p->type->output (idx, buf, p->u.other);
  else
    tputs (dcc[idx].sock, buf, strlen (buf));
}
struct dcc_table DCC_SCRIPT =
  { "SCRIPT", DCT_VALIDIDX, eof_dcc_script, dcc_script, NULL, NULL,
display_dcc_script, expmem_dcc_script, kill_dcc_script, out_dcc_script };
static void
dcc_socket (int idx, char *buf, int len)
{
} static void
eof_dcc_socket (int idx)
{
  killsock (dcc[idx].sock);
  lostdcc (idx);
} static void
display_dcc_socket (int idx, char *buf)
{
  strcpy (buf, "sock  (stranded)");
} struct dcc_table DCC_SOCKET =
  { "SOCKET", DCT_VALIDIDX, eof_dcc_socket, dcc_socket, NULL, NULL,
display_dcc_socket, NULL, NULL, NULL };
static void
display_dcc_lost (int idx, char *buf)
{
  strcpy (buf, "lost");
} struct dcc_table DCC_LOST =
  { "LOST", 0, NULL, dcc_socket, NULL, NULL, display_dcc_lost, NULL, NULL,
NULL };
void
dcc_identwait (int idx, char *buf, int len)
{
} void
eof_dcc_identwait (int idx)
{
  int i;
  putlog (LOG_MISC, "*", DCC_LOSTCONN, dcc[idx].host, dcc[idx].port);
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENT) && (dcc[i].u.ident_sock == dcc[idx].sock))
      {
	killsock (dcc[i].sock);
	dcc[i].u.other = 0;
	lostdcc (i);
	break;
      }
  killsock (dcc[idx].sock);
  dcc[idx].u.other = 0;
  lostdcc (idx);
}
static void
display_dcc_identwait (int idx, char *buf)
{
  sprintf (buf, "idtw  waited %lus", now - dcc[idx].timeval);
} struct dcc_table DCC_IDENTWAIT =
  { "IDENTWAIT", 0, eof_dcc_identwait, dcc_identwait, NULL, NULL,
display_dcc_identwait, NULL, NULL, NULL };
void
dcc_ident (int idx, char *buf, int len)
{
  char response[512], uid[512], buf1[UHOSTLEN];
  int i;
  sscanf (buf, "%*[^:]:%[^:]:%*[^:]:%[^\n]\n", response, uid);
  rmspace (response);
  if (response[0] != 'U')
    {
      dcc[idx].timeval = now;
      return;
    }
  rmspace (uid);
  uid[20] = 0;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENTWAIT)
	&& (dcc[i].sock == dcc[idx].u.ident_sock))
      {
	simple_sprintf (buf1, "%s@%s", uid, dcc[idx].host);
	dcc_telnet_got_ident (i, buf1);
      }
  dcc[idx].u.other = 0;
  killsock (dcc[idx].sock);
  lostdcc (idx);
}

void
eof_dcc_ident (int idx)
{
  char buf[UHOSTLEN];
  int i;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENTWAIT)
	&& (dcc[i].sock == dcc[idx].u.ident_sock))
      {
	putlog (LOG_MISC, "*", DCC_EOFIDENT);
	simple_sprintf (buf, "telnet@%s", dcc[idx].host);
	dcc_telnet_got_ident (i, buf);
      }
  killsock (dcc[idx].sock);
  dcc[idx].u.other = 0;
  lostdcc (idx);
}
static void
display_dcc_ident (int idx, char *buf)
{
  sprintf (buf, "idnt  (sock %d)", dcc[idx].u.ident_sock);
} struct dcc_table DCC_IDENT =
  { "IDENT", 0, eof_dcc_ident, dcc_ident, &identtimeout, eof_dcc_ident,
display_dcc_ident, NULL, NULL, NULL };
void
dcc_telnet_got_ident (int i, char *host)
{
  int idx;
  char x[1024];
  for (idx = 0; idx < dcc_total; idx++)
    if ((dcc[idx].type == &DCC_TELNET)
	&& (dcc[idx].sock == dcc[i].u.ident_sock))
      break;
  dcc[i].u.other = 0;
  if (dcc_total == idx)
    {
      putlog (LOG_MISC, "*", DCC_LOSTIDENT);
      killsock (dcc[i].sock);
      lostdcc (i);
      return;
    }
  strncpyz (dcc[i].host, host, UHOSTLEN);
  egg_snprintf (x, sizeof x, "-telnet!%s", dcc[i].host);
  if (protect_telnet)
    {
      struct userrec *u;
      int ok = 1;
      u = get_user_by_host (x);
      if (!u)
	ok = 0;
#ifdef HUB
      if (ok && !(u->flags & USER_HUBA))
	ok = 0;
#endif
#ifdef LEAF
      if (ok && (ischanhub () && !(u->flags & USER_CHUBA)))
	ok = 0;
      if (ok && (issechub () && !(u->flags & USER_HUBA)))
	ok = 0;
#endif
      if (!ok && u && (u->flags & USER_BOT))
	ok = 1;
      if (!ok && (dcc[idx].status & LSTN_PUBLIC))
	ok = 1;
      if (!ok)
	{
	  putlog (LOG_MISC, "*", DCC_NOACCESS, dcc[i].host);
	  killsock (dcc[i].sock);
	  lostdcc (i);
	  return;
	}
    }
  if (match_ignore (x))
    {
      killsock (dcc[i].sock);
      lostdcc (i);
      return;
    }
  if (!strcmp (dcc[idx].nick, "(script)"))
    {
      dcc[i].type = &DCC_SOCKET;
      dcc[i].u.other = NULL;
      strcpy (dcc[i].nick, "*");
      check_tcl_listen (dcc[idx].host, dcc[i].sock);
      return;
    }
  sockoptions (dcc[i].sock, EGG_OPTION_UNSET, SOCK_BUFFER);
  dcc[i].type = &DCC_TELNET_ID;
  dcc[i].u.chat = get_data_ptr (sizeof (struct chat_info));
  egg_bzero (dcc[i].u.chat, sizeof (struct chat_info));
  dcc[i].status = STAT_TELNET | STAT_ECHO;
  if (!strcmp (dcc[idx].nick, "(bots)"))
    dcc[i].status |= STAT_BOTONLY;
  if (!strcmp (dcc[idx].nick, "(users)"))
    dcc[i].status |= STAT_USRONLY;
  strncpyz (dcc[i].nick, dcc[idx].host, HANDLEN);
  dcc[i].timeval = now;
  strcpy (dcc[i].u.chat->con_chan, chanset ? chanset->name : "*");
#ifdef HUB
  dprintf (i, "\n");
#else
  dprintf (i, "%s", rand_dccresp ());
#endif
}

int
listen_all (int lport, int off)
{
  int i, idx = (-1), port, realport;
  struct portmap *pmap = NULL, *pold = NULL;
  Context;
  port = realport = lport;
  for (pmap = root; pmap; pold = pmap, pmap = pmap->next)
    if (pmap->realport == port)
      {
	port = pmap->mappedto;
	break;
      }
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_TELNET) && (dcc[i].port == port))
      idx = i;
  if (off)
    {
      if (pmap)
	{
	  if (pold)
	    pold->next = pmap->next;
	  else
	    root = pmap->next;
	  nfree (pmap);
	}
      if (idx < 0)
	{
	  putlog (LOG_ERRORS, "*", STR ("No such listening port open - %d"),
		  lport);
	  return idx;
	}
      putlog (LOG_DEBUG, "*", "Closing listening port %d", dcc[idx].port);
      killsock (dcc[idx].sock);
      lostdcc (idx);
      return idx;
    }
  if (idx < 0)
    {
      if (dcc_total >= max_dcc)
	{
	  putlog (LOG_ERRORS, "*",
		  STR ("Can't open listening port - no more DCC Slots"));
	}
      else
	{
	  i = open_listen (&port);
	  if (i < 0)
	    putlog (LOG_ERRORS, "*",
		    STR ("Can't open listening port - it's taken"));
	  else
	    {
	      idx = new_dcc (&DCC_TELNET, 0);
	      dcc[idx].addr = iptolong (getmyip ());
	      dcc[idx].port = port;
	      dcc[idx].sock = i;
	      dcc[idx].timeval = now;
	      strcpy (dcc[idx].nick, STR ("(telnet)"));
	      strcpy (dcc[idx].host, "*");
	      if (!pmap)
		{
		  pmap = nmalloc (sizeof (struct portmap));
		  pmap->next = root;
		  root = pmap;
		}
	      pmap->realport = realport;
	      pmap->mappedto = port;
	      putlog (LOG_DEBUG, "*", STR ("Listening at telnet port %d"),
		      port);
    }}}
  return idx;
}
