/* 
 * misc.c -- handles:
 *   split() maskhost() dumplots() daysago() days() daysdur()
 *   queueing output for the bot (msg and help)
 *   resync buffers for sharebots
 *   motd display and %var substitution
 *
 */

#include "main.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "modules.h"
#include <pwd.h>
#include <errno.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif /* S_ANTITRACE */
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/utsname.h>
#include "stat.h"

extern struct userrec 	*userlist;
extern struct dcc_t	*dcc;
extern struct chanset_t	*chanset;
extern tand_t 		*tandbot;

extern char		 version[], origbotname[], botname[],
			 admin[], network[], motdfile[], ver[], botnetnick[],
			 bannerfile[], textdir[], userfile[], dcc_prefix[],
                         *binname, pid_file[], tempdir[], *owneremail;

extern int		 backgrd, con_chan, term_z, use_stderr, dcc_total, timesync,  
#ifdef HUB
                         my_port,
#endif
			 strict_host, loading,
                         localhub;
extern time_t		 now;
extern Tcl_Interp	*interp;
extern struct cfg_entry	CFG_MOTD, CFG_LOGIN, CFG_BADPROCESS, CFG_PROCESSLIST, CFG_PROMISC, 
			CFG_TRACE, CFG_HIJACK;

void detected(int, char *);

int	 shtime = 1;		/* Whether or not to display the time
				   with console output */
int	 conmask = LOG_MODES | LOG_CMDS | LOG_MISC; /* Console mask */
int	 debug_output = 1;	/* Disply output to server to LOG_SERVEROUT */
int 	 server_lag = 0;	/* GUESS! */


/* Expected memory usage
 */
int expmem_misc()
{
  int tot = 0;

  tot += strlen(binname) + 1;

  return tot;
}

void init_misc()
{
}


/*
 *    Misc functions
 */

/* low-level stuff for other modules
 */

/*	  This implementation wont overrun dst - 'max' is the max bytes that dst
 *	can be, including the null terminator. So if 'dst' is a 128 byte buffer,
 *	pass 128 as 'max'. The function will _always_ null-terminate 'dst'.
 *
 *	Returns: The number of characters appended to 'dst'.
 *
 *  Usage eg.
 *
 *		char 	buf[128];
 *		size_t	bufsize = sizeof(buf);
 *
 *		buf[0] = 0, bufsize--;
 *
 *		while (blah && bufsize) {
 *			bufsize -= egg_strcatn(buf, <some-long-string>, sizeof(buf));
 *		}
 *
 *	<Cybah>
 */
int egg_strcatn(char *dst, const char *src, size_t max)
{
  size_t tmpmax = 0;

  /* find end of 'dst' */
  while (*dst && max > 0) {
    dst++;
    max--;
  }

  /*    Store 'max', so we can use it to workout how many characters were
   *  written later on.
   */
  tmpmax = max;

  /* copy upto, but not including the null terminator */
  while (*src && max > 1) {
    *dst++ = *src++;
    max--;
  }

  /* null-terminate the buffer */
  *dst = 0;

  /*    Don't include the terminating null in our count, as it will cumulate
   *  in loops - causing a headache for the caller.
   */
  return tmpmax - max;
}

int my_strcpy(register char *a, register char *b)
{
  register char *c = b;

  while (*b)
    *a++ = *b++;
  *a = *b;
  return b - c;
}

/* Split first word off of rest and put it in first
 */
void splitc(char *first, char *rest, char divider)
{
  char *p = strchr(rest, divider);

  if (p == NULL) {
    if (first != rest && first)
      first[0] = 0;
    return;
  }
  *p = 0;
  if (first != NULL)
    strcpy(first, rest);
  if (first != rest)
    /*    In most circumstances, strcpy with src and dst being the same buffer
     *  can produce undefined results. We're safe here, as the src is
     *  guaranteed to be at least 2 bytes higher in memory than dest. <Cybah>
     */
    strcpy(rest, p + 1);
}

/*    As above, but lets you specify the 'max' number of bytes (EXCLUDING the
 * terminating null).
 *
 * Example of use:
 *
 * char buf[HANDLEN + 1];
 *
 * splitcn(buf, input, "@", HANDLEN);
 *
 * <Cybah>
 */
void splitcn(char *first, char *rest, char divider, size_t max)
{
  char *p = strchr(rest, divider);

  if (p == NULL) {
    if (first != rest && first)
      first[0] = 0;
    return;
  }
  *p = 0;
  if (first != NULL)
    strncpyz(first, rest, max);
  if (first != rest)
    /*    In most circumstances, strcpy with src and dst being the same buffer
     *  can produce undefined results. We're safe here, as the src is
     *  guaranteed to be at least 2 bytes higher in memory than dest. <Cybah>
     */
    strcpy(rest, p + 1);
}

char *splitnick(char **blah)
{
  char *p = strchr(*blah, '!'), *q = *blah;

  if (p) {
    *p = 0;
    *blah = p + 1;
    return q;
  }
  return "";
}

void remove_crlf(char **line)
{
  char *p;

  p = strchr(*line, '\n');
  if (p != NULL)
    *p = 0;
  p = strchr(*line, '\r');
  if (p != NULL)
    *p = 0;
}

char *newsplit(char **rest)
{
  register char *o, *r;

  if (!rest)
    return *rest = "";
  o = *rest;
  while (*o == ' ')
    o++;
  r = o;
  while (*o && (*o != ' '))
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

/* Convert "abc!user@a.b.host" into "*!user@*.b.host"
 * or "abc!user@1.2.3.4" into "*!user@1.2.3.*"
 * or "abc!user@0:0:0:0:0:ffff:1.2.3.4" into "*!user@0:0:0:0:0:ffff:1.2.3.*"
 * or "abc!user@3ffe:604:2:b02e:6174:7265:6964:6573" into
 *    "*!user@3ffe:604:2:b02e:6174:7265:6964:*"
 */
void maskhost(const char *s, char *nw)
{
  register const char *p, *q, *e, *f;
  int i;

  *nw++ = '*';
  *nw++ = '!';
  p = (q = strchr(s, '!')) ? q + 1 : s;
  /* Strip of any nick, if a username is found, use last 8 chars */
  if ((q = strchr(p, '@'))) {
    int fl = 0;

    if ((q - p) > 9) {
      nw[0] = '*';
      p = q - 7;
      i = 1;
    } else
      i = 0;
    while (*p != '@') {
      if (!fl && strchr("~+-^=", *p)) {
        if (strict_host)
	  nw[i] = '?';
	else
	  i--;
      } else
	nw[i] = *p;
      fl++;
      p++;
      i++;
    }
    nw[i++] = '@';
    q++;
  } else {
    nw[0] = '*';
    nw[1] = '@';
    i = 2;
    q = s;
  }
  nw += i;
  e = NULL;
  /* Now q points to the hostname, i point to where to put the mask */
  if ((!(p = strchr(q, '.')) || !(e = strchr(p + 1, '.'))) && !strchr(q, ':'))
    /* TLD or 2 part host */
    strcpy(nw, q);
  else {
    if (e == NULL) {		/* IPv6 address?		*/
      const char *mask_str;

      f = strrchr(q, ':');
      if (strchr(f, '.')) {	/* IPv4 wrapped in an IPv6?	*/
	f = strrchr(f, '.');
	mask_str = ".*";
      } else 			/* ... no, true IPv6.		*/
	mask_str = ":*";
      strncpy(nw, q, f - q);
      /* No need to nw[f-q] = 0 here, as the strcpy below will
       * terminate the string for us.
       */
      nw += (f - q);
      strcpy(nw, mask_str);
    } else {
      for (f = e; *f; f++);
      f--;
      if (*f >= '0' && *f <= '9') {	/* Numeric IP address */
	while (*f != '.')
	  f--;
	strncpy(nw, q, f - q);
	/* No need to nw[f-q] = 0 here, as the strcpy below will
	 * terminate the string for us.
	 */
	nw += (f - q);
	strcpy(nw, ".*");
      } else {				/* Normal host >= 3 parts */
	/*    a.b.c  -> *.b.c
	 *    a.b.c.d ->  *.b.c.d if tld is a country (2 chars)
	 *             OR   *.c.d if tld is com/edu/etc (3 chars)
	 *    a.b.c.d.e -> *.c.d.e   etc
	 */
	const char *x = strchr(e + 1, '.');

	if (!x)
	  x = p;
	else if (strchr(x + 1, '.'))
	  x = e;
	else if (strlen(x) == 3)
	  x = p;
	else
	  x = e;
	sprintf(nw, "*%s", x);
      }
    }
  }
}

/* Dump a potentially super-long string of text.
 */
void dumplots(int idx, const char *prefix, char *data)
{
  char		*p = data, *q, *n, c;
  const int	 max_data_len = 500 - strlen(prefix);

  if (!*data) {
    dprintf(idx, "%s\n", prefix);
    return;
  }
  while (strlen(p) > max_data_len) {
    q = p + max_data_len;
    /* Search for embedded linefeed first */
    n = strchr(p, '\n');
    if (n && n < q) {
      /* Great! dump that first line then start over */
      *n = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *n = '\n';
      p = n + 1;
    } else {
      /* Search backwards for the last space */
      while (*q != ' ' && q != p)
	q--;
      if (q == p)
	q = p + max_data_len;
      c = *q;
      *q = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *q = c;
      p = q;
      if (c == ' ')
	p++;
    }
  }
  /* Last trailing bit: split by linefeeds if possible */
  n = strchr(p, '\n');
  while (n) {
    *n = 0;
    dprintf(idx, "%s%s\n", prefix, p);
    *n = '\n';
    p = n + 1;
    n = strchr(p, '\n');
  }
  if (*p)
    dprintf(idx, "%s%s\n", prefix, p);	/* Last trailing bit */
}

/* Convert an interval (in seconds) to one of:
 * "19 days ago", "1 day ago", "18:12"
 */
void daysago(time_t now, time_t then, char *out)
{
  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "%d day%s ago", days, (days == 1) ? "" : "s");
    return;
  }
