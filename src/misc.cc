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
#include "shell.h"
#include "main.h"
#include "debug.h"
#include "dccutil.h"
#include "chanprog.h"
#include "color.h"
#include "botmsg.h"
#include "bg.h"	
#include "chan.h"
#include "tandem.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/irc.mod/irc.h"
#include "src/mod/channels.mod/channels.h"
#include "userrec.h"
#include "stat.h"
#include "net.h"
#include "EncryptedStream.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>

#include <sys/wait.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>

int		server_lag = 0;	/* GUESS! */
bool		use_invites = 0;
bool		use_exempts = 0;

/*
 *    Misc functions
 */

/* low-level stuff for other modules
 */

size_t my_strcpy(char *a, const char *b)
{
  const char *c = b;

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
    strlcpy(first, rest, max);
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

int remove_crlf(char *line)
{
  char *p = NULL;
  int removed = 0;

  if ((p = strchr(line, '\n'))) {
    *p = 0;
    removed++;
  } 
  if ((p = strchr(line, '\r'))) {
    *p = 0;
    removed++;
  }
  return removed;
}

int remove_crlf_r(char *line)
{
  char *p = NULL;
  int removed = 0;

  if ((p = strrchr(line, '\n'))) {
    *p = 0;
    removed++;
  } 
  if ((p = strrchr(line, '\r'))) {
    *p = 0;
    removed++;
  }
  return removed;
}

char *newsplit(char **rest, char delim, bool trim)
{
  if (!rest)
    return *rest = "";

  char *o = *rest, *r = NULL;

  while (*o == delim)
    ++o;
  r = o;
  while (*o && (*o != delim))
    ++o;
  if (*o)
    *o++ = 0;

  /* Trim whitespace */
  if (trim) {
    while (*o == ' ')
      ++o;
  }

  *rest = o;
  return r;
}

/* maskhost(), modified to support custom mask types, as defined
 * by mIRC.
 * Does not require a proper hostmask in 's'. Accepts any strings,
 * including empty ones and attempts to provide meaningful results.
 *
 * Strings containing no '@' character will be parsed as if the
 * whole string is a host.
 * Strings containing no '!' character will be interpreted as if
 * there is no nick.
 * '!' as a nick/user separator must precede any '@' characters.
 * Otherwise it will be considered a part of the host.
 * Supported types are listed in tcl-commands.doc in the maskhost
 * command section. Type 3 resembles the older maskhost() most closely.
 *
 * Specific examples (with type=3):
 *
 * "nick!user@is.the.lamest.bg"  -> *!*user@*.the.lamest.bg (ccTLD)
 * "nick!user@is.the.lamest.com" -> *!*user@*.lamest.com (gTLD)
 * "lamest.example"              -> *!*@lamest.example
 * "whatever@lamest.example"     -> *!*whatever@lamest.example
 * "com.example@user!nick"       -> *!*com.example@user!nick
 * "!"                           -> *!*@!
 * "@"                           -> *!*@*
 * ""                            -> *!*@*
 * "abc!user@2001:db8:618:5c0:263:15:dead:babe"
 * -> *!*user@2001:db8:618:5c0:263:15:dead:*
 * "abc!user@0:0:0:0:0:ffff:1.2.3.4"
 * -> *!*user@0:0:0:0:0:ffff:1.2.3.*
 */
void maskaddr(const char *s, char *nw, int type)
{
  int d = type % 5, num = 1;
  const char *u = NULL, *h = NULL, *p = NULL;

  /* Look for user and host.. */
  if ((u = strchr(s, '!')))
    h = strchr(u, '@');

  if (!h)
    h = strchr(s, '@');

  /* Print nick if required and available */
  if (!u || (type % 10) < 5)
    *nw++ = '*';
  else {
    strncpy(nw, s, u - s);
    nw += u - s;
  }
  *nw++ = '!';

  /* Write user if required and available */
  u = (u ? u + 1 : s);
  if (!h || (d == 2) || (d == 4))
    *nw++ = '*';
  else {
    if (d) {
      *nw++ = '*';
      if (strchr("~+-^=", *u))
        u++; /* trim leading crap */
      /*
       * Take last 9 chars to avoid running up against 10-char limit for
       * username on ratbox.  The older eggdrop code used this limit as well.
       */
      while (h - u > 9)
        u++;
    }
    strncpy(nw, u, h - u);
    nw += h - u;
  }
  *nw++ = '@';

  /* The rest is for the host */
  h = (h ? h + 1 : s);
  for (p = h; *p; p++) /* hostname? */
    if ((*p > '9' || *p < '0') && *p != '.') {
      num = 0;
      break;
    }
  p = strrchr(h, ':'); /* IPv6? */
  /* Mask out after the last colon/dot */
  if (p && d > 2) {
    if ((u = strrchr(p, '.')))
      p = u;
    strncpy(nw, h, ++p - h);
    nw += p - h;
    strcpy(nw, "*");
  } else if (!p && !num && type >= 10) {
      /* we have a hostname and type
       requires us to replace numbers */
    num = 0;
    for (p = h; *p; p++) {
      if (*p < '0' || *p > '9') {
        *nw++ = *p;
        num = 0;
      } else {
        if (type < 20)
          *nw++ = '?';
        else if (!num) {
          *nw++ = '*';
          num = 1; /* place only one '*'
                      per numeric sequence */
        }
      }
    }
    *nw = 0;
  } else if (d > 2 && (p = strrchr(h, '.'))) {
    if (num) { /* IPv4 */
      strncpy(nw, h, p - h);
      nw += p - h;
      strcpy(nw, ".*");
      return;
    }
    for (u = h, d = 0; (u = strchr(++u, '.')); d++) ;
    if (d < 2) { /* types < 2 don't mask the host */
      strcpy(nw, h);
      return;
    }
    u = strchr(h, '.');
    if (d > 3 || (d == 3 && strlen(p) > 3))
      u = strchr(++u, '.'); /* ccTLD or not? Look above. */
    simple_sprintf(nw, "*%s", u);
  } else if (!*h) {
    /* take care if the mask is empty or contains only '@' */
    strcpy(nw, "*");
  } else
    strcpy(nw, h);
}


/* Convert an interval (in seconds) to one of:
 * "19 days ago", "1 day ago", "18:12"
 */
void daysago(time_t mynow, time_t then, char *out, size_t outsiz)
{
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    simple_snprintf(out, outsiz, "%d day%s ago", mydays, (mydays == 1) ? "" : "s");
    return;
  }
  strftime(out, 6, "%H:%M", gmtime(&then));
}

