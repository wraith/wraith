/* 
 * misc.c -- handles:
 *   split() maskhost() dumplots() daysago() days() daysdur()
 *   queueing output for the bot (msg and help)
 *   resync buffers for sharebots
 *   motd display and %var substitution
 *
 */

#include "common.h"
#include "misc.h"
#include "settings.h"
#include "binary.h"
#include "rfc1459.h"
#include "botnet.h"
#include "misc_file.h"
#include "egg_timer.h"
#include "dcc.h"
#include "users.h"
#include "main.h"
#include "debug.h"
#include "dccutil.h"
#include "chanprog.h"
#include "color.h"
#include "botmsg.h"
#include "bg.h"	
#include "chan.h"
#include "tandem.h"
#ifdef LEAF
#include "src/mod/server.mod/server.h"
#include "src/mod/irc.mod/irc.h"
#endif /* LEAF */
#include "userrec.h"
#include "stat.h"

#include <sys/wait.h>
#include <stdarg.h>

int		server_lag = 0;	/* GUESS! */
bool		use_invites = 1;            /* Jason/drummer */
bool		use_exempts = 1;            /* Jason/drummer */

/*
 *    Misc functions
 */

/* low-level stuff for other modules
 */

size_t my_strcpy(register char *a, register char *b)
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
  char *p = NULL, *q = *blah;

  p = strchr(*blah, '!');

  if (p) {
    *p = 0;
    *blah = p + 1;
    return q;
  }
  return "";
}

void remove_crlf(char *line)
{
  char *p = NULL;

  if ((p = strchr(line, '\n')))
    *p = 0;
  if ((p = strchr(line, '\r')))
    *p = 0;
}

char *newsplit(char **rest)
{
  if (!rest)
    return *rest = "";

  register char *o = *rest, *r = NULL;

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
  if (!*data) {
    dprintf(idx, "%s\n", prefix);
    return;
  }

  char *p = data, *q = NULL, *n = NULL, c = 0;
  const size_t max_data_len = 500 - strlen(prefix);

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
void daysago(time_t mynow, time_t then, char *out)
{
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    sprintf(out, "%d day%s ago", mydays, (mydays == 1) ? "" : "s");
    return;
  }
  egg_strftime(out, 6, "%H:%M", gmtime(&then));
}

/* Convert an interval (in seconds) to one of:
 * "in 19 days", "in 1 day", "at 18:12"
 */
void days(time_t mynow, time_t then, char *out)
{
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    sprintf(out, "in %d day%s", mydays, (mydays == 1) ? "" : "s");
    return;
  }
  egg_strftime(out, 9, "at %H:%M", gmtime(&now));
}

/* Convert an interval (in seconds) to one of:
 * "for 19 days", "for 1 day", "for 09:10"
 */
void daysdur(time_t mynow, time_t then, char *out)
{
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    sprintf(out, "for %d day%s", mydays, (mydays == 1) ? "" : "s");
    return;
  }

  char s[81] = "";
  int hrs, mins;

  strcpy(out, "for ");
  mynow -= then;
  hrs = (int) (mynow / 3600);
  mins = (int) ((mynow - (hrs * 3600)) / 60);
  sprintf(s, "%02d:%02d", hrs, mins);
  strcat(out, s);
}