#ifdef S_UTCTIME
  egg_strftime(out, 6, "%H:%M", gmtime(&then));
#else /* !S_UTCTIME */
  egg_strftime(out, 6, "%H:%M", localtime(&then));
#endif /* S_UTCTIME */
}

/* Convert an interval (in seconds) to one of:
 * "in 19 days", "in 1 day", "at 18:12"
 */
void days(time_t now, time_t then, char *out)
{
  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "in %d day%s", days, (days == 1) ? "" : "s");
    return;
  }
#ifdef S_UTCTIME
  egg_strftime(out, 9, "at %H:%M", gmtime(&now));
#else /* !S_UTCTIME */
  egg_strftime(out, 9, "at %H:%M", localtime(&now));
#endif /* S_UTCTIME */
}

/* Convert an interval (in seconds) to one of:
 * "for 19 days", "for 1 day", "for 09:10"
 */
void daysdur(time_t now, time_t then, char *out)
{
  char s[81];
  int hrs, mins;

  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "for %d day%s", days, (days == 1) ? "" : "s");
    return;
  }
  strcpy(out, "for ");
  now -= then;
  hrs = (int) (now / 3600);
  mins = (int) ((now - (hrs * 3600)) / 60);
  sprintf(s, "%02d:%02d", hrs, mins);
  strcat(out, s);
}

/* show l33t banner */

char *wbanner() {
  int r;
  r = random();
  switch (r % 9) {
   case 0: return STR("                       .__  __  .__\n__  _  ______________  |__|/  |_|  |__\n\\ \\/ \\/ /\\_  __ \\__  \\ |  \\   __\\  |  \\\n \\     /  |  | \\// __ \\|  ||  | |   Y  \\\n  \\/\\_/   |__|  (____  /__||__| |___|  /\n                     \\/              \\/\n");
   case 1: return STR("                    _ _   _     \n__      ___ __ __ _(_) |_| |__  \n\\ \\ /\\ / / '__/ _` | | __| '_ \\ \n \\ V  V /| | | (_| | | |_| | | |\n  \\_/\\_/ |_|  \\__,_|_|\\__|_| |_|\n");
   case 2: return STR("@@@  @@@  @@@  @@@@@@@    @@@@@@   @@@  @@@@@@@  @@@  @@@\n@@@  @@@  @@@  @@@@@@@@  @@@@@@@@  @@@  @@@@@@@  @@@  @@@\n@@!  @@!  @@!  @@!  @@@  @@!  @@@  @@!    @@!    @@!  @@@\n!@!  !@!  !@!  !@!  @!@  !@!  @!@  !@!    !@!    !@!  @!@\n@!!  !!@  @!@  @!@!!@!   @!@!@!@!  !!@    @!!    @!@!@!@!\n!@!  !!!  !@!  !!@!@!    !!!@!!!!  !!!    !!!    !!!@!!!!\n!!:  !!:  !!:  !!: :!!   !!:  !!!  !!:    !!:    !!:  !!!\n:!:  :!:  :!:  :!:  !:!  :!:  !:!  :!:    :!:    :!:  !:!\n :::: :: :::   ::   :::  ::   :::   ::     ::    ::   :::\n  :: :  : :     :   : :   :   : :  :       :      :   : :\n");
   case 3: return STR("                                     o8o      .   oooo\n                                     `''    .o8   `888\noooo oooo    ooo oooo d8b  .oooo.   oooo  .o888oo  888 .oo.\n `88. `88.  .8'  `888''8P `P  )88b  `888    888    888P'Y88b\n  `88..]88..8'    888      .oP'888   888    888    888   888\n   `888'`888'     888     d8(  888   888    888 .  888   888\n    `8'  `8'     d888b    `Y888''8o o888o   '888' o888o o888o\n");
   case 4: return STR("                                                                   *\n                                                 *         *     **\n**                                              ***       **     **\n**                                               *        **     **\n **    ***    ****     ***  ****                        ******** **\n  **    ***     ***  *  **** **** *    ****    ***     ********  **  ***\n  **     ***     ****    **   ****    * ***  *  ***       **     ** * ***\n  **      **      **     **          *   ****    **       **     ***   ***\n  **      **      **     **         **    **     **       **     **     **\n  **      **      **     **         **    **     **       **     **     **\n  **      **      **     **         **    **     **       **     **     **\n  **      **      *      **         **    **     **       **     **     **\n   ******* *******       ***        **    **     **       **     **     **\n    *****   *****         ***        ***** **    *** *     **    **     **\n                                      ***   **    ***             **    **\n                                                                        *\n                                                                       *\n                                                                      *\n                                                                     *\n");
   case 5: return STR(" :::  ===  === :::====  :::====  ::: :::==== :::  ===\n :::  ===  === :::  === :::  === ::: :::==== :::  ===\n ===  ===  === =======  ======== ===   ===   ========\n  ===========  === ===  ===  === ===   ===   ===  ===\n   ==== ====   ===  === ===  === ===   ===   ===  ===\n");
   case 6: return STR(" _  _  _  ______ _______ _____ _______ _     _\n |  |  | |_____/ |_____|   |      |    |_____|\n |__|__| |    \\_ |     | __|__    |    |     |\n");
   case 7: return STR("     dBPdBPdBP dBBBBBb dBBBBBb     dBP dBBBBBBP dBP dBP\n                   dBP      BB\n   dBPdBPdBP   dBBBBK   dBP BB   dBP    dBP   dBBBBBP\n  dBPdBPdBP   dBP  BB  dBP  BB  dBP    dBP   dBP dBP\n dBBBBBBBP   dBP  dB' dBBBBBBB dBP    dBP   dBP dBP\n");
   case 8: return STR("                                                    /\n                                       #          #/\n                                      ###    #    ##\n##                                     #    ##    ##\n##                                          ##    ##\n ##    ###    ####  ###  /###   /### ###  ##########  /##\n  ##    ###     ###/ ###/ #### / ###/ ########### ## / ###\n  ##     ###     ###  ##   ###/   ###  ##   ##    ##/   ###\n  ##      ##      ##  ##     ##    ##  ##   ##    ##     ##\n  ##      ##      ##  ##     ##    ##  ##   ##    ##     ##\n  ##      ##      ##  ##     ##    ##  ##   ##    ##     ##\n  ##      ##      ##  ##     ##    ##  ##   ##    ##     ##\n  ##      /#      /   ##     ##    /#  ##   ##    ##     ##\n   ######/ ######/    ###     ####/ ## ### /##    ##     ##\n    #####   #####      ###     ###   ##/##/  ##    ##    ##\n                                                          /\n                                                         /\n                                                        /\n                                                       /");
  }
  return "";
}

void show_banner(int idx)
{
  dumplots(-dcc[idx].sock, "", wbanner()); /* we use sock so that colors aren't applied to banner */
  dprintf(idx, "\n \n");
  dprintf(idx, STR("info, bugs, suggestions, comments:\n- http://wraith.shatow.net/ -\n \n"));
}

/* show motd to dcc chatter */
void show_motd(int idx)
{
  
  if (CFG_MOTD.gdata && *(char *) CFG_MOTD.gdata) {
    char *who, *buf, date[50];
    time_t time;
    void *buf_ptr;
    buf = buf_ptr = nmalloc(strlen((char *) CFG_MOTD.gdata) + 1);
    strcpy(buf, (char *) CFG_MOTD.gdata);
    who = newsplit(&buf);
    time = atoi(newsplit(&buf));
#ifdef S_UTCTIME
    egg_strftime(date, sizeof date, "%c %Z", gmtime(&time));
#else /* !S_UTCTIME */
    egg_strftime(date, sizeof date, "%c %Z", localtime(&time));
#endif /* S_UTCTIME */
    dprintf(idx, "Motd set by \002%s\002 (%s)\n", who, date);
    dumplots(idx, "* ", replace(buf, "\\n", "\n"));
    dprintf(idx, " \n");
    nfree(buf_ptr);
  } else
    dprintf(idx, STR("Motd: none\n"));
}