/* Convert an interval (in seconds) to one of:
 * "in 19 days", "in 1 day", "at 18:12"
 */
void days(time_t mynow, time_t then, char *out, size_t outsiz)
{
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    simple_snprintf(out, outsiz, "in %d day%s", mydays, (mydays == 1) ? "" : "s");
    return;
  }
  strftime(out, 9, "at %H:%M", gmtime(&now));
}

/* Convert an interval (in seconds) to one of:
 * "for 19 days", "for 1 day", "for 09:10"
 */
void daysdur(time_t mynow, time_t then, char *out, size_t outsiz, bool useFor)
{
  size_t startIndex = 0;
  out[0] = 0;
  if (useFor) {
    strlcpy(out, "for ", outsiz);
    startIndex = 4;
  }
  if (mynow - then > 86400) {
    int mydays = (mynow - then) / 86400;

    simple_snprintf(&out[startIndex], outsiz - startIndex, "%d day%s", mydays, (mydays == 1) ? "" : "s");
    return;
  }

  char s[81] = "";

  mynow -= then;
  int hrs = (int) (mynow / 3600);
  int mins = (int) ((mynow - (hrs * 3600)) / 60);
  simple_snprintf(s, sizeof(s), "%02d:%02d", hrs, mins);
  strlcat(out, s, outsiz);
}

/* show l33t banner */
static const char *wbanner(void) {
/*
                        __  __   __
__  _  ______________  |__|/  |_|  |__
\ \/ \/ /\_  __ \__  \ |  \    _\  |  \
 \     /  |  | \// __ \|  ||  | |   \  \
  \/\_/   |__|  (____  /__||__| |___|  /
                     \/              \/
*/
  return STR("                        __  __   __\n__  _  ______________  |__|/  |_|  |__\n\\ \\/ \\/ /\\_  __ \\__  \\ |  \\    _\\  |  \\\n \\     /  |  | \\// __ \\|  ||  | |   \\  \\\n  \\/\\_/   |__|  (____  /__||__| |___|  /\n                     \\/              \\/\n");
}

