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
#include "chanprog.h"

#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int	conmask = LOG_MODES | LOG_CMDS | LOG_MISC; /* Console mask */
bool	debug_output = 1;      /* Disply output to server to LOG_SERVEROUT */

typedef struct {
	int flag;
	char *type;
	unsigned char c;
} logmode_mapping_t;

static logmode_mapping_t logmode_mappings[] = {
	{LOG_MSGS, "msgs", 'm'},
	{LOG_PUBLIC, "public", 'p'},
	{LOG_JOIN, "joins", 'j'},
	{LOG_MODES, "kicks/modes", 'k'},
	{LOG_CMDS, "cmds", 'c'},
	{LOG_MISC, "misc", 'o'},
	{LOG_BOTS, "bots", 'b'},
	{LOG_RAW, "raw", 'r'},
	{LOG_WALL, "wallops", 'w'},
	{LOG_FILES, "files", 'x'},
	{LOG_SERV, "server", 's'},
	{LOG_DEBUG, "debug", 'd'},
	{LOG_SRVOUT, "server output", 'v'},
	{LOG_BOTNET, "botnet traffic", 't'},
	{LOG_BOTSHARE, "share traffic", 'h'},
	{LOG_ERRORS, "errors", 'e'},
	{LOG_GETIN, "getin", 'g'},
	{LOG_WARN, "warnings", 'u'},
	{0, NULL, 0}
};
#define LOG_LEVELS 18 		/* change this if you change the levels */

#define NEEDS_DEBUG_OUTPUT (LOG_RAW|LOG_SRVOUT|LOG_BOTNET|LOG_BOTSHARE)


int logmodes(const char *s)
{
	const logmode_mapping_t *mapping = NULL;
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
	const logmode_mapping_t *mapping = NULL;
	int len;

	*s = 0;
	for (mapping = logmode_mappings; mapping->type; mapping++) {
		if (x & mapping->flag) {
			if ((mapping->flag & NEEDS_DEBUG_OUTPUT) && !debug_output) continue;
			strlcat(s, mapping->type, sizeof(s));
			strlcat(s, ", ", sizeof(s));
		}
	}
	len = strlen(s);
	if (len) s[len-2] = 0;
	else strlcpy(s, "none", sizeof(s));
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
  egg_vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  if (idx < 0)
    putlog(LOG_MISC, "*", "%s", va_out);
  else
    dprintf(idx, "%s\n", va_out);
}

#ifdef DEBUG
/* CURRENTLY SPAWNS TONS OF FILES */
FILE *log_f;
bool flush_log = 1;
bool init_log_exit = 0;
char log_last[LOGLINEMAX + 1] = "";
int repeats = 0;

void logfile_close(void)
{
  if (!log_f)
    return;

  char date[50] = "";

  strftime(date, sizeof date, "%c %Z", gmtime(&now));
  fprintf(log_f, "--- Log session end: %s ---\n", date);
  fclose(log_f);
  log_f = NULL;
}

bool logfile_open()
{
  if (!conf.bot || !conf.bot->nick) return 0;

  char filename[20] = "";
  simple_snprintf(filename, sizeof(filename), ".l-%s", conf.bot->nick);

  if (!(log_f = fopen(filename, "w")))
    return 0;

  if (!init_log_exit) {
    init_log_exit = 1;
    atexit(logfile_close);
  } 

  char date[50] = "";

  strftime(date, sizeof date, "%c %Z", gmtime(&now));
  fprintf(log_f, "--- Log session begin: %s ---\n", date);
  return 1;
}

#ifdef unused
bool logfile_stat(const char *fname)
{
  struct stat st;
  int fd = open(fname, O_RDONLY);

  if (fd == -1 || fstat(fd, &st) < 0) {
    if (fd != -1)
      close(fd);
    fclose(log_f);
    log_f = NULL;
    if (!logfile_open())
      return 0;
  }
  return 1;
}
#endif