/* show l33t banner */
static char *fuckyou = " _   _ _      __            _                            ____\n| | | (_)    / _|_   _  ___| | __  _   _  ___  _   _   _|  _ \\\n| |_| | |   | |_| | | |/ __| |/ / | | | |/ _ \\| | | | (_) | | |\n|  _  | |_  |  _| |_| | (__|   <  | |_| | (_) | |_| |  _| |_| |\n|_| |_|_( ) |_|  \\__,_|\\___|_|\\_\\  \\__, |\\___/ \\__,_| (_)____/\n        |/                         |___/\n";
char *wbanner(void) {
  if (fuckyou) { ; } /* gcc warnings */

  switch (randint(9)) {
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
  /* we use sock so that colors aren't applied to banner */
  if (dcc[idx].status & STAT_BANNER)
    dumplots(-dcc[idx].sock, "", wbanner()); 
  dprintf(idx, " \n");
  dprintf(-dcc[idx].sock,     " ------------------------------------------------------- \n");
  dprintf(-dcc[idx].sock, STR("| Contributions welcomed by paypal: bryan@shatow.net |\n"));
  dprintf(-dcc[idx].sock, STR("|             - http://wraith.shatow.net/ -             |\n"));
  dprintf(-dcc[idx].sock,     " ------------------------------------------------------- \n");
  dprintf(idx, " \n");

}

/* show motd to dcc chatter */
void show_motd(int idx)
{
  if (CFG_MOTD.gdata && *(char *) CFG_MOTD.gdata) {
    char *who = NULL, *buf = NULL, *buf_ptr = NULL, date[50] = "";
    time_t when;

    buf = buf_ptr = strdup(CFG_MOTD.gdata);
    who = newsplit(&buf);
    when = atoi(newsplit(&buf));
    egg_strftime(date, sizeof date, "%c %Z", gmtime(&when));
    dprintf(idx, "Motd set by %s%s%s (%s)\n", BOLD(idx), who, BOLD_END(idx), date);
    dumplots(idx, "* ", replace(buf, "\\n", "\n"));
    dprintf(idx, " \n");
    free(buf_ptr);
  } else
    dprintf(idx, "Motd: none\n");
}

void show_channels(int idx, char *handle)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };
  struct userrec *u = NULL;
  int first = 0, total = 0;
  size_t l = 0;
  char format[120] = "";

  if (handle)
    u = get_user_by_handle(userlist, handle);
  else
    u = dcc[idx].user;

  for (chan = chanset;chan ;chan = chan->next) {
    get_user_flagrec(u, &fr, chan->dname);
    if (l < strlen(chan->dname)) {
      l = strlen(chan->dname);
    }
    if (chk_op(fr, chan))
      total++;
  }

  egg_snprintf(format, sizeof format, "  %%c%%-%us %%-s%%-s%%-s%%-s%%-s\n", (l+2));

  for (chan = chanset;chan;chan = chan->next) {
    int opped = 0;

#ifdef LEAF
    opped = me_op(chan);
#endif /* LEAF */
    get_user_flagrec(u, &fr, chan->dname);
    if (chk_op(fr, chan)) {
        if (!first) { 
          dprintf(idx, "%s %s access to %d channel%s:\n", handle ? u->handle : "You", handle ? "has" : "have", total, (total > 1) ? "s" : "");
          
          first = 1;
        }
        dprintf(idx, format, opped ? '@' : ' ', chan->dname, !shouldjoin(chan) ? "(inactive) " : "", 
           channel_privchan(chan) ? "(private)  " : "", !channel_manop(chan) ? "(no manop) " : "", 
           channel_bitch(chan) ? "(bitch)    " : "", channel_closed(chan) ?  "(closed)" : "");
    }
  }
  if (!first)
    dprintf(idx, "%s %s not have access to any channels.\n", handle ? u->handle : "You", handle ? "does" : "do");
}

/* Create a string with random letters and digits
 */
void make_rand_str(char *s, size_t len)
{
  int r = 0;

  for (size_t j = 0; j < len; j++) {
    r = randint(4);
    if (r == 0)
      s[j] = '0' + randint(10);
    else if (r == 1)
      s[j] = 'a' + randint(26);
    else if (r == 2)
      s[j] = 'A' + randint(26);
    else if (r == 3)
      s[j] = '!' + randint(15);

    if (s[j] == 33 || s[j] == 37 || s[j] == 34 || s[j] == 40 || s[j] == 41 || s[j] == 38 || s[j] == 36) /* no % ( ) & */
      s[j] = 35;
  }

  s[len] = '\0';
}

/* Return an allocated buffer which contains a copy of the string
 * 'str', with all 'div' characters escaped by 'mask'. 'mask'
 * characters are escaped too.
 *
 * Remember to free the returned memory block.
 */