void show_channels(int idx, char *handle)
{
  struct chanset_t *chan;
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };
  struct userrec *u;
  int first = 0, l = 0, total = 0;
  char format[120];
#ifdef LEAF
  module_entry *me = module_find("irc", 0, 0);
  Function *func = me->funcs;
#endif /* LEAF */

  if (handle)
    u = get_user_by_handle(userlist, handle);
  else
    u = dcc[idx].user;

  for (chan = chanset;chan;chan = chan->next) {
    get_user_flagrec(u, &fr, chan->dname);
    if (l < strlen(chan->dname)) {
      l = strlen(chan->dname);
    }
    if (chk_op(fr, chan))
      total++;
  }

  egg_snprintf(format, sizeof format, "  %%c%%-%us %%-s%%-s%%-s%%-s%%-s\n", (l+2));

  for (chan = chanset;chan;chan = chan->next) {
#ifdef LEAF
    int opped = (func[16] (chan));
#else /* !LEAF */
    int opped = 0;
#endif /* LEAF */
    get_user_flagrec(u, &fr, chan->dname);
    if (chk_op(fr, chan)) {
        if (!first) { 
          dprintf(idx, STR("%s %s access to %d channel%s:\n"), handle ? u->handle : "You", handle ? "has" : "have", total, (total > 1) ? "s" : "");
          
          first = 1;
        }
        dprintf(idx, format, opped ? '@' : ' ', chan->dname, !shouldjoin(chan) ? "(inactive) " : "", 
           channel_private(chan) ? "(private)  " : "", !channel_manop(chan) ? "(no manop) " : "", 
           channel_bitch(chan) ? "(bitch)    " : "", channel_closed(chan) ?  "(closed)" : "");
    }
  }
  if (!first)
    dprintf(idx, STR("%s %s not have access to any channels.\n"), handle ? u->handle : "You", handle ? "does" : "do");
}

int getting_users()
{
  int i;

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_BOT) && (dcc[i].status & STAT_GETTING))
      return 1;
  return 0;
}

/*
 *    Logging functions
 */

/* Log something
 * putlog(level,channel_name,format,...);
 */
void putlog EGG_VARARGS_DEF(int, arg1)
{
  int i, type, tsl = 0, dohl = 0; //hl
  char *format, *chname, s[LOGLINELEN], *out, stamp[34], buf2[LOGLINELEN]; 
  va_list va;
#ifdef HUB
  time_t now2 = time(NULL);
#endif /* HUB */
  struct tm *t;
#ifdef LEAF
  t = 0;
#endif /* LEAF */
  type = EGG_VARARGS_START(int, arg1, va);
  chname = va_arg(va, char *);
  format = va_arg(va, char *);
//The putlog should not be broadcast over bots, @ is *.
  if ((chname[0] == '*'))
    dohl = 1;
#ifdef HUB
#ifdef S_UTCTIME
  t = gmtime(&now2);
#else /* !S_UTCTIME */
  t = localtime(&now2);
#endif /* S_UTCTIME */
  if (shtime) {
    egg_strftime(stamp, sizeof(stamp) - 2, LOG_TS, t);
    strcat(stamp, " ");
   tsl = strlen(stamp);
  }
#endif /* HUB */
 

  /* Format log entry at offset 'tsl,' then i can prepend the timestamp */
  out = s+tsl;

  /* No need to check if out should be null-terminated here,
   * just do it! <cybah>
   */

  egg_vsnprintf(out, LOGLINEMAX - tsl, format, va);

  out[LOGLINEMAX - tsl] = 0;

  /* Place the timestamp in the string to be printed */
  if ((out[0]) && (shtime)) {
    strncpy(s, stamp, tsl);
    out = s;
  }

  strcat(out, "\n");
  /* WRITE LOG HERE */
/* echo line to hubs (not if it was on a +h though)*/

  if (dohl) {
    tand_t *bot;
    struct userrec *ubot;
    sprintf(buf2, "hl %d %s", type, out);
    if (userlist && !loading) {
      for (bot = tandbot ;bot ; bot = bot->next) {
        ubot = get_user_by_handle(userlist, bot->bot);
        if (ubot) {
          if (bot_hublevel(ubot) < 999) {
            putbot(ubot->handle, buf2);
          }
        }
      }
    } else {
      putallbots(buf2);
    }
  }

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_CHAT && !dcc[i].simul) && (dcc[i].u.chat->con_flags & type)) {
      if ((chname[0] == '@') || (chname[0] == '*') || (dcc[i].u.chat->con_chan[0] == '*') ||
	  (!rfc_casecmp(chname, dcc[i].u.chat->con_chan)))
	dprintf(i, "%s", out);
    }
  if ((!backgrd) && (!con_chan) && (!term_z))
    dprintf(DP_STDOUT, "%s", out);
  else if ((type & LOG_MISC) && use_stderr) {
    if (shtime)
      out += tsl;
    dprintf(DP_STDERR, "%s", s);
  }
  va_end(va);
}

char *extracthostname(char *hostmask)
{
  char *p = strrchr(hostmask, '@');
  return p ? p + 1 : "";
}

/* Create a string with random letters and digits
 */
void make_rand_str(char *s, int len)
{
  int j, r = 0;

  for (j = 0; j < len; j++) {
    r = random();
    if (r % 4 == 0)
      s[j] = '0' + (random() % 10);
    else if (r % 4 == 1)
      s[j] = 'a' + (random() % 26);
    else if (r % 4 == 2)
      s[j] = 'A' + (random() % 26);
    else
      s[j] = '!' + (random() % 15);

    if (s[j] == 33 || s[j] == 37 || s[j] == 34 || s[j] == 40 || s[j] == 41 || s[j] == 38 || s[j] == 36) //no % ( ) & 
      s[j] = 35;
  }

  s[len] = '\0';
}

/* Convert an octal string into a decimal integer value.  If the string
 * is empty or contains non-octal characters, -1 is returned.
 */
int oatoi(const char *octal)
{
  register int i;

  if (!*octal)
    return -1;
  for (i = 0; ((*octal >= '0') && (*octal <= '7')); octal++)
    i = (i * 8) + (*octal - '0');
  if (*octal)
    return -1;
  return i;
}

/* Return an allocated buffer which contains a copy of the string
 * 'str', with all 'div' characters escaped by 'mask'. 'mask'
 * characters are escaped too.
 *
 * Remember to free the returned memory block.
 */
char *str_escape(const char *str, const char div, const char mask)
{
  const int	 len = strlen(str);
  int		 buflen = (2 * len), blen = 0;
  char		*buf = nmalloc(buflen + 1), *b = buf;
  const char	*s;

  if (!buf)
    return NULL;
  for (s = str; *s; s++) {
    /* Resize buffer. */
    if ((buflen - blen) <= 3) {
      buflen = (buflen * 2);
      buf = nrealloc(buf, buflen + 1);
      if (!buf)
	return NULL;
      b = buf + blen;
    }

    if (*s == div || *s == mask) {
      sprintf(b, "%c%02x", mask, *s);
      b += 3;
      blen += 3;
    } else {
      *(b++) = *s;
      blen++;
    }
  }
  *b = 0;
  return buf;
}

/* Is every character in a string a digit? */
int str_isdigit(const char *str)
{
  if (!*str)
    return 0;

  for(; *str; ++str) {
    if (!egg_isdigit(*str))
      return 0;
  }
  return 1;
}


/* Search for a certain character 'div' in the string 'str', while
 * ignoring escaped characters prefixed with 'mask'.
 *
 * The string
 *
 *   "\\3a\\5c i am funny \\3a):further text\\5c):oink"
 *
 * as str, '\\' as mask and ':' as div would change the str buffer
 * to
 *
 *   ":\\ i am funny :)"
 *
 * and return a pointer to "further text\\5c):oink".
 *
 * NOTE: If you look carefully, you'll notice that strchr_unescape()
 *       behaves differently than strchr().
 */
char *strchr_unescape(char *str, const char div, register const char esc_char)
{
  char		 buf[3];
  register char	*s, *p;

  buf[3] = 0;
  for (s = p = str; *s; s++, p++) {
    if (*s == esc_char) {	/* Found escape character.		*/
      /* Convert code to character. */
      buf[0] = s[1], buf[1] = s[2];
      *p = (unsigned char) strtol(buf, NULL, 16);
      s += 2;
    } else if (*s == div) {
      *p = *s = 0;
      return (s + 1);		/* Found searched for character.	*/
    } else
      *p = *s;
  }
  *p = 0;
  return NULL;
}

/* As strchr_unescape(), but converts the complete string, without
 * searching for a specific delimiter character.
 */
void str_unescape(char *str, register const char esc_char)
{
  (void) strchr_unescape(str, 0, esc_char);
}

/* Kills the bot. s1 is the reason shown to other bots, 
 * s2 the reason shown on the partyline. (Sup 25Jul2001)
 */
