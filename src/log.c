/*
 * log.c -- handles:
 *
 *   What else?!
 *
 */

#include "common.h"
#include "log.h"
#include "tandem.h"
#include "botmsg.h"
#include "dcc.h"
#include "dccutil.h"
#include "rfc1459.h"
#include "users.h"
#include "misc.h"
#include "main.h"

#include <stdarg.h>

extern struct userrec   *userlist;
extern tand_t           *tandbot;
extern struct dcc_t     *dcc;
extern int               debug_output, backgrd, term_z, 
			 dcc_total, loading;
extern struct dcc_t     *dcc;
extern time_t		now;


int	conmask = LOG_MODES | LOG_CMDS | LOG_MISC; /* Console mask */
int	debug_output = 1;      /* Disply output to server to LOG_SERVEROUT */
int 	use_console_r = 1;      /* Allow users to set their console +r  */

int logmodes(char *s)
{
  int i;
  int res = 0;

  for (i = 0; i < strlen(s); i++)
    switch (s[i]) {
    case 'm':
    case 'M':
      res |= LOG_MSGS;
      break;
    case 'p':
    case 'P':
      res |= LOG_PUBLIC;
      break;
    case 'j':
    case 'J':
      res |= LOG_JOIN;
      break;
    case 'k':
    case 'K':
      res |= LOG_MODES;
      break;
    case 'c':
    case 'C':
      res |= LOG_CMDS;
      break;
    case 'o':
    case 'O':
      res |= LOG_MISC;
      break;
    case 'b':
    case 'B':
      res |= LOG_BOTS;
      break;
    case 'r':
    case 'R':
      res |= use_console_r ? LOG_RAW : 0;
      break;
    case 'w':
    case 'W':
      res |= LOG_WALL;
      break;
    case 'x':
    case 'X':
      res |= LOG_FILES;
      break;
    case 's':
    case 'S':
      res |= LOG_SERV;
      break;
    case 'd':
    case 'D':
      res |= LOG_DEBUG;
      break;
    case 'v':
    case 'V':
      res |= debug_output ? LOG_SRVOUT : 0;
      break;
    case 't':
    case 'T':
      res |= debug_output ? LOG_BOTNET : 0;
      break;
    case 'h':
    case 'H':
      res |= debug_output ? LOG_BOTSHARE : 0;
      break;
    case 'e':
    case 'E': 
      res |= LOG_ERRORS;
      break;
    case 'g':
    case 'G':
      res |= LOG_GETIN;
      break;
    case 'u':
    case 'U':
      res |= LOG_WARN;
      break;
    case '*':
      res |= LOG_ALL;
      break;
    }
  return res;
}

char *masktype(int x)
{
  static char s[24] = "";		/* Change this if you change the levels */
  char *p = s;

  if (x & LOG_MSGS)
    *p++ = 'm';
  if (x & LOG_PUBLIC)
    *p++ = 'p';
  if (x & LOG_JOIN)
    *p++ = 'j';
  if (x & LOG_MODES)
    *p++ = 'k';
  if (x & LOG_CMDS)
    *p++ = 'c';
  if (x & LOG_MISC)
    *p++ = 'o';
  if (x & LOG_BOTS)
    *p++ = 'b';
  if ((x & LOG_RAW) && use_console_r)
    *p++ = 'r';
  if (x & LOG_FILES)
    *p++ = 'x';
  if (x & LOG_SERV)
    *p++ = 's';
  if (x & LOG_DEBUG)
    *p++ = 'd';
  if (x & LOG_WALL)
    *p++ = 'w';
  if ((x & LOG_SRVOUT) && debug_output)
    *p++ = 'v';
  if ((x & LOG_BOTNET) && debug_output)
    *p++ = 't';
  if ((x & LOG_BOTSHARE) && debug_output)
    *p++ = 'h';
  if (x & LOG_ERRORS)
    *p++ = 'e';
  if (x & LOG_GETIN)
    *p++ = 'g';
  if (x & LOG_WARN)
    *p++ = 'u';
  if (p == s)
    *p++ = '-';
  *p = 0;
  return s;
}