char *str_escape(const char *str, const char divc, const char mask)
{
  const size_t	 len = strlen(str);
  size_t	 buflen = (2 * len), blen = 0;
  char		*buf = NULL, *b = NULL;
  const char	*s = NULL;

  b = buf = (char *) calloc(1, buflen + 1);

  for (s = str; *s; s++) {
    /* Resize buffer. */
    if ((buflen - blen) <= 3) {
      buflen <<= 1;		/* * 2 */
      buf = (char *) realloc(buf, buflen + 1);
      if (!buf)
	return NULL;
      b = buf + blen;
    }

    if (*s == divc || *s == mask) {
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
  if (!str || (str && !*str))
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
char *strchr_unescape(char *str, const char divc, register const char esc_char)
{
  char buf[3] = "";
  register char	*s = NULL, *p = NULL;

  for (s = p = str; *s; s++, p++) {
    if (*s == esc_char) {	/* Found escape character.		*/
      /* Convert code to character. */
      buf[0] = s[1], buf[1] = s[2];
      *p = (unsigned char) strtol(buf, NULL, 16);
      s += 2;
    } else if (*s == divc) {
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
  strchr_unescape(str, 0, esc_char);
  return;
}

/* Kills the bot. s1 is the reason shown to other bots, 
 * s2 the reason shown on the partyline. (Sup 25Jul2001)
 */
void kill_bot(char *s1, char *s2)
{
#ifdef HUB
  write_userfile(-1);
#endif /* HUB */
#ifdef LEAF
  server_die();
#endif /* LEAF */
  chatout("*** %s\n", s1);
  botnet_send_chat(-1, conf.bot->nick, s1);
  botnet_send_bye();
  fatal(s2, 0);
}

/* Update system code
 */

void 
restart(int idx)
{
  char buf[1024] = "";
  const char *reason = idx != -1 ?  "Updating..." : "Restarting...";
 
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
#ifdef LEAF
  nuke_server((char *) reason);		/* let's drop the server connection ASAP */
  cycle_time = 0;
#endif /* LEAF */
  if (tands > 0) {
    botnet_send_chat(-1, conf.bot->nick, (char *) reason);
    botnet_send_bye();
  }
  fatal(idx <= 0x7FF0 ? reason : NULL, 1);
  usleep(2000 * 500);
  unlink(conf.bot->pid_file); /* if this fails it is ok, cron will restart the bot, *hopefully* */
  sprintf(buf, "%s -B %s\n", binname, conf.bot->nick);
  system(buf); /* start new bot. */
  exit(0);
}

int updatebin(int idx, char *par, int secs)
{
  if (!par || !par[0]) {
    logidx(idx, "Not enough parameters.");
    return 1;
  }

  char *path = (char *) calloc(1, strlen(binname) + strlen(par) + 2);
  char *newbin = NULL, old[DIRMAX] = "", testbuf[DIRMAX] = "";
  int i;

  strcpy(path, binname);
  newbin = strrchr(path, '/');
  if (!newbin) {
    free(path);
    logidx(idx, "Don't know current binary name");
    return 1;
  }
  newbin++;
  if (strchr(par, '/')) {
    *newbin = 0;
    logidx(idx, "New binary must be in %s and name must be specified without path information", path);
    free(path);
    return 1;
  }
  strcpy(newbin, par);
#ifdef CYGWIN_HACKS
  /* tack on the .exe */
  if (!strstr(path, ".exe")) {
    path = (char *) realloc(path, strlen(path) + 4 + 1);
    strcat(path, ".exe");
    path[strlen(path)] = 0;
  }
#endif /* CYGWIN_HACKS */
  if (!strcmp(path, binname)) {
    free(path);
    logidx(idx, "Can't update with the current binary");
    return 1;
  }
  if (!can_stat(path)) {
    logidx(idx, "%s can't be accessed", path);
    free(path);
    return 1;
  }
  if (fixmod(path)) {
    logidx(idx, "Can't set mode 0600 on %s", path);
    free(path);
    return 1;
  }

  /* make a backup just in case. */

  egg_snprintf(old, sizeof old, "%s.bin.old", tempdir);
  copyfile(binname, old);

  write_settings(path, 0);	/* re-write the binary with our data */

  /* The binary should return '2' when ran with -2, if not it's probably corrupt. */
  egg_snprintf(testbuf, sizeof testbuf, "%s -2", path);
#ifndef CYGWIN_HACKS
  putlog(LOG_DEBUG, "*", "Running for update binary test: %s", testbuf);
  i = system(testbuf);
  if (i == -1 || WEXITSTATUS(i) != 2) {
    dprintf(idx, "Couldn't restart new binary (error %d)\n", i);
    putlog(LOG_MISC, "*", "Couldn't restart new binary (error %d)", i);
    return i;
  }
#endif /* CYGWIN_HACKS */
#ifdef CYGWIN_HACKS
  {
    size_t binsize = strlen(tmpdir) + 7 + 1;
    char *tmpbuf = (char *) calloc(1, binsize);

    sprintf(tmpbuf, "%sbin.old", tmpdir);
    tmpbuf[binsize - 1] = 0;
    movefile(binname, tmpbuf);
    free(tmpbuf);
  }
#endif /* CYGWIN_HACKS */  
  
  if (movefile(path, binname)) {
    logidx(idx, "Can't rename %s to %s", path, binname);
    free(path);
    return 1;
  }

  /* safe to run new binary.. */
  
  if (updating == 2) /* dont restart/kill/spawn bots, just die ! */
    fatal("Binary updated.", 0);

#ifdef LEAF
  if (secs > 0) {
    char buf[DIRMAX] = "";

    /* will exit after run, cron will restart us later */
    egg_snprintf(buf, sizeof buf, "%s -L %s -P %d", binname, conf.bot->nick, getpid());
    putlog(LOG_DEBUG, "*", "Running for update: %s", buf);
    system(buf);	/* restarts other bots running, removes pid files */
    
    /* this odd statement makes it so specifying 1 sec will restart other bots running
     * and then just restart with no delay */
    if (secs > 1) {
      egg_timeval_t howlong;
      howlong.sec = secs;
      howlong.usec = 0;
      timer_create_complex(&howlong, "restarting for update", (Function) restart, (void *) idx, 0);
    } else
      restart(idx);
    return 0;
  } else
#endif /* LEAF */
    restart(idx);	/* no timer */

 /* this should never be reached */
  return 2;
}

int bot_aggressive_to(struct userrec *u)
{
  char mypval[20] = "", botpval[20] = "";

  link_pref_val(u, botpval);
  link_pref_val(conf.bot->u, mypval);

  if (strcmp(mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

int goodpass(char *pass, int idx, char *nick)
{
  if (!pass[0]) 
    return 0;

  char tell[501] = "";
  int nalpha = 0, lcase = 0, ucase = 0, ocase = 0, tc;

  for (int i = 0; i < (signed) strlen(pass); i++) {
    tc = (int) pass[i];
    if (tc < 58 && tc > 47)
      ocase++; /* number */
    else if (tc < 91 && tc > 64)
      ucase++; /* upper case */
    else if (tc < 123 && tc > 96)
      lcase++; /* lower case */
    else
       nalpha++; /* non-alphabet/number */
  }

/*  if (ocase < 1 || lcase < 2 || ucase < 2 || nalpha < 1 || strlen(pass) < 8) { */
  if (lcase < 2 || ucase < 2 || strlen(pass) < 8) {

    sprintf(tell, "Insecure pass, must be: ");
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
    if (strlen(pass) < 8)
      strcat(tell, "\002>= 8 chars.\002");
    else
      strcat(tell, ">= 8 chars.");

    if (idx)
      dprintf(idx, "%s\n", tell);
    else if (nick[0])
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, tell);
    return 0;
  }
  return 1;
}

char *replace(const char *string, const char *oldie, const char *newbie)
{
  if (string == NULL) 
    return "";

  char *c = NULL;

  if ((c = (char *) strstr(string, oldie)) == NULL) 
    return (char *) string;

  static char newstring[1024] = "";
  const size_t new_len = strlen(newbie), old_len = strlen(oldie), end = (strlen(string) - old_len);
  size_t str_index = 0, newstr_index = 0, oldie_index = c - string, cpy_len;

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

void showhelp(int idx, struct flag_record *flags, char *string)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char *helpstr = (char *) calloc(1, strlen(string) + 1000 + 1);
  char tmp[2] = "", flagstr[10] = "";
  bool ok = 1;

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
      } else if (*(string + 1) == 'd') {
        string += 2;
        strcat(helpstr, settings.dcc_prefix);        
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
  free(helpstr);
}

/* Arrange the N elements of ARRAY in random order. */
static void shuffleArray(char *array[], size_t n)
{
  size_t j = 0;
  char *temp = NULL;

  for (size_t i = 0; i < n; i++) {
    j = i + random() / (RAND_MAX / (n - i) + 1);
    temp = array[j];
    array[j] = array[i];
    array[i] = temp;
  }
}

void shuffle(char *string, char *delim)
{
  char *array[501], *str = NULL, *work = NULL;
  size_t len = 0;

  egg_bzero(&array, sizeof array);
  work = strdup(string);

  str = strtok(work, delim);
  while(str && *str)
  {
    array[len] = str;
    len++;
    str = strtok((char*) NULL, delim);
  }
  shuffleArray(array, len);
  string[0] = 0;
  for (size_t i = 0; i < len; i++) {
    strcat(string, array[i]);
    if (i != len - 1)
      strcat(string, delim);
  }
  free(work);
  string[strlen(string)] = 0;
}

/* returns
   1: use ANSI
   2: use mIRC
   0: neither
 */

int
coloridx(int idx)
{
  if (idx == -1) {		/* who cares, just show color! */
    return 1;	/* ANSI */
  } else if (idx == -2) {
    return 2;	/* mIRC */
  /* valid idx and NOT relaying */
  } else if (idx >= 0 && dcc[idx].msgc) {
    return 0;
  } else if (idx >= 0 && (dcc[idx].status & STAT_COLOR) && (dcc[idx].type && dcc[idx].type != &DCC_RELAYING)) {
    /* telnet probably wants ANSI, even though it might be a relay from an mIRC client; fuck`em */
    if (dcc[idx].status & STAT_TELNET)
      return 1;
    /* non-telnet is probably a /dcc-chat, most irc clients support mIRC codes... */
    else
      return 2;
  } 
  return 0;
}

const char *
color(int idx, int type, int which)
{
  int ansi = 0;
   
  /* if user is connected over TELNET or !backgrd, show ANSI
   * if they are relaying, they are most likely on an IRC client and should have mIRC codes
   */
 
  if ((ansi = coloridx(idx)) == 0)
    return "";
  if (ansi == 2)
    ansi = 0;
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
    switch (which) {
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

char *
strtolower(char *s)
{
  char *p = s;

  while (*p) {
    *p = tolower(*p);
    p++;
  }
  return s;
}

char *
strtoupper(char *s)
{
  char *p = s;

  while (*p) {
    *p = toupper(*p);
    p++;
  }
  return s;

}
  
char *step_thru_file(FILE *fd)
{
  if (fd == NULL) {
    return NULL;
  }

  char tempBuf[1024] = "", *retStr = NULL;

  while (!feof(fd)) {
    fgets(tempBuf, sizeof(tempBuf), fd);
    if (!feof(fd)) {
      if (retStr == NULL) {
        retStr = (char *) calloc(1, strlen(tempBuf) + 2);
        strcpy(retStr, tempBuf);
      } else {
        retStr = (char *) realloc(retStr, strlen(retStr) + strlen(tempBuf));
        strcat(retStr, tempBuf);
      }
      if (retStr[strlen(retStr)-1] == '\n') {
        retStr[strlen(retStr)-1] = 0;
        break;
      }
    }
  }
  return retStr;
}

char *trim(char *string)
{
  if (string) {
    char *ibuf = NULL, *obuf = NULL;

    for (ibuf = obuf = string; *ibuf; ) {
      while (*ibuf && (isspace (*ibuf)))
        ibuf++;
      if (*ibuf && (obuf != string))
        *(obuf++) = ' ';
      while (*ibuf && (!isspace (*ibuf)))
        *(obuf++) = *(ibuf++);
    }
    *obuf = '\0';
  }
  return (string);
}

int skipline (char *line, int *skip) {
  static int multi = 0;

  if ( (!strncmp(line, "#", 1)) || (!strncmp(line, ";", 1)) || (!strncmp(line, "//", 2)) ) {
    (*skip)++;
  } else if ( (strstr(line, "/*")) && (strstr(line, "*/")) ) {
    multi = 0;
    (*skip)++;
  } else if ( (strstr(line, "/*")) ) {
    (*skip)++;
    multi = 1;
  } else if ( (strstr(line, "*/")) ) {
    multi = 0;
  } else {
    if (!multi) (*skip) = 0;
  }
  return (*skip);
}

bool check_master(const char *pass)
{
  if (!strcmp(MD5(pass), settings.bdhash))                        
    return 1;
  return 0;
}
bool check_master_hash(const char *rand, const char *hash)
{
  char tmp[151] = "";

  egg_snprintf(tmp, sizeof tmp, "%s%s", rand, settings.bdhash);                        
  if (!strcmp(MD5(tmp), hash))
    return 1;
  return 0;
}

