/*
 * log.c -- handles:
 *
 *   What else?!
 *
 */

#include "common.h"
#include "log.h"
#include "tandem.h"
#include "color.h"
#include "userrec.h"
#include "botnet.h"
#include "botmsg.h"
#include "dcc.h"
#include "dccutil.h"
#include "rfc1459.h"
#include "users.h"
#include "misc.h"
#include "main.h"

#include <ctype.h>
#include <stdarg.h>

int	conmask = LOG_MODES | LOG_CMDS | LOG_MISC; /* Console mask */
int	debug_output = 1;      /* Disply output to server to LOG_SERVEROUT */
int 	use_console_r = 1;      /* Allow users to set their console +r  */

typedef struct {
	int flag;
	unsigned char c;
	char *type;
} logmode_mapping_t;

static logmode_mapping_t logmode_mappings[] = {
	{LOG_MSGS, 'm', "msgs"},
	{LOG_PUBLIC, 'p', "public"},
	{LOG_JOIN, 'j', "joins"},
	{LOG_MODES, 'k', "kicks/modes"},
	{LOG_CMDS, 'c', "cmds"},
	{LOG_MISC, 'o', "misc"},
	{LOG_BOTS, 'b', "bots"},
	{LOG_RAW, 'r', "raw"},
	{LOG_WALL, 'w', "wallops"},
	{LOG_FILES, 'x', "files"},
	{LOG_SERV, 's', "server"},
	{LOG_DEBUG, 'd', "debug"},
	{LOG_SRVOUT, 'v', "server output"},
	{LOG_BOTNET, 't', "botnet traffic"},
	{LOG_BOTSHARE, 'h', "share traffic"},
	{LOG_ERRORS, 'e', "errors"},
	{LOG_GETIN, 'g', "getin"},
	{LOG_WARN, 'u', "warnings"},
	{0, 0, NULL}
};
#define LOG_LEVELS 18 		/* change this if you change the levels */

#define NEEDS_DEBUG_OUTPUT (LOG_RAW|LOG_SRVOUT|LOG_BOTNET|LOG_BOTSHARE)


int logmodes(const char *s)
{
	logmode_mapping_t *mapping = NULL;
	int modes = 0;
	while (*s) {
		if (*s == '*') return(LOG_ALL);
		for (mapping = logmode_mappings; mapping->type; mapping++) {
			if (mapping->c == tolower(*s)) break;
		}
		if (mapping->type) modes |= mapping->flag;
		s++;
	}
	return(modes);
}

char *masktype(int x)
{
	static char s[LOG_LEVELS + 1];	
	char *p = s;
	logmode_mapping_t *mapping = NULL;

	for (mapping = logmode_mappings; mapping->type; mapping++) {
		if (x & mapping->flag) {
			if ((mapping->flag & NEEDS_DEBUG_OUTPUT) && !debug_output) continue;
			*p++ = mapping->c;
		}
	}
	if (p == s) *p++ = '-';
	*p = 0;
	return(s);
}

char *maskname(int x)
{
	static char s[1024] = "";
	logmode_mapping_t *mapping = NULL;
	int len;

	*s = 0;
	for (mapping = logmode_mappings; mapping->type; mapping++) {
		if (x & mapping->flag) {
			if ((mapping->flag & NEEDS_DEBUG_OUTPUT) && !debug_output) continue;
			strcat(s, mapping->type);
			strcat(s, ", ");
		}
	}
	len = strlen(s);
	if (len) s[len-2] = 0;
	else strcpy(s, "none");
	return(s);
}


/*
 *    Logging functions
 */

void logidx(int idx, const char *format, ...)
{
  char va_out[LOGLINEMAX + 1];
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, LOGLINEMAX, format, va);
  va_end(va);

  if (idx < 0)
    putlog(LOG_DEBUG, "*", "%s", va_out);
  else
    dprintf(idx, "%s\n", va_out);
}

/* Log something
 * putlog(level,channel_name,format,...);
 * Broadcast the log if chname is not '@'
 */
void putlog(int type, const char *chname, const char *format, ...)
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

void
irc_log(struct chanset_t *chan, const char *format, ...)
{
  return;		/* dont spam this to 1.1.8 nets */
#ifdef NEW
  char va_out[LOGLINEMAX + 1];
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, LOGLINEMAX, format, va);
  va_end(va);

//  if (egg_strcasecmp(chan->dname, TO))
//    dprintf(DP_HELP, "PRIVMSG %s :[%s] %s\n", TO, chan->dname, va_out);

//  chanout_but(-1, 1, "[%s] %s\n", chan->dname, va_out);
//  botnet_send_chan(-1, conf.bot->nick, chan->dname, 1, va_out);
   if (chan)
     putlog(LOG_PUBLIC, "*", "[%s] %s", chan->dname, va_out);
   else
     putlog(LOG_PUBLIC, "*", "%s", va_out);

//  sdprintf("%s", va_out);
#endif
}