void kill_bot(char *s1, char *s2)
{
#ifdef HUB
  write_userfile(-1);
#endif /* HUB */
  call_hook(HOOK_DIE);
  chatout("*** %s\n", s1);
  botnet_send_chat(-1, botnetnick, s1);
  botnet_send_bye();
  fatal(s2, 0);
}

int isupdatehub()
{
#ifdef HUB
  struct userrec *buser;
  buser = get_user_by_handle(userlist, botnetnick);
  if ((buser) && (buser->flags & USER_UPDATEHUB))
    return 1;
  else
#endif /* HUB */
    return 0;
}

int ischanhub()
{
  struct userrec *buser;
  buser = get_user_by_handle(userlist, botnetnick);
  if ((buser) && (buser->flags & USER_CHANHUB))
    return 1;
  else
    return 0;
}

int dovoice(struct chanset_t *chan)
{
  struct userrec *user = NULL;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0 };

  if (!chan) return 0;

  user = get_user_by_handle(userlist, botnetnick);
  get_user_flagrec(user, &fr, chan->dname);
  if (glob_dovoice(fr) || chan_dovoice(fr))
    return 1;
  return 0;
}

int dolimit(struct chanset_t *chan)
{
  struct userrec *user = NULL;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0 };

  if (!chan) return 0;

  user = get_user_by_handle(userlist, botnetnick);
  get_user_flagrec(user, &fr, chan->dname);
  if (glob_dolimit(fr) || chan_dolimit(fr))
    return 1;
  return 0;
}

#ifdef S_LASTCHECK
char last_buf[128]="";
#endif /* S_LASTCHECK */

void check_last() {
#ifdef S_LASTCHECK
  char user[20];
  struct passwd *pw;

  if (!strcmp((char *) CFG_LOGIN.ldata ? CFG_LOGIN.ldata : CFG_LOGIN.gdata ? CFG_LOGIN.gdata : "", STR("ignore")))
    return;

  pw = getpwuid(geteuid());
  if (!pw) return;

  strncpyz(user, pw->pw_name ? pw->pw_name : "" , sizeof(user));
  if (user[0]) {
    char *out;
    char buf[50];

    sprintf(buf, STR("last %s"), user);
    if (shell_exec(buf, NULL, &out, NULL)) {
      if (out) {
        char *p;

        p = strchr(out, '\n');
        if (p)
          *p = 0;
        if (strlen(out) > 10) {
          if (last_buf[0]) {
            if (strncmp(last_buf, out, sizeof(last_buf))) {
              char wrk[16384];

              sprintf(wrk, STR("Login: %s"), out);
              detected(DETECT_LOGIN, wrk);
            }
          }
          strncpyz(last_buf, out, sizeof(last_buf));
        }
        nfree(out);
      }
    }
  }
#endif /* S_LASTCHECK */
}

void check_processes()
{
#ifdef S_PROCESSCHECK
  char *proclist,
   *out,
   *p,
   *np,
   *curp,
    buf[1024],
    bin[128];

  if (!strcmp((char *) CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : "", STR("ignore")))
    return;

  proclist = (char *) (CFG_PROCESSLIST.ldata && ((char *) CFG_PROCESSLIST.ldata)[0] ?
                       CFG_PROCESSLIST.ldata : CFG_PROCESSLIST.gdata && ((char *) CFG_PROCESSLIST.gdata)[0] ? CFG_PROCESSLIST.gdata : NULL);
  if (!proclist)
    return;

  if (!shell_exec(STR("ps x"), NULL, &out, NULL))
    return;

  /* Get this binary's filename */
  strncpyz(buf, binname, sizeof(buf));
  p = strrchr(buf, '/');
  if (p) {
    p++;
    strncpyz(bin, p, sizeof(bin));
  } else {
    bin[0] = 0;
  }
  /* Fix up the "permitted processes" list */
  p = nmalloc(strlen(proclist) + strlen(bin) + 6);
  strcpy(p, proclist);
  strcat(p, " ");
  strcat(p, bin);
  strcat(p, " ");
  proclist = p;
  curp = out;
  while (curp) {
    np = strchr(curp, '\n');
    if (np)
      *np++ = 0;
    if (atoi(curp) > 0) {
      char *pid,
       *tty,
       *stat,
       *time,
        cmd[512],
        line[2048];

      strncpyz(line, curp, sizeof(line));
      /* it's a process line */
      /* Assuming format: pid tty stat time cmd */
      pid = newsplit(&curp);
      tty = newsplit(&curp);
      stat = newsplit(&curp);
      time = newsplit(&curp);
      strncpyz(cmd, curp, sizeof(cmd));
      /* skip any <defunct> procs "/bin/sh -c" crontab stuff and binname crontab stuff */
      if (!strstr(cmd, STR("<defunct>")) && !strncmp(cmd, STR("/bin/sh -c"), 10)
          && !strncmp(cmd, binname, strlen(binname))) {
        /* get rid of any args */
        if ((p = strchr(cmd, ' ')))
          *p = 0;
        /* remove [] or () */
        if (strlen(cmd)) {
          p = cmd + strlen(cmd) - 1;
          if (((cmd[0] == '(') && (*p == ')')) || ((cmd[0] == '[') && (*p == ']'))) {
            *p = 0;
            strcpy(buf, cmd + 1);
            strcpy(cmd, buf);
          }
        }

        /* remove path */
        if ((p = strrchr(cmd, '/'))) {
          p++;
          strcpy(buf, p);
          strcpy(cmd, buf);
        }

        /* skip "ps" */
        if (strcmp(cmd, "ps")) {
          /* see if proc's in permitted list */
          strcat(cmd, " ");
          if ((p = strstr(proclist, cmd))) {
            /* Remove from permitted list */
            while (*p != ' ')
              *p++ = 1;
          } else {
            char wrk[16384];

            sprintf(wrk, STR("Unexpected process: %s"), line);
            detected(DETECT_PROCESS, wrk);
          }
        }
      }
    }
    curp = np;
  }
  nfree(proclist);
  if (out)
    nfree(out);
#endif /* S_PROCESSCHECK */
}

void check_promisc()
{
#ifdef S_PROMISC
#ifdef SIOCGIFCONF
  char buf[8192];
  struct ifreq ifreq, *ifr;
  struct ifconf ifcnf;
  char *cp, *cplim;
  int sock;

  if (!strcmp((char *) CFG_PROMISC.ldata ? CFG_PROMISC.ldata : CFG_PROMISC.gdata ? CFG_PROMISC.gdata : "", STR("ignore")))
    return;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  ifcnf.ifc_len = 8191;
  ifcnf.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, (char *) &ifcnf) < 0) {
    close(sock);
    return;
  }
  ifr = ifcnf.ifc_req;
  cplim = buf + ifcnf.ifc_len;
  for (cp = buf; cp < cplim; cp += sizeof(ifr->ifr_name) + sizeof(ifr->ifr_addr)) {
    ifr = (struct ifreq *) cp;
    ifreq = *ifr;
    if (!ioctl(sock, SIOCGIFFLAGS, (char *) &ifreq)) {
      if (ifreq.ifr_flags & IFF_PROMISC) {
        close(sock);
        detected(DETECT_PROMISC, STR("Detected promiscuous mode"));
        return;
      }
    }
  }
  close(sock);
#endif /* SIOCGIFCONF */
#endif /* S_PROMISC */
}

#ifdef S_ANTITRACE
int traced = 0;

void got_trace(int z)
{
  traced = 0;
}
#endif /* S_ANTITRACE */

void check_trace(int n)
{
#ifdef S_ANTITRACE
  int x, parent, i;
  struct sigaction sv, *oldsv = NULL;

  if (n && !strcmp((char *) CFG_TRACE.ldata ? CFG_TRACE.ldata : CFG_TRACE.gdata ? CFG_TRACE.gdata : "", STR("ignore")))
    return;
  parent = getpid();
#ifdef __linux__
  egg_bzero(&sv, sizeof(sv));
  sv.sa_handler = got_trace;
  sigemptyset(&sv.sa_mask);
  oldsv = NULL;
  sigaction(SIGTRAP, &sv, oldsv);
  traced = 1;
  asm("INT3");
  sigaction(SIGTRAP, oldsv, NULL);
  if (traced)
    detected(DETECT_TRACE, STR("I'm being traced!"));
  else {
    x = fork();
    if (x == -1)
      return;
    else if (x == 0) {
      i = ptrace(PTRACE_ATTACH, parent, 0, 0);
      if (i == (-1) && errno == EPERM)
        detected(DETECT_TRACE, STR("I'm being traced!"));
      else {
        waitpid(parent, &i, 0);
        kill(parent, SIGCHLD);
        ptrace(PTRACE_DETACH, parent, 0, 0);
        kill(parent, SIGCHLD);
      }
      exit(0);
    } else
      wait(&i);
  }
#endif /* __linux__ */
#ifdef __FreeBSD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif /* __FreeBSD__ */
#ifdef __OpenBSD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif /* __OpenBSD__ */
#endif /* S_ANTITRACE */
}