void logfile(int type, const char *msg)
{
  // Only log if this is an actual bot and not a startup/cron proc
  if (!safe_to_log || (!log_f && !logfile_open()))
    return;

//  if (!logfile_stat(".l"))
//    return;

  if (!strncasecmp(msg, log_last, sizeof(log_last))) {
    repeats++;
    return;
  }
  if (repeats) {
    fprintf(log_f, "Last message repeated %d times.\n", repeats);
    repeats = 0;
  }

  strlcpy(log_last, msg, sizeof(log_last));

  fprintf(log_f, "%s\n", msg);
  if (flush_log)
    fflush(log_f);
}
#endif

/* Log something
 * putlog(level,channel_name,format,...);
 * Broadcast the log if chname is not '@'
 */
char last_log[LOGLINEMAX + 1] = "";
int log_repeats = 0;
char last_chname[25] = "";
int last_type;
bool log_repeated;

void putlog(int type, const char *chname, const char *format, ...)
{
  char va_out[LOGLINEMAX + 1] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  if (!va_out[0]) {
    putlog(LOG_ERRORS, "*", "Empty putlog() detected");
    return;
  }

  if (!log_repeated) {
    if (type == last_type && 
        !strncasecmp(chname, last_chname, sizeof(last_chname)) &&
        !strncasecmp(va_out, last_log, sizeof(last_log))) {
      ++log_repeats;

      return;
    }
    if (log_repeats) {
      log_repeated = 1;
      putlog(type, last_chname, "Last message repeated %d times.\n", log_repeats);
      log_repeats = 0;
    }
    strlcpy(last_log, va_out, sizeof(last_log));
    last_type = type;
    strlcpy(last_chname, chname, sizeof(last_chname));
  } else
    log_repeated = 0;

  char *p = NULL;

  if ((p = strchr(va_out, '\n')))		/* make sure no trailing newline */
     *p = 0;

  int idx = 0;
  char out[LOGLINEMAX + 1] = "";

  if (conf.bot && conf.bot->hub) {
    char stamp[34] = "";
    time_t synced = now + timesync;
    struct tm *t = gmtime(&synced);

    strftime(stamp, sizeof(stamp), LOG_TS, t);
    /* Place the timestamp in the string to be printed */
    strlcpy(out, stamp, sizeof(out));
    strlcat(out, " ", sizeof(out));
    strlcat(out, va_out, sizeof(out));
  } else
    strlcpy(out, va_out, sizeof(out));

  /* strcat(out, "\n"); */

#ifdef DEBUG
  /* FIXME: WRITE LOG HERE */
  int logfile_masks = LOG_ALL;

  if ((logfile_masks && (logfile_masks & type)) || 1)
    logfile(type, out);
#endif
  /* broadcast to hubs */
  if (chname[0] == '*' && conf.bot && conf.bot->nick)
    botnet_send_log(-1, conf.bot->nick, type, out, (conf.bot->hub ? 1 : 0));

  for (idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && (dcc[idx].type == &DCC_CHAT && dcc[idx].simul == -1) && (dcc[idx].u.chat->con_flags & type)) {
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

#if 0
void
irc_log(struct chanset_t *chan, const char *format, ...)
{
#ifdef NOTHANKS
  char va_out[LOGLINEMAX + 1];
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  if ((chan && strcasecmp(chan->dname, relay_chan)) || !chan) {
    bd::String msg;
    msg = bd::String::printf("[%s] %s", chan ? chan->dname : "*" , va_out);
    privmsg(relay_chan, msg, DP_HELP);
  }
/*
  chanout_but(-1, 1, "[%s] %s\n", chan->dname, va_out);
  botnet_send_chan(-1, conf.bot->nick, chan->dname, 1, va_out);
   if (chan)
     putlog(LOG_PUBLIC, "*", "[%s] %s", chan->dname, va_out);
   else
     putlog(LOG_PUBLIC, "*", "%s", va_out);
  sdprintf("%s", va_out);
*/
#endif 
}
#endif
/* vim: set sts=2 sw=2 ts=8 et: */