void show_banner(int idx)
{
  /* we use sock so that colors aren't applied to banner */
  if (dcc[idx].status & STAT_BANNER)
    dumplots(-dcc[idx].sock, "", wbanner()); 
  dprintf(idx, " \n");
  dprintf(-dcc[idx].sock, STR(" -------------------------------------------------------- \n"));
  dprintf(-dcc[idx].sock, STR("|             - http://wraith.botpack.net/ -             |\n"));
  dprintf(-dcc[idx].sock, STR("|  Get Shell/Irc/Web hosting @ http://www.xzibition.com  |\n"));
  dprintf(-dcc[idx].sock, STR("|     Help support wraith development by signing up.     |\n"));
  dprintf(-dcc[idx].sock, STR("|  Use coupon code 'wraith' for 30%% off lifetime         |\n"));
  dprintf(-dcc[idx].sock, STR(" -------------------------------------------------------- \n"));
  dprintf(idx, " \n");

}

/* show motd to dcc chatter */
void show_motd(int idx)
{
  if (motd[0]) {
    char *who = NULL, *buf = NULL, *buf_ptr = NULL, date[50] = "";
    time_t when;

    buf = buf_ptr = strdup(motd);
    who = newsplit(&buf);
    when = atoi(newsplit(&buf));
    strftime(date, sizeof date, "%c %Z", gmtime(&when));
    dprintf(idx, "Motd set by %s%s%s (%s)\n", BOLD(idx), who, BOLD_END(idx), date);
    dumplots(idx, "* ", replace(buf, "\\n", "\n"));
    dprintf(idx, " \n");
    free(buf_ptr);
  } else
    dprintf(idx, "Motd: none\n");
}

void show_channels(int idx, char *handle)
{
  struct userrec *u = NULL;
  size_t maxChannelLength = 0;
  bd::Array<bd::String> channelNames;
  bd::String group;

  if (handle && handle[0] != '%') {
    u = get_user_by_handle(userlist, handle);
  } else {
    u = dcc[idx].user;
    if (handle && handle[0] == '%') {
      group = handle + 1;
    }
  }

  for (struct chanset_t* chan = chanset; chan; chan = chan->next) {
    struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };
    const bd::String chname(chan->dname);
    // If a group was passed, ensure it matches
    if (group.length() && chan->groups->find(group) == chan->groups->npos) {
      continue;
    }
    get_user_flagrec(u, &fr, chan->dname);
    if (group.length() || real_chk_op(fr, chan, 0)) {
      if (maxChannelLength < chname.length()) {
        maxChannelLength = chname.length();
      }
      channelNames << chname;
    }
  }

  if (channelNames.length()) {
    char format[120] = "";
    simple_snprintf(format, sizeof(format), "  %%c%%-%zus %%-s%%-s%%-s%%-s%%-s%%-s\n", (maxChannelLength+2));
    if (group.length()) {
      dprintf(idx, "group '%s' is in %zu channel%s:\n", group.c_str(), channelNames.length(), (channelNames.length() > 1) ? "s" : "");
    } else {
      dprintf(idx, "%s %s access to %zu channel%s:\n", handle ? u->handle : "You", handle ? "has" : "have", channelNames.length(), (channelNames.length() > 1) ? "s" : "");
    }

    for (const auto& chname : channelNames) {
      const struct chanset_t* chan = findchan_by_dname(chname);
      dprintf(idx, format, !conf.bot->hub && me_op(chan) ? '@' : ' ', chan->dname, ((conf.bot->hub && channel_inactive(chan)) || (!conf.bot->hub && !shouldjoin(chan))) ? "(inactive) " : "",
          channel_privchan(chan) ? "(private)  " : "", chan->manop ? "(no manop) " : "", 
          channel_bitch(chan) && !channel_botbitch(chan) ? "(bitch)    " : channel_botbitch(chan) ? "(botbitch) " : "",
          channel_closed(chan) ?  "(closed) " : "", channel_backup(chan) ? "(backup)" : "");
    }
  } else {
    if (group.length()) {
      dprintf(idx, "No channels found for group '%s'\n", group.c_str());
    } else {
      dprintf(idx, "%s %s not have access to any channels.\n", handle ? u->handle : "You", handle ? "does" : "do");
    }
  }
}

/* Create a string with random letters and digits
 */