char *maskname(int x)
{
  static char s[207] = "";		/* Change this if you change the levels */
  int i = 0;

  s[0] = 0;
  if (x & LOG_MSGS)
    i += my_strcpy(s, "msgs, ");
  if (x & LOG_PUBLIC)
    i += my_strcpy(s + i, "public, ");
  if (x & LOG_JOIN)
    i += my_strcpy(s + i, "joins, ");
  if (x & LOG_MODES)
    i += my_strcpy(s + i, "kicks/modes, ");
  if (x & LOG_CMDS)
    i += my_strcpy(s + i, "cmds, ");
  if (x & LOG_MISC)
    i += my_strcpy(s + i, "misc, ");
  if (x & LOG_BOTS)
    i += my_strcpy(s + i, "bots, ");
  if ((x & LOG_RAW) && use_console_r)
    i += my_strcpy(s + i, "raw, ");
  if (x & LOG_FILES)
    i += my_strcpy(s + i, "files, ");
  if (x & LOG_SERV)
    i += my_strcpy(s + i, "server, ");
  if (x & LOG_DEBUG)
    i += my_strcpy(s + i, "debug, ");
  if (x & LOG_WALL)
    i += my_strcpy(s + i, "wallops, ");
  if ((x & LOG_SRVOUT) && debug_output)
    i += my_strcpy(s + i, "server output, ");
  if ((x & LOG_BOTNET) && debug_output)
    i += my_strcpy(s + i, "botnet traffic, ");
  if ((x & LOG_BOTSHARE) && debug_output)
    i += my_strcpy(s + i, "share traffic, ");
  if (x & LOG_ERRORS)
    i += my_strcpy(s + i, "errors, ");
  if (x & LOG_GETIN)
    i += my_strcpy(s + i, "getin, ");
  if (x & LOG_WARN)
    i += my_strcpy(s + i, "warnings, ");
  if (i)
    s[i - 2] = 0;
  else
    strcpy(s, "none");
  return s;
}


/*
 *    Logging functions
 */

/* Log something
 * putlog(level,channel_name,format,...);
 * Broadcast the log if chname is not '@'
 */
void putlog(int type, char *chname, char *format, ...)
{
  int idx = 0;
  char va_out[LOGLINEMAX + 1] = "", out[LOGLINEMAX + 1] = "", *p = NULL;
#ifdef HUB
  char stamp[34] = "";
  struct tm *t = NULL;
#endif /* HUB */
  va_list va;

  va_start(va, format);
#ifdef HUB
# ifdef S_UTCTIME
  t = gmtime(&now);
# else /* !S_UTCTIME */
  t = localtime(&now);
# endif /* S_UTCTIME */

  egg_strftime(stamp, sizeof(stamp), LOG_TS, t);
#endif /* HUB */

  /* No need to check if out should be null-terminated here,
   * just do it! <cybah>
   */
  egg_vsnprintf(va_out, LOGLINEMAX, format, va);
  va_end(va);

  if (!va_out[0]) {
    putlog(LOG_ERRORS, "*", "Empty putlog() detected");
    return;
  }

  if ((p = strchr(va_out, '\n')))		/* make sure no trailing newline */
     *p = 0;


#ifdef HUB
  /* Place the timestamp in the string to be printed */
  if (stamp[0])
    egg_snprintf(out, sizeof out, "%s %s", stamp, va_out);
#endif /* HUB */
#ifdef LEAF
  egg_snprintf(out, sizeof out, "%s", va_out);
#endif /* LEAF */

  /* strcat(out, "\n"); */

  /* FIXME: WRITE LOG HERE */

  /* broadcast to hubs */
  if (chname[0] == '*' && conf.bot && conf.bot->nick) {
    char outbuf[LOGLINEMAX + 1] = "";

    egg_snprintf(outbuf, sizeof outbuf, "hl %d %s", type, out);
    if (userlist && !loading) {
      tand_t *bot = NULL;
      struct userrec *ubot = NULL;

      for (bot = tandbot; bot; bot = bot->next) {
        if ((ubot = get_user_by_handle(userlist, bot->bot))) {
          if (bot_hublevel(ubot) < 999)
            putbot(ubot->handle, outbuf);
        }
      }
    } else {
      putallbots(outbuf);
    }
  }

  for (idx = 0; idx < dcc_total; idx++) {
    if ((dcc[idx].type == &DCC_CHAT && !dcc[idx].simul) && (dcc[idx].u.chat->con_flags & type)) {
      if ((chname[0] == '@') || (chname[0] == '*') || (dcc[idx].u.chat->con_chan[0] == '*') ||
          (!rfc_casecmp(chname, dcc[idx].u.chat->con_chan)))
        dprintf(idx, "%s\n", out);
    }
  }

  if ((!backgrd) && (!term_z)) {
    dprintf(DP_STDOUT, "%s\n", out);
  } else if ((type & LOG_ERRORS || type & LOG_MISC) && use_stderr) {
    dprintf(DP_STDERR, "%s\n", va_out);
  }
}