int shell_exec(char *cmdline, char *input, char **output, char **erroutput)
{
  FILE *inpFile,
   *outFile,
   *errFile;
  char tmpfile[161];
  int x, fd;
  int parent = getpid();

  if (!cmdline)
    return 0;
  /* Set up temp files */
  /* always use mkstemp() when handling temp filess! -dizz */
  sprintf(tmpfile, STR("%s.in-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (inpFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*" , STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  if (input) {
    if (fwrite(input, 1, strlen(input), inpFile) != strlen(input)) {
      fclose(inpFile);
      putlog(LOG_ERRORS, "*", STR("exec: Couldn't write to '%s': %s"), tmpfile, strerror(errno));
      return 0;
    }
    fseek(inpFile, 0, SEEK_SET);
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.err-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (errFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.out-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (outFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  x = fork();
  if (x == -1) {
    putlog(LOG_ERRORS, "*", STR("exec: fork() failed: %s"), strerror(errno));
    fclose(inpFile);
    fclose(errFile);
    fclose(outFile);
    return 0;
  }
  if (x) {
    /* Parent: wait for the child to complete */
    int st = 0;

    waitpid(x, &st, 0);
    /* Now read the files into the buffers */
    fclose(inpFile);
    fflush(outFile);
    fflush(errFile);
    if (erroutput) {
      char *buf;
      int fs;

      fseek(errFile, 0, SEEK_END);
      fs = ftell(errFile);
      if (fs == 0) {
        (*erroutput) = NULL;
      } else {
        buf = nmalloc(fs + 1);
        fseek(errFile, 0, SEEK_SET);
        fread(buf, 1, fs, errFile);
        buf[fs] = 0;
        (*erroutput) = buf;
      }
    }
    fclose(errFile);
    if (output) {
      char *buf;
      int fs;

      fseek(outFile, 0, SEEK_END);
      fs = ftell(outFile);
      if (fs == 0) {
        (*output) = NULL;
      } else {
        buf = nmalloc(fs + 1);
        fseek(outFile, 0, SEEK_SET);
        fread(buf, 1, fs, outFile);
        buf[fs] = 0;
        (*output) = buf;
      }
    }
    fclose(outFile);
    return 1;
  } else {
    /* Child: make fd's and set them up as std* */
    int ind,
      outd,
      errd;
    char *argv[4];

    ind = fileno(inpFile);
    outd = fileno(outFile);
    errd = fileno(errFile);
    if (dup2(ind, STDIN_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    if (dup2(outd, STDOUT_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    if (dup2(errd, STDERR_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    argv[0] = STR("sh");
    argv[1] = STR("-c");
    argv[2] = cmdline;
    argv[3] = NULL;
    execvp(argv[0], &argv[0]);
    kill(parent, SIGCHLD);
    exit(1);
  }

}

/* Update system code
 */
int ucnt = 0;
void updatelocal(void)
{
#ifdef LEAF
  module_entry *me;
#endif /* LEAF */
  Context;
  if (ucnt < 300) {
    ucnt++;
    return;
  } 
  del_hook(HOOK_SECONDLY, (Function) updatelocal);
  ucnt = 0;

  /* let's drop the server connection ASAP */
#ifdef LEAF
  if ((me = module_find("server", 0, 0))) {
    Function *func = me->funcs;
    (func[SERVER_NUKESERVER]) ("Updating...");
  }
#endif /* LEAF */

  botnet_send_chat(-1, botnetnick, "Updating...");
  botnet_send_bye();

  fatal("Updating...", 1);
  usleep(2000 * 500);
  unlink(pid_file); //if this fails it is ok, cron will restart the bot, *hopefully*
  system(binname); //start new bot. 
  exit(0);
}

int updatebin (int idx, char *par, int autoi)
{
  char *path = NULL,
   *newbin;
  char buf[DIRMAX], old[DIRMAX], testbuf[DIRMAX];
  struct stat sb;
  int i;
#ifdef LEAF
  module_entry *me;
#endif /* LEAF */

  buf[0] = testbuf[0] = old[0] = 0;
  path = newsplit(&par);
  par = path;
  if (!par[0]) {
    if (idx)
      dprintf(idx, STR("Not enough parameters.\n"));
    return 1;
  }
  path = nmalloc(strlen(binname) + strlen(par));
  strcpy(path, binname);
  newbin = strrchr(path, '/');
  if (!newbin) {
    nfree(path);
    if (idx)
      dprintf(idx, STR("Don't know current binary name\n"));
    return 1;
  }
  newbin++;
  if (strchr(par, '/')) {
    *newbin = 0;
    if (idx)
      dprintf(idx, STR("New binary must be in %s and name must be specified without path information\n"), path);
    nfree(path);
    return 1;
  }
  strcpy(newbin, par);
  if (!strcmp(path, binname)) {
    nfree(path);
    if (idx)
      dprintf(idx, STR("Can't update with the current binary\n"));
    return 1;
  }
  if (stat(path, &sb)) {
    if (idx)
      dprintf(idx, STR("%s can't be accessed\n"), path);
    nfree(path);
    return 1;
  }
  if (chmod(path, S_IRUSR | S_IWUSR | S_IXUSR)) {
    if (idx)
      dprintf(idx, STR("Can't set mode 0600 on %s\n"), path);
    nfree(path);
    return 1;
  }

  //make a backup just in case.

  egg_snprintf(old, sizeof old, "%s.bin.old", tempdir);
  copyfile(binname, old);

  /* The binary should return '2' when ran with -2, if not it's probably corrupt. */
  snprintf(testbuf, sizeof testbuf, "%s -2", path);
  i = system(testbuf);
  if (i == -1 || WEXITSTATUS(i) != 2) {
    if (idx)
      dprintf(idx, STR("Couldn't restart new binary (error %d)\n"), i);
    putlog(LOG_MISC, "*", STR("Couldn't restart new binary (error %d)\n"), i);
    return i;
  }

  if (movefile(path, binname)) {
    if (idx)
      dprintf(idx, STR("Can't rename %s to %s\n"), path, binname);
    nfree(path);
    return 1;
  }

  sprintf(buf, "%s", binname);
#ifdef LEAF
  if (localhub) {
    /* if localhub = 1, this is the spawn bot and controls
     * the spawning of new bots. */
     sprintf(buf, "%s -L %s -P %d", buf, botnetnick, getpid());
  } 
#endif /* LEAF */

  //safe to run new binary..
#ifdef LEAF
  if (!autoi && !localhub) //dont delete pid for auto update!!!
#endif /* LEAF */
    unlink(pid_file); //delete pid so new binary doesnt exit.
#ifdef HUB
  listen_all(my_port, 1); //close the listening port...
  usleep(5000);
#endif /* HUB */
  putlog(LOG_DEBUG, "*", "Running for update: %s", buf);
#ifdef LEAF
  if (!autoi && localhub) {
    /* let's drop the server connection ASAP */
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("Updating...");
    }
#endif /* LEAF */
    if (idx)
      dprintf(idx, STR("Updating...bye\n"));
    putlog(LOG_MISC, "*", STR("Updating...\n"));
    botnet_send_chat(-1, botnetnick, "Updating...");
    botnet_send_bye();
    fatal("Updating...", 1);
    usleep(2000 * 500);
    system(buf);		/* run the binary, it SHOULD work from earlier tests.. */
    exit(0);
#ifdef LEAF
  } else if (localhub && autoi) {
    system(buf);
    add_hook(HOOK_SECONDLY, (Function) updatelocal);
    return 0;
  }
#endif /* LEAF */
 /* this should never be reached */
  return 2;
}

int bot_aggressive_to(struct userrec *u)
{
  char mypval[20],
    botpval[20];

  link_pref_val(u, botpval);
  link_pref_val(get_user_by_handle(userlist, botnetnick), mypval);

  if (strcmp(mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

void detected(int code, char *msg)
{
#ifdef LEAF
  module_entry *me;
#endif /* LEAF */
  char *p = NULL;
  char tmp[512];
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL, 0, 0 };
  int act;

  u = get_user_by_handle(userlist, botnetnick);
#ifdef S_LASTCHECK
  if (code == DETECT_LOGIN)
    p = (char *) (CFG_LOGIN.ldata ? CFG_LOGIN.ldata : (CFG_LOGIN.gdata ? CFG_LOGIN.gdata : NULL));
#endif /* S_LASTCHECK */
#ifdef S_ANTITRACE
  if (code == DETECT_TRACE)
    p = (char *) (CFG_TRACE.ldata ? CFG_TRACE.ldata : (CFG_TRACE.gdata ? CFG_TRACE.gdata : NULL));
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
  if (code == DETECT_PROMISC)
    p = (char *) (CFG_PROMISC.ldata ? CFG_PROMISC.ldata : (CFG_PROMISC.gdata ? CFG_PROMISC.gdata : NULL));
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
  if (code == DETECT_PROCESS)
    p = (char *) (CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : (CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : NULL));
#endif /* S_PROMISC */
#ifdef S_HIJACKCHECK
  if (code == DETECT_SIGCONT)
    p = (char *) (CFG_HIJACK.ldata ? CFG_HIJACK.ldata : (CFG_HIJACK.gdata ? CFG_HIJACK.gdata : NULL));
#endif /* S_PROMISC */

  if (!p)
    act = DET_WARN;
  else if (!strcmp(p, STR("die")))
    act = DET_DIE;
  else if (!strcmp(p, STR("reject")))
    act = DET_REJECT;
  else if (!strcmp(p, STR("suicide")))
    act = DET_SUICIDE;
  else if (!strcmp(p, STR("ignore")))
    act = DET_IGNORE;
  else
    act = DET_WARN;
  switch (act) {
  case DET_IGNORE:
    break;
  case DET_WARN:
    putlog(LOG_WARN, "*", msg);
    break;
  case DET_REJECT:
    do_fork();
    putlog(LOG_WARN, "*", STR("Setting myself +d: %s"), msg);
    sprintf(tmp, STR("+d: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
    get_user_flagrec(u, &fr, 0);
    fr.global = USER_DEOP | USER_BOT;

    set_user_flagrec(u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    putlog(LOG_WARN, "*", STR("Dying: %s"), msg);
    sprintf(tmp, STR("Dying: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("BBL");
    }
#endif /* LEAF */
    sleep(1);
    fatal(msg, 0);
    break;
  case DET_SUICIDE:
    putlog(LOG_WARN, "*", STR("Comitting suicide: %s"), msg);
    sprintf(tmp, STR("Suicide: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("HARAKIRI!!");
    }
#endif /* LEAF */
    sleep(1);
    unlink(binname);
#ifdef HUB
    unlink(userfile);
    sprintf(tmp, STR("%s~"), userfile);
    unlink(tmp);
#endif /* HUB */
    fatal(msg, 0);
    break;
  }
}

char kickprefix[25] = "";
char bankickprefix[25] = "";
char * kickreason(int kind) {
  int r;
  r=random();
  switch (kind) {
  case KICK_BANNED:
    switch (r % 6) {
    case 0: return STR("bye");
    case 1: return STR("banned");
    case 2: return STR("see you in hell");
    case 3: return STR("go away");
    case 4: return STR("cya around lewser");
    case 5: return STR("unwanted!");
    }
  case KICK_KUSER:
    switch (r % 4) {
    case 0: return STR("not wanted");
    case 1: return STR("something tells me you're annoying");
    case 2: return STR("don't bug me lewser");
    case 3: return STR("creep");
    }
  case KICK_KICKBAN:
    switch (r % 4) {
    case 0: return STR("gone");
    case 1: return STR("stupid");
    case 2: return STR("lewser");
    case 3: return STR("...");
    }     
  case KICK_MASSDEOP:
    switch (r % 8) {
    case 0: return STR("spammer!");
    case 1: return STR("easy on the modes now");
    case 2: return STR("mode this");
    case 3: return STR("nice try");
    case 4: return STR("really?");
    case 5: return STR("you lose");
    case 6: return STR("scary... really scary...");
    case 7: return STR("i win kthx");
    }
  case KICK_BADOP:
    switch (r % 5) {
    case 0: return STR("neat...");
    case 1: return STR("oh, no you don't. go away.");
    case 2: return STR("didn't you forget something now?");
    case 3: return STR("no");
    case 4: return STR("hijack this");
    }
  case KICK_BADOPPED:
    switch (r % 5) {
    case 0: return STR("fuck off kid");
    case 1: return STR("asl?");
    case 2: return STR("whoa... what a hacker... skills!");
    case 3: return STR("yes! yes! yes! hit me baby one more time!");
    case 4: return STR("with your skills, you're better off jacking off than hijacking");
    }
  case KICK_MANUALOP:
    switch (r % 6) {
    case 0: return STR("naughty kid");
    case 1: return STR("didn't someone tell you that is bad?");
    case 2: return STR("want perm?");
    case 3: return STR("see how much good that did you?");
    case 4: return STR("not a smart move...");
    case 5: return STR("jackass!");
    }
  case KICK_MANUALOPPED:
    switch (r % 8) {
    case 0: return STR("your pal got mean friends. like me.");
    case 1: return STR("uhh now.. don't wake me up...");
    case 2: return STR("hi hun. missed me?");
    case 3: return STR("spammer! die!");
    case 4: return STR("boo!");
    case 5: return STR("that @ was useful, don't ya think?");
    case 6: return STR("not in my book");
    case 7: return STR("lol, really?");
    }
  case KICK_CLOSED:
    switch (r % 17) {
    case 0: return STR("locked");
    case 1: return STR("later");
    case 2: return STR("closed for now");
    case 3: return STR("come back later");
    case 4: return STR("better safe than sorry");
    case 5: return STR("cleanup, come back later");
    case 6: return STR("this channel is closed");
    case 7: return STR("shutting down for now");
    case 8: return STR("lockdown");
    case 9: return STR("reopening later");
    case 10: return STR("not for the public atm");
    case 11: return STR("private channel for now");
    case 12: return STR("might reopen soon, might reopen later");
    case 13: return STR("you're not supposed to be here right now");
    case 14: return STR("sorry, closed");
    case 15: return STR("try us later, atm we're locked down");
    case 16: return STR("closed. try tomorrow");
    }
  case KICK_FLOOD:
    switch (r % 7) {
    case 0: return STR("so much bullshit in such a short time. amazing.");
    case 1: return STR("slow down. i'm trying to read here.");
    case 2: return STR("uhm... you actually think irc is for talking?");
    case 3: return STR("talk talk talk");
    case 4: return STR("blabbering are we?");
    case 5: return STR("... and i don't even like you!");
    case 6: return STR("and you're outa here...");

    }
  case KICK_NICKFLOOD:
    switch (r % 7) {
    case 0: return STR("make up your mind?");
    case 1: return STR("be schizofrenic elsewhere");
    case 2: return STR("I'm loosing track of you... not!");
    case 3: return STR("that is REALLY annoying");
    case 4: return STR("try this: /NICK n00b");
    case 5: return STR("playing hide 'n' seek?");
    case 6: return STR("gotcha!");
    }
  case KICK_KICKFLOOD:
    switch (r % 6) {
    case 0: return STR("easier to just leave if you wan't to be alone");
    case 1: return STR("cool down");
    case 2: return STR("don't be so damned aggressive. that's my job.");
    case 3: return STR("kicking's fun, isn't it?");
    case 4: return STR("what's the rush?");
    case 5: return STR("next time you do that, i'll kick you again");

    }
  case KICK_BOGUSUSERNAME:
    return STR("bogus username");
  case KICK_MEAN:
    switch (r % 11) {
    case 0: return STR("hey! that wasn't very nice!");
    case 1: return STR("don't fuck with my pals");
    case 2: return STR("meanie!");
    case 3: return STR("I can be a bitch too...");
    case 4: return STR("leave the bots alone, will ya?");
    case 5: return STR("not very clever");
    case 6: return STR("watch it");
    case 7: return STR("fuck off");
    case 8: return STR("easy now. that's a friend.");
    case 9: return STR("abuse of power. leave that to me, will ya?");      
    case 10: return STR("there as some things you cannot do, and that was one of them...");
    }
  case KICK_BOGUSKEY:
    return STR("I have a really hard time reading that key");
  default:
    return "OMFG@YUO";    
  }

}

void makeplaincookie(char *chname, char *nick, char *buf)
{
  /*
     plain cookie:
     Last 6 digits of time
     Last 5 chars of nick
     Last 4 regular chars of chan
   */
  char work[256],
    work2[256];
  int i,
    n;

  sprintf(work, STR("%010li"), (now + timesync));
  strcpy(buf, (char *) &work[4]);
  work[0] = 0;
  if (strlen(nick) < 5)
    while (strlen(work) + strlen(nick) < 5)
      strcat(work, " ");
  else
    strcpy(work, (char *) &nick[strlen(nick) - 5]);
  strcat(buf, work);

  n = 3;
  for (i = strlen(chname) - 1; (i >= 0) && (n >= 0); i--)
    if (((unsigned char) chname[i] < 128) && ((unsigned char) chname[i] > 32)) {
      work2[n] = tolower(chname[i]);
      n--;
    }
  while (n >= 0)
    work2[n--] = ' ';
  work2[4] = 0;
  strcat(buf, work2);
}

int goodpass(char *pass, int idx, char *nick)
{
  char *tell;
#ifdef S_NAZIPASS
  int i, nalpha = 0, lcase = 0, ucase = 0, ocase = 0, tc;
#endif /* S_NAZIPASS */
  if (!pass[0]) 
    return 0;

  tell = nmalloc(300);

#ifdef S_NAZIPASS
  for (i = 0; i < strlen(pass); i++) {
    tc = (int) pass[i];
    if (tc < 58 && tc > 47)
      ocase++; //number
    else if (tc < 91 && tc > 64)
      ucase++; //upper case
    else if (tc < 123 && tc > 96)
      lcase++; //lower case
    else
       nalpha++; //non-alphabet/number
  }

/*  if (ocase < 1 || lcase < 2 || ucase < 2 || nalpha < 1 || strlen(pass) < 8) { */
  if (ocase < 1 || lcase < 2 || ucase < 2 || strlen(pass) < 8) {
#else /* !S_NAZIPASS */
  if (strlen(pass) < 8) {
#endif /* S_NAZIPASS */

    sprintf(tell, "Insecure pass, must be: ");
#ifdef S_NAZIPASS 
    if (ocase < 1)
      strcat(tell, "\002>= 1 number\002, ");
    else
      strcat(tell, ">= 1 number, ");

    if (lcase < 2)
      strcat(tell, "\002>= 2 lcase\002, ");
    else
      strcat(tell, ">= 2 lowercase, ");

    if (ucase < 2)
      strcat(tell, "\002>= 2 ucase\002, ");
    else
      strcat(tell, ">= 2 uppercase, ");
/* This is annoying as hell
    if (nalpha < 1)
      strcat(tell, "\002>= 1 non-alpha/num\002, ");
    else
      strcat(tell, ">= 1 non-alpha/num, ");
*/
#endif /* S_NAZIPASS */
    if (strlen(pass) < 8)
      strcat(tell, "\002>= 8 chars.\002");
    else
      strcat(tell, ">= 8 chars.");

    if (idx)
      dprintf(idx, "%s\n", tell);
    else if (nick[0])
      dprintf(DP_HELP, STR("NOTICE %s :%s\n"), nick, tell);
    nfree(tell);
    return 0;
  }
  nfree(tell);
  return 1;
}

char *replace (char *string, char *oldie, char *newbie)
{
  static char newstring[1024] = "";
  int str_index, newstr_index, oldie_index, end, new_len, old_len, cpy_len;
  char *c;
  if (string == NULL) return "";
  if ((c = (char *) strstr(string, oldie)) == NULL) return string;
  new_len = strlen(newbie);
  old_len = strlen(oldie);
  end = strlen(string) - old_len;
  oldie_index = c - string;
  newstr_index = 0;
  str_index = 0;
  while(str_index <= end && c != NULL) {
    cpy_len = oldie_index-str_index;
    strncpy(newstring + newstr_index, string + str_index, cpy_len);
    newstr_index += cpy_len;
    str_index += cpy_len;
    strcpy(newstring + newstr_index, newbie);
    newstr_index += new_len;
    str_index += old_len;
    if((c = (char *) strstr(string + str_index, oldie)) != NULL)
     oldie_index = c - string;
  }
  strcpy(newstring + newstr_index, string + str_index);
  return (newstring);
}

char *getfullbinname(char *argv0)
{
  char *cwd, *bin, *p, *p2;

  bin = nmalloc(strlen(argv0) + 1);
  strcpy(bin, argv0);
  if (bin[0] == '/') {
    return bin;
  }
  cwd = nmalloc(8192);
  getcwd(cwd, 8191);
  cwd[8191] = 0;
  if (cwd[strlen(cwd) - 1] == '/')
    cwd[strlen(cwd) - 1] = 0;

  p = bin;
  p2 = strchr(p, '/');
  while (p) {
    if (p2)
      *p2++ = 0;
    if (!strcmp(p, "..")) {
      p = strrchr(cwd, '/');
      if (p)
        *p = 0;
    } else if (strcmp(p, ".")) {
      strcat(cwd, "/");
      strcat(cwd, p);
    }
    p = p2;
    if (p)
      p2 = strchr(p, '/');
  }
  nfree(bin);
  bin = nmalloc(strlen(cwd) + 1);
  strcpy(bin, cwd);
  nfree(cwd);
  return bin;
}

char *werr_tostr(int errnum)
{
  switch (errnum) {
  case ERR_BINSTAT:
    return STR("Cannot access binary");
  case ERR_BINMOD:
    return STR("Cannot chmod() binary");
  case ERR_PASSWD:
    return STR("Cannot access the global passwd file");
  case ERR_WRONGBINDIR:
    return STR("Wrong directory/binary name");
  case ERR_CONFSTAT:
#ifdef LEAF
    return STR("Cannot access config directory (~/.ssh/)");
#else
    return STR("Cannot access config directory (./)");
#endif /* LEAF */
  case ERR_TMPSTAT:
#ifdef LEAF
    return STR("Cannot access tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot access config directory (./tmp/)");
#endif /* LEAF */
  case ERR_CONFDIRMOD:
#ifdef LEAF
    return STR("Cannot chmod() config directory (~/.ssh/)");
#else
    return STR("Cannot chmod() config directory (./)");
#endif /* LEAF */
  case ERR_CONFMOD:
#ifdef LEAF
    return STR("Cannot chmod() config (~/.ssh/.known_hosts/)");
#else
    return STR("Cannot chmod() config (./conf)");
#endif /* LEAF */
  case ERR_TMPMOD:
#ifdef LEAF
    return STR("Cannot chmod() tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot chmod() tmp directory (./tmp)");
#endif /* LEAF */
  case ERR_NOCONF:
#ifdef LEAF
    return STR("The local config is missing (~/.ssh/.known_hosts)");
#else
    return STR("The local config is missing (./conf)");
#endif /* LEAF */
  case ERR_CONFBADENC:
    return STR("Encryption in config is wrong/corrupt");
  case ERR_WRONGUID:
    return STR("UID in conf does not match getuid()");
  case ERR_WRONGUNAME:
    return STR("Uname in conf does not match uname()");
  case ERR_BADCONF:
    return STR("Config file is incomplete");
  default:
    return STR("Unforseen error");
  }

}

void werr(int errnum)
{
  putlog(LOG_MISC, "*", STR("error #%d"), errnum);
  sdprintf(STR("error translates to: %s"), werr_tostr(errnum));
  printf(STR("(segmentation fault)\n"));
  fatal("", 0);
}


/* private returns 0 if user has access, and 1 if they dont because of +private 
 * This function does not check if the user has "op" access, it only checks if the user is
 * restricted by +private for the channel
 */
int private(struct flag_record fr, struct chanset_t *chan, int type)
{
  if (!channel_private(chan) || glob_bot(fr) || glob_owner(fr))
    return 0; /* user is implicitly not restricted by +private, they may however be lacking other flags */

  if (type == PRIV_OP) {
    /* |o implies all flags above. n| has access to all +private. Bots are exempt. */
    if (chan_op(fr))
      return 0;
  } else if (type == PRIV_VOICE) {
    if (chan_voice(fr))
      return 0;
  }
  return 1; /* user is restricted by +private */
}

int chk_op(struct flag_record fr, struct chanset_t *chan)
{
  if (!chan || (!private(fr, chan, PRIV_OP) && !chk_deop(fr, chan))) {
    if (chan_op(fr) || (glob_op(fr) && !chan_deop(fr)))
      return 1;
  }
  return 0;
}

int chk_deop(struct flag_record fr, struct chanset_t *chan)
{
  if (chan_deop(fr) || (glob_deop(fr) && !chan_op(fr)))
    return 1;
  else
    return 0;
}

int chk_voice(struct flag_record fr, struct chanset_t *chan)
{
  if (!chan || (!private(fr, chan, PRIV_VOICE) && !chk_devoice(fr, chan))) {
    if (chan_voice(fr) || (glob_voice(fr) && !chan_quiet(fr)))
      return 1;
  }
  return 0;
}

int chk_devoice(struct flag_record fr, struct chanset_t *chan)
{
  if (chan_quiet(fr) || (glob_quiet(fr) && !chan_voice(fr)))
    return 1;
  else
    return 0;
}

void local_check_should_lock()
{
  module_entry *me;
  if ((me = module_find("channels", 0, 0))) {
    Function *func = me->funcs;
    /* check_should_lock() */
    (func[51]) ();
  }
}

/* convert binary hashes to hex */
char *btoh(const unsigned char *md, int len)
{
  int i;
  char buf[100], *ret;

  for (i = 0; i < len; i++)
    sprintf(&(buf[i*2]), "%02x", md[i]);
  ret = buf;
  return ret;
}

#define HELP_BOLD  1
#define HELP_REV   2
#define HELP_UNDER 4
#define HELP_FLASH 8

/* so many string++ is making the problem */
void showhelp (int idx, struct flag_record *flags, char *string)
{
  static int help_flags;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  char helpstr[12288], tmp[2] = "", flagstr[10] = "";
  int ok = 1;
  helpstr[0] = 0;
  while (string && string[0]) {
    if (*string == '%') {
      if (!strncmp(string + 1, "{+", 2)) {
        while (*string && *string != '+') {
          string++;
        }
        flagstr[0] = 0;
        while (*string && *string != '}') {
          sprintf(tmp, "%c", *string);
          strcat(flagstr, tmp);
          string++;
        }
        string++;
        break_down_flags(flagstr, &fr, NULL);
        if (flagrec_ok(&fr, flags)) {
          ok = 1;
          while (*string && *string != '%') {
            sprintf(tmp, "%c", *string);
            strcat(helpstr, tmp);
            string++;
          }
          if (!strncmp(string + 1, "{-", 2)) {
            ok = 1;
            while (*string && *string != '}') {
              string++;
            }
            string++;
          }
        } else {
          ok = 0;
        }
      } else if (!strncmp(string + 1, "{-", 2)) {
        ok = 1;
        while (*string && *string != '}') {
          string++;
        }
        string++;
      } else if (*string == '{') {
        while (*string && *string != '}') {
          string++;
        }
      } else if (*(string + 1) == 'b') {
        string += 2;
        if (help_flags & HELP_BOLD) {
          help_flags &= ~HELP_BOLD;
          strcat(helpstr, color(idx, BOLD_CLOSE, 0));
        } else {
          help_flags |= HELP_BOLD;
          strcat(helpstr, color(idx, BOLD_OPEN, 0));
        }
      } else if (*(string + 1) == 'f') {
        string += 2;
        
        if (help_flags & HELP_FLASH) {
          strcat(helpstr, color(idx, FLASH_CLOSE, 0));
          help_flags &= ~HELP_FLASH;
        } else {
          help_flags |= HELP_FLASH;
          strcat(helpstr, color(idx, FLASH_OPEN, 0));
        }
      } else if (*(string + 1) == 'd') {
        string += 2;
        strcat(helpstr, dcc_prefix);        
      } else if (*(string + 1) == '%') {
        string += 2;
        strcat(helpstr, "%");        
      } else {
        if (ok) {
          sprintf(tmp, "%c", *string);
          strcat(helpstr, tmp);
        }
        string++;
      }
    } else {
      if (ok) {
        sprintf(tmp, "%c", *string);
        strcat(helpstr, tmp);
      }
      string++;
    }
  }
  helpstr[strlen(helpstr)] = 0;
  if (helpstr[0]) dumplots(idx, "", helpstr);
}

/* Arrange the N elements of ARRAY in random order. */
static void shuffleArray(char *array[], int n)
{
  int i;
  for (i = 0; i < n; i++) {
    int j = i + random() / (RAND_MAX / (n - i) + 1);
    char *t = array[j];
    array[j] = array[i];
    array[i] = t;
  }
}

void shuffle(char *string, char *delim)
{
  char *array[501], *str, *work;
  int len = 0, i = 0;

  work = nmalloc(strlen(string) + 1);
  strcpy(work, string);

  str = strtok(work, delim);
  while(str && *str)
  {
    array[len] = str;
    len++;
    str = strtok((char*) NULL, delim);
  }
  shuffleArray(array, len);
  string[0] = 0;
  for (i = 0; i < len; i++) {
    strcat(string, array[i]);
    if (i != len - 1)
      strcat(string, delim);
  }
  nfree(work);
  string[strlen(string)] = 0;
}

char *color(int idx, int type, int color)
{
  int ansi = 0;
   
  /* if user is connected over TELNET or !backgrd, show ANSI
   * if they are relaying, they are most likely on an IRC client and should have mIRC codes
   */
  if ((idx && ((dcc[idx].type != &DCC_RELAYING) && (dcc[idx].status & STAT_TELNET))) || !backgrd) ansi++;

  if (type == BOLD_OPEN) {
    return ansi ? "\033[1m" : "\002";
  } else if (type == BOLD_CLOSE) {
    return ansi ? "\033[22m" : "\002";
  } else if (type == UNDERLINE_OPEN) {
    return ansi ? "\033[4m" : "\037";
  } else if (type == UNDERLINE_CLOSE) {
    return ansi ? "\033[24m" : "\037";
  } else if (type == FLASH_OPEN) {
    return ansi ? "\033[5m" : "\002\037";
  } else if (type == FLASH_CLOSE) {
    return ansi ? "\033[0m" : "\037\002";
  } else if (type == COLOR_OPEN) {
    switch (color) {
      case C_BLACK: 		return ansi ? "\033[30m"   : "\00301";
      case C_RED: 		return ansi ? "\033[31m"   : "\00305";
      case C_GREEN: 		return ansi ? "\033[32m"   : "\00303";
      case C_BROWN: 		return ansi ? "\033[33m"   : "\00307";
      case C_BLUE: 		return ansi ? "\033[34m"   : "\00302";
      case C_PURPLE: 		return ansi ? "\033[35m"   : "\00306";
      case C_CYAN: 		return ansi ? "\033[36m"   : "\00310";
      case C_WHITE:	 	return ansi ? "\033[1;37m" : "\00300";
      case C_DARKGREY:		return ansi ? "\033[1;30m" : "\00314";
      case C_LIGHTRED:  	return ansi ? "\033[1;31m" : "\00304";
      case C_LIGHTGREEN: 	return ansi ? "\033[1;32m" : "\00309";
      case C_LIGHTBLUE:		return ansi ? "\033[1;34m" : "\00312";
      case C_LIGHTPURPLE: 	return ansi ? "\033[1;35m" : "\00313";
      case C_LIGHTCYAN:		return ansi ? "\033[1;36m" : "\00311";
      case C_LIGHTGREY:		return ansi ? "\033[37m"   : "\00315";
      case C_YELLOW:		return ansi ? "\033[1;33m" : "\00308";
      default: break;
    }
  } else if (type == COLOR_CLOSE) {
    return ansi ? "\033[0m" : "\00300";
  } 
  /* This should never be reached.. */
  return "";
}

int email(char *subject, char *msg, int who)
{
  struct utsname un;
  char open[2048], addrs[1024];
  int mail = 0, sendmail = 0;
  FILE *f;
  
  uname(&un);
  if (is_file("/usr/sbin/sendmail"))
    sendmail++;
  else if (is_file("/usr/bin/mail"))
    mail++;
  else {
    putlog(LOG_WARN, "*", "I Have no usable mail client.");
    return 1;
  }
  open[0] = addrs[0] = 0;

  if (who & EMAIL_OWNERS) {
    sprintf(addrs, "%s", replace(owneremail, ",", " "));
  }
  if (who & EMAIL_TEAM) {
    if (addrs[0])
      sprintf(addrs, "%s wraith@shatow.net", addrs);
    else 
      sprintf(addrs, "wraith@shatow.net");
  }

  if (sendmail)
    sprintf(open, "/usr/sbin/sendmail -t");
  else if (mail)
    sprintf(open, "/usr/bin/mail %s -a \"From: %s@%s\" -s \"%s\" -a \"Content-Type: text/plain\"", addrs, (origbotname && origbotname[0]) ? origbotname : "none", un.nodename, subject);
  if ((f = popen(open, "w"))) {
    if (sendmail) {
      struct passwd *pw;
      pw = getpwuid(geteuid());
      fprintf(f, "To: %s\n", addrs);
      fprintf(f, "From: %s@%s\n", (origbotname && origbotname[0]) ? origbotname : pw->pw_name, un.nodename);
      fprintf(f, "Subject: %s\n", subject);
      fprintf(f, "Content-Type: text/plain\n");
    }
    fprintf(f, "%s\n", msg);
    if (fflush(f))
      return 1;
    if (pclose(f))
      return 1;
  } else
    return 1;
  return 0;
}

void baduname(char *conf, char *my_uname) {
  char *tmpfile = nmalloc(strlen(tempdir) + 3 + 1);
  int send = 0;

  tmpfile[0] = 0;
  sprintf(tmpfile, "%s.un", tempdir);
  sdprintf("CHECKING %s", tmpfile);
  if (is_file(tmpfile)) {
    struct stat ss;
    stat(tmpfile, &ss);
    time_t diff = now - ss.st_mtime;
    if (diff >= 86400) send++;		/* only send once a day */
  } else {
    FILE *fp;
    if ((fp = fopen(tmpfile, "w"))) {
      fprintf(fp, "\n");
      fflush(fp);
      fclose(fp);
      send++;		/* only send if we could write the file. */
    } 
  }
  if (send) {
    struct passwd *pw;
    struct utsname un;
    char msg[501], subject[31];

    pw = getpwuid(geteuid());
    if (!pw) return;
    uname(&un);
    egg_snprintf(subject, sizeof subject, "CONF/UNAME() mismatch notice");
    egg_snprintf(msg, sizeof msg, "This is an auto email from a wraith bot which has you in it's OWNER_EMAIL list..\n \nThe uname() output on this box has changed, probably due to a kernel upgrade...\nMy login is: %s\nConf   : %s\nUname(): %s\n \nThis email will only be sent once a day while this error is present.\nYou need to login to my shell (%s) and fix my local config.\n", pw->pw_name, conf, my_uname, un.nodename);
    email(subject, msg, EMAIL_OWNERS);
  }
  nfree(tmpfile);
}


int whois_access(struct userrec *user, struct userrec *whois_user) 
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct flag_record whois = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
 
  get_user_flagrec(user, &fr, NULL);
  get_user_flagrec(whois_user, &whois, NULL);

  if ((glob_master(whois) && !glob_master(fr)) ||
     (glob_owner(whois) && !glob_owner(fr)) ||
     (glob_admin(whois) && !glob_admin(fr)) ||
     (glob_bot(whois) && !glob_master(fr)))
    return 0;
  return 1;
}