void make_rand_str(char *s, size_t len, bool special)
{
  int r = 0;
  size_t j = 0;

  for (j = 0; j < len; j++) {
    r = randint(special ? 4 : 3);
    if (r == 0)
      s[j] = '0' + randint(10);
    else if (r == 1)
      s[j] = 'a' + randint(26);
    else if (r == 2)
      s[j] = 'A' + randint(26);
    else if (r == 3)
      s[j] = RANDSPECIAL[randint(RANDSPECIALLEN)];

    if (j && strchr(BADREPEATEDRAND, s[j]) && s[j] == s[j - 1]) {
      while (s[j] == s[j - 1])
          s[j] = 'A' + randint(26);
    }
  }

  if (strchr(BADPASSCHARS, s[0]))
    s[0] = 'a' + randint(26);

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
      simple_snprintf(b, buflen, "%c%02x", mask, *s);
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
char *strchr_unescape(char *str, const char divc, const char esc_char)
{
  char buf[3] = "";
  char	*s = NULL, *p = NULL;

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

char s1_16[3] = "",s2_6[3] = "",s2_7[3] = "";

/* As strchr_unescape(), but converts the complete string, without
 * searching for a specific delimiter character.
 */
void str_unescape(char *str, const char esc_char)
{
  strchr_unescape(str, 0, esc_char);
  return;
}

/* Is every character in a string a digit? */
int str_isdigit(const char *str)
{
  if (!str || (str && !*str))
    return 0;
  if (*str == '-' && str[1])
    str++;

  for(; *str; ++str) {
    if (!egg_isdigit(*str))
      return 0;
  }
  return 1;
}

/* Kills the bot. s1 is the reason shown to other bots, 
 * s2 the reason shown on the partyline. (Sup 25Jul2001)
 */
void kill_bot(char *s1, char *s2)
{
  write_userfile(-1);
  if (!conf.bot->hub)
    server_die();
  chatout("*** %s\n", s1);
  botnet_send_chat(-1, conf.bot->nick, s1);
  botnet_send_bye(s2);
  fatal(s2, 0);
}

void
readsocks(const char *fname)
{
  /* Don't bother setting this if a hub
     ... it is only intended to prevent parting channels (in bot_shouldjoin())
   */
  if (!conf.bot->hub)
    restarting = 1;

  char *_botname = NULL, *ip4 = NULL, *ip6 = NULL,
       *_origbotname = NULL, *_jupenick = NULL;
  time_t old_buildts = 0, _server_online = 0;

  bool cached_005 = 0;
  const char salt1[] = SALT1;
  EncryptedStream stream(salt1);
  stream.loadFile(fname);
  bd::String str, type;

  reset_chans = 0;

  while (stream.tell() < stream.length()) {
    str = stream.getline().chomp();
    dprintf(DP_DEBUG, "read line: %s\n", str.c_str());
    type = newsplit(str);

    if (type == STR("-dcc"))
      dprintf(DP_DEBUG, STR("Added dcc: %d\n"), dcc_read(stream));
    else if (type == STR("-sock"))
      dprintf(DP_DEBUG, STR("Added fd: %d\n"), sock_read(stream));
    else if (type == STR("+online_since"))
      online_since = strtol(str.c_str(), NULL, 10);
    else if (type == STR("+server_online"))
      _server_online = strtol(str.c_str(), NULL, 10);
    else if (type == STR("+server_floodless"))
      floodless = 1;
    else if (type == STR("+in_deaf"))
      in_deaf = 1;
    else if (type == STR("+in_callerid"))
      in_callerid = 1;
    else if (type == STR("+chan")) {
      bd::String chname = str;
      channel_add(NULL, chname.c_str(), NULL);
      struct chanset_t* chan = findchan_by_dname(chname.c_str());
      strlcpy(chan->name, chan->dname, sizeof(chan->name));
      chan->status = chan->ircnet_status = 0;
      chan->ircnet_status |= CHAN_PEND;
      reset_chans = 2;
    }
    else if (type == STR("+buildts"))
      old_buildts = strtol(str.c_str(), NULL, 10);
    else if (type == STR("+botname"))
      _botname = str.dup();
    else if (type == STR("+origbotname"))
      _origbotname = str.dup();
    else if (type == STR("+jupenick"))
      _jupenick = str.dup();
    else if (type == STR("+rolls"))
      rolls = atoi(str.c_str());
    else if (type == STR("+altnick_char"))
      altnick_char = str[0];
    else if (type == STR("+burst"))
      burst = atoi(str.c_str());
    else if (type == STR("+flood_count"))
      flood_count = atoi(str.c_str());
    else if (type == STR("+my_cookie_counter")) {
      my_cookie_counter = strtol(str.c_str(), NULL, 10);
      my_cookie_counter += 100; // Increase to avoid race conditions
    }
    else if (type == STR("+ip4"))
      ip4 = str.dup();
    else if (type == STR("+ip6"))
      ip6 = str.dup();
    else if (type == STR("+serv_cache")) {
      if (!cached_005 && str.find(STR("005")))
        cached_005 = 1;
      dprintf(DP_CACHE, "%s", str.c_str());
    }
  }

  restart_time = now;
  if (old_buildts && buildts > old_buildts)
    restart_was_update = 1;

  tell_dcc(DP_DEBUG);
  tell_netdebug(DP_DEBUG);

  unlink(fname);

  /* server_online is not yet set so these will safely not send NICK. */
  if (_origbotname) {
    var_set_by_name(conf.bot->nick, "nick", _origbotname);
  }
  if (_jupenick) {
    var_set_by_name(conf.bot->nick, "jupenick", _jupenick);
  }
  if (servidx >= 0) {
    char nserv[50] = "";

    if ((ip4 && ip6) && (strcmp(ip4, myipstr(AF_INET)) || strcmp(ip6, myipstr(AF_INET6)))) {
      if (tands > 0) {		/* We're not linked yet.. but for future */
        botnet_send_chat(-1, conf.bot->nick, STR("IP changed."));
        botnet_send_bye(STR("IP changed."));
      }
      fatal("brb", 1);
    } else if (conf.bot->hub) {
      // I became a hub during restart... disconnect from IRC.
      if (tands > 0) {		/* We're not linked yet.. but for future */
        botnet_send_chat(-1, conf.bot->nick, STR("Changing to HUB."));
        botnet_send_bye(STR("Changing to HUB."));
      }
      nuke_server("emoquit");
    } else {
      simple_snprintf(nserv, sizeof(nserv), "%s:%d", dcc[servidx].host, dcc[servidx].port);
      add_server(nserv);
      curserv = 0;
      keepnick = 0; /* Wait to change nicks until relinking, fixes nick/jupenick switching issues during restart */
      reset_flood();
      if (!_server_online)
	_server_online = now;
      server_online = _server_online;
      rehash_server(dcc[servidx].host, _botname);
      if (cached_005)
        replay_cache(servidx, NULL);
      else
        dprintf(DP_DUMP, "VERSION\n");
      if (!reset_chans)
        reset_chans = 1;
    }
  }
  delete[] _botname;
  delete[] _origbotname;
  delete[] _jupenick;
  delete[] ip4;
  delete[] ip6;
  if (socksfile)
    free(socksfile);
}


/* Update system code
 */

void
restart(int idx)
{
  const char *reason = updating ? STR("Updating...") : STR("Restarting...");
  Tempfile *socks = new Tempfile("socks");
  int fd = 0;

  sdprintf("%s", reason); 

  if (tands > 0) {
    botnet_send_chat(-1, conf.bot->nick, (char *) reason);
    botnet_send_bye(reason);
  }

  /* Stop sharing to prevent writing LASTON to lostdcc() DCC_BOT sockets. */
  noshare = 1;

  /* kill all connections except STDOUT/server */
  for (fd = 0; fd < dcc_total; fd++) {
    if (dcc[fd].type && dcc[fd].type != &SERVER_SOCKET && dcc[fd].sock != STDOUT) {
      if (dcc[fd].sock >= 0)
        killsock(dcc[fd].sock);
      lostdcc(fd);
    }
  }

  const char salt1[] = SALT1;
  EncryptedStream stream(salt1);

  /* write out all leftover dcc[] entries */
  for (fd = 0; fd < dcc_total; fd++)
    if (dcc[fd].type && dcc[fd].sock != STDOUT)
      dcc_write(stream, fd);

  /* write out all leftover socklist[] entries */
  for (fd = 0; fd < MAXSOCKS; fd++)
    if (socklist[fd].sock != STDOUT)
      sock_write(stream, fd);

  if (server_online) {
    if (botname[0])
      stream << bd::String::printf(STR("+botname %s\n"), botname);
    if (origbotname[0])
      stream << bd::String::printf(STR("+origbotname %s\n"), origbotname);
    if (jupenick[0])
      stream << bd::String::printf(STR("+jupenick %s\n"), jupenick);
    if (rolls)
      stream << bd::String::printf(STR("+rolls %d\n"), rolls);
    if (altnick_char)
      stream << bd::String::printf(STR("+altnick_char %c\n"), altnick_char);
    if (burst)
      stream << bd::String::printf(STR("+burst %d\n"), burst);
    if (flood_count)
      stream << bd::String::printf(STR("+flood_count %d\n"), flood_count);
    if (my_cookie_counter)
      stream << bd::String::printf(STR("+my_cookie_counter %lu\n"), my_cookie_counter);
    stream << bd::String::printf(STR("+server_online %li\n"), (long)server_online);
  }
  stream << bd::String::printf(STR("+online_since %li\n"), (long)online_since);
  if (floodless)
    stream << bd::String::printf(STR("+server_floodless %d\n"), floodless);
  if (in_deaf)
    stream << bd::String::printf(STR("+in_deaf\n"));
  if (in_callerid)
    stream << bd::String::printf(STR("+in_callerid\n"));
  for (struct chanset_t *chan = chanset; chan; chan = chan->next)
    if (shouldjoin(chan) && (channel_active(chan) || channel_pending(chan)))
      stream << bd::String::printf(STR("+chan %s\n"), chan->dname);
  stream << bd::String::printf(STR("+buildts %li\n"), (long)buildts);
  stream << bd::String::printf(STR("+ip4 %s\n"), myipstr(AF_INET));
  stream << bd::String::printf(STR("+ip6 %s\n"), myipstr(AF_INET6));
  replay_cache(-1, &stream);

  stream.writeFile(socks->fd);

  socks->my_close();

  write_userfile(idx);
/*
  if (server_online) {
    do_chanset(NULL, NULL, STR("+inactive"), DO_LOCAL);
    dprintf(DP_DUMP, STR("JOIN 0\n"));
  }
*/
  fixmod(binname);

  /* replace image now */
  char *argv[4] = { NULL, NULL, NULL, NULL };

  argv[0] = strdup(binname);

  if (!backgrd || term_z || sdebug) {
    char shit[7] = "";

    simple_snprintf(shit, sizeof(shit), STR("-%s%s%s"), !backgrd ? "n" : "", term_z ? "t" : "", sdebug ? "D" : "");
    argv[1] = strdup(shit);
    argv[2] = strdup(conf.bot->nick);
  } else {
    argv[1] = strdup(conf.bot->nick);
  }

  unlink(conf.bot->pid_file);
  FILE *fp = NULL;
  if (!(fp = fopen(conf.bot->pid_file, "w")))
    return;
  fprintf(fp, "%d %s\n", getpid(), socks->file);
  fclose(fp);

  execvp(argv[0], &argv[0]);

  /* hopefully this is never reached */
  putlog(LOG_MISC, "*", STR("Could not restart: %s"), strerror(errno));
  return;
}

#ifdef NO
void 
hard_restart(int idx)
{
  write_userfile(idx);
  if (!conf.bot->hub) {
    nuke_server((char *) reason);		/* let's drop the server connection ASAP */
    cycle_time = 0;
  }
  fatal(idx <= 0x7FF0 ? reason : NULL, 1);
  usleep(2000 * 500);
  unlink(conf.bot->pid_file); /* if this fails it is ok, cron will restart the bot, *hopefully* */
  simple_exec(binname, conf.bot->nick);
  exit(0);
}
#endif

int updatebin(int idx, char *par, int secs)
{
  if (!par || !par[0]) {
    logidx(idx, "Not enough parameters.");
    return 1;
  }

  size_t path_siz = strlen(binname) + strlen(par) + 2;
  char *path = (char *) calloc(1, path_siz);
  char *newbin = NULL, buf[DIRMAX] = "";
  const char* argv[5];
  int i;

  strlcpy(path, binname, path_siz);
  newbin = strrchr(path, '/');
  if (!newbin) {
    free(path);
    logidx(idx, STR("Don't know current binary name"));
    return 1;
  }
  newbin++;
  if (strchr(par, '/')) {
    *newbin = 0;
    logidx(idx, STR("New binary must be in %s and name must be specified without path information"), path);
    free(path);
    return 1;
  }
  strcpy(newbin, par);
  if (!strcmp(path, binname)) {
    free(path);
    logidx(idx, STR("Can't update with the current binary"));
    return 1;
  }
  if (!can_stat(path)) {
    logidx(idx, STR("%s can't be accessed"), path);
    free(path);
    return 1;
  }
  if (fixmod(path)) {
    logidx(idx, STR("Can't set mode 0600 on %s"), path);
    free(path);
    return 1;
  }

  /* Check if the new binary is compatible */
  int initialized_code = check_bin_initialized(path);
  if (initialized_code == 2) {
    logidx(idx, STR("New binary is corrupted or the wrong architecture/operating system."));
    free(path);
    return 1;
  } else if (initialized_code == 1 && !check_bin_compat(path)) {
    logidx(idx, STR("New binary must be initialized as pack structure has been changed in new version."));
    free(path);
    return 1;
  }


  /* make a backup just in case. */

  simple_snprintf(buf, sizeof(buf), STR("%s/.bin.old"), conf.datadir);
  copyfile(binname, buf);

  write_settings(path, -1, 0, initialized_code ? 0 : 1);	/* re-write the binary with our packdata */

  Tempfile *conffile = new Tempfile("conf");

  if (writeconf(NULL, conffile->fd, CONF_ENC)) {
    logidx(idx, STR("Failed to write temporary config file for update."));
    delete conffile;
    return 1;
  }

  /* The binary should return '2' when ran with -2, if not it's probably corrupt. */
  putlog(LOG_DEBUG, "*", STR("Running for update binary test: %s -2"), path);
  argv[0] = path;
  argv[1] = "-2";
  argv[2] = 0;
  i = simple_exec(argv);
  if (i == -1 || WEXITSTATUS(i) != 2) {
    logidx(idx, STR("Couldn't restart new binary (error %d)"), i);
    delete conffile;
    return i;
  }

  /* now to send our config to the new binary */
  putlog(LOG_DEBUG, "*", STR("Running for update conf: %s -4 %s"), path, conffile->file);
  argv[0] = path;
  argv[1] = "-4";
  argv[2] = conffile->file;
  argv[3] = 0;
  i = simple_exec(argv);
  delete conffile;
  if (i == -1 || WEXITSTATUS(i) != 6) { /* 6 for successfull config read/write */
    logidx(idx, STR("Couldn't pass config to new binary (error %d)"), i);
    return i;
  }

  if (movefile(path, binname)) {
    logidx(idx, STR("Can't rename %s to %s"), path, binname);
    free(path);
    return 1;
  }

  if (updating == UPDATE_EXIT) {	  /* dont restart/kill/spawn bots, just die ! */
    printf(STR("* Moved binary to: %s\n"), binname);
    fatal(STR("Binary updated."), 0);
  }
  if (updating == UPDATE_AUTO) {
    /* Make all other bots do a soft restart */
    conf_checkpids(conf.bots);
    conf_killbot(conf.bots, NULL, NULL, SIGHUP);

    if (conf.bot->pid)
      kill(conf.bot->pid, SIGHUP);
    exit(0);
  }

  if (!conf.bot->hub && secs > 0) {
    /* Make all other bots do a soft restart */
    conf_checkpids(conf.bots);
    conf_killbot(conf.bots, NULL, NULL, SIGHUP);
    
    /* invoked with -u */
    if (updating == UPDATE_AUTO) {
      if (conf.bot->pid)
        kill(conf.bot->pid, SIGHUP);
      exit(0);
    }
    /* this odd statement makes it so specifying 1 sec will restart other bots running
     * and then just restart with no delay */
    updating = UPDATE_AUTO;
    if (secs > 1) {
      egg_timeval_t howlong;
      howlong.sec = secs;
      howlong.usec = 0;
      timer_create_complex(&howlong, STR("restarting for update"), (Function) restart, (void *) (long) idx, 0);
    } else
      restart(idx);

    return 0;
  } else
    restart(idx);	/* no timer */

 /* this should never be reached */
  return 2;
}

int bot_aggressive_to(struct userrec *u)
{
  if (conf.bot->localhub) {
    for (conf_bot* bot = conf.bots; bot && bot->nick; bot = bot->next) {
      if (!bot->hub && !strcmp(u->handle, bot->nick))
        return 1;
    }
  }

  char mypval[HANDLEN + 3 + 1] = "", botpval[HANDLEN + 3 + 1] = "";

  link_pref_val(u, botpval);
  link_pref_val(conf.bot->u, mypval);
//  sdprintf("botpval: %s", botpval);
//  sdprintf("mypval: %s", mypval);
  if (strcmp(mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

int goodpass(const char *pass, int idx, char *nick)
{
  if (!pass[0]) 
    return 0;

  char tell[201] = "", last = 0;
  int nalpha = 0, lcase = 0, ucase = 0, ocase = 0, tc, repeats = 0, score = 0;
  size_t length = strlen(pass);

  if (strchr(BADPASSCHARS, pass[0])) {
    simple_snprintf(tell, sizeof(tell), "Passes may not begin with '-'");
    goto fail;
  }

  for (int i = 0; i < (signed) length; i++) {
    if (pass[i] == last)
      repeats++;

    tc = (int) pass[i];
    if (tc < 58 && tc > 47)
      ocase++; /* number */
    else if (tc < 91 && tc > 64)
      ucase++; /* upper case */
    else if (tc < 123 && tc > 96)
      lcase++; /* lower case */
    else
       nalpha++; /* non-alphabet/number */
    last = pass[i];
  }

  score += ocase * 3;
  score += nalpha * 2;
  score += ucase * 2;
  score += lcase * 1;
  score += length * 1;
  score -= repeats * 1;
  
  simple_snprintf(tell, sizeof(tell), "Password NOT set due to being too weak. Try more characters, numbers, capitals");

  if (score < 16) {
    fail:

    if (idx)
      dprintf(idx, "%s\n", tell);
    else if (nick[0])
      notice(nick, tell, DP_HELP);
    return 0;
  }
  return 1;
}

#define REPLACES 10
char *replace(const char *string, const char *oldie, const char *newbie)
{
  if (string == NULL || !string[0])
    return (char *) string;

  if (oldie == NULL || !oldie[0])
    return (char *) string;

  char *c = NULL;

  if ((c = (char *) strstr(string, oldie)) == NULL) 
    return (char *) string;

  static int n = 0;
  static char newstring_buf[REPLACES][1024];
  char *newstring = newstring_buf[n++];

  memset(newstring, 0, 1024);
  if (n == REPLACES)
    n = 0;

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

char* replace_vars(char *buf) {
  return replace(buf, "$n", botname);
}

void showhelp(int idx, struct flag_record *flags, const char *string)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  size_t help_siz = strlen(string) + 1000 + 1;
  char *helpstr = (char *) calloc(1, help_siz);
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
          simple_snprintf(tmp, sizeof(tmp), "%c", *string);
          strlcat(flagstr, tmp, sizeof(flagstr));
          string++;
        }
        string++;
        break_down_flags(flagstr, &fr, NULL);
        if (flagrec_ok(&fr, flags)) {
          ok = 1;
          while (*string && *string != '%') {
            simple_snprintf(tmp, sizeof(tmp), "%c", *string);
            strlcat(helpstr, tmp, help_siz);
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
        if (dcc[idx].u.chat->channel >= 0)
          strlcat(helpstr, settings.dcc_prefix, help_siz);
      } else if (*(string + 1) == '%') {
        string += 2;
        strlcat(helpstr, "%", help_siz);
      } else {
        if (ok) {
          simple_snprintf(tmp, sizeof(tmp), "%c", *string);
          strlcat(helpstr, tmp, help_siz);
        }
        string++;
      }
    } else {
      if (ok) {
        simple_snprintf(tmp, sizeof(tmp), "%c", *string);
        strlcat(helpstr, tmp, help_siz);
      }
      string++;
    }
  }
  helpstr[strlen(helpstr)] = 0;
  if (helpstr[0]) dumplots(idx, "", helpstr);
  free(helpstr);
}

/* Arrange the N elements of ARRAY in random order. */
void shuffleArray(char* array[], size_t n)
{
  for (size_t i = 0; i < n; i++) {
    const size_t j = randint(n);
    char* temp = array[j];
    array[j] = array[i];
    array[i] = temp;
  }
}

void shuffle(char *string, char *delim, size_t str_len)
{
  char *array[501], *str = NULL, *work = NULL;
  size_t len = 0;

  bzero(&array, sizeof array);
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
    strlcat(string, array[i], str_len);
    if (i != len - 1)
      strlcat(string, delim, str_len);
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
  } else if (idx >= 0) {
    if (dcc[idx].irc || dcc[idx].bot) {
      return 0;
    } else if ((dcc[idx].status & STAT_COLOR) && (dcc[idx].type && dcc[idx].type != &DCC_RELAYING)) {
      /* telnet probably wants ANSI, even though it might be a relay from an mIRC client */
      if (dcc[idx].status & STAT_TELNET || dcc[idx].sock == STDOUT)
        return 1;
      /* non-telnet is probably a /dcc-chat, most irc clients support mIRC codes... */
      else
        return 2;
    }
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
//    return ansi ? "\033[22m" : "\002";
    return ansi ? "\033[0m" : "\002";
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
  size_t ret_siz = 0;

  while (!feof(fd)) {
    if (fgets(tempBuf, sizeof(tempBuf), fd) && !feof(fd)) {
      if (retStr == NULL) {
        ret_siz = strlen(tempBuf) + 2;
        retStr = (char *) calloc(1, ret_siz);
        strlcpy(retStr, tempBuf, ret_siz);
      } else {
        ret_siz = strlen(retStr) + strlen(tempBuf);
        retStr = (char *) realloc(retStr, ret_siz);
        strlcat(retStr, tempBuf, ret_siz);
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

  if (strstr(line, "*/"))
    multi = 0;

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
/* vim: set sts=2 sw=2 ts=8 et: */
