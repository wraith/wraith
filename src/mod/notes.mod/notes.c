/*
 * notes.c -- part of notes.mod
 *   reading and sending notes
 *   killing old notes and changing the destinations
 *   note cmds
 *   note ignores
 *
 */

#define MODULE_NAME "notes"
#define MAKING_NOTES
#include <fcntl.h>
#include <sys/stat.h> /* chmod(..) */
#include "src/mod/module.h"
#include "src/tandem.h"
#include "src/botmsg.h" /* NOTE DEFINES */
#undef global
#include "notes.h"

static int maxnotes = 50;	/* Maximum number of notes to allow stored
				 * for each user */
static int note_life = 60;	/* Number of DAYS a note lives */
static char notefile[121] = ".n";	/* Name of the notefile */
static int allow_fwd = 1;	/* Allow note forwarding */
static int notify_users = 1;	/* Notify users they have notes every hour? */
static Function *global = NULL;	/* DAMN fcntl.h */

static struct user_entry_type USERENTRY_FWD =
{
  NULL,				/* always 0 ;) */
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  fwd_display,
  "FWD"
};

#include "cmdsnote.c"


static void fwd_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (dcc[idx].user && (dcc[idx].user->flags & USER_MASTER))
    dprintf(idx, NOTES_FORWARD_TO, e->u.string);
}

/* Determine how many notes are waiting for a user.
 */
static int num_notes(char *user)
{
  int tot = 0;
  FILE *f;
  char s[513], *to, *s1;

  if (!notefile[0])
    return 0;
  f = fopen(notefile, "r");
  if (f == NULL)
    return 0;
  while (!feof(f)) {
    fgets(s, 512, f);
    if (!feof(f)) {
      if (s[strlen(s) - 1] == '\n')
	s[strlen(s) - 1] = 0;
      rmspace(s);
      if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* Not comment */
	s1 = s;
	to = newsplit(&s1);
	if (!egg_strcasecmp(to, user))
	  tot++;
      }
    }
  }
  fclose(f);
  return tot;
}

/* Change someone's handle.
 */
static void notes_change(char *oldnick, char *newnick)
{
  FILE *f, *g;
  char s[513], *to, *s1;
  int tot = 0;

  if (!egg_strcasecmp(oldnick, newnick))
    return;
  if (!notefile[0])
    return;
  f = fopen(notefile, "r");
  if (f == NULL)
    return;
  sprintf(s, "%s~new", notefile);
  g = fopen(s, "w");
  if (g == NULL) {
    fclose(f);
    return;
  }
  chmod(s, userfile_perm);	/* Use userfile permissions. */
  while (!feof(f)) {
    fgets(s, 512, f);
    if (!feof(f)) {
      if (s[strlen(s) - 1] == '\n')
	s[strlen(s) - 1] = 0;
      rmspace(s);
      if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* Not comment */
	s1 = s;
	to = newsplit(&s1);
	if (!egg_strcasecmp(to, oldnick)) {
	  tot++;
	  fprintf(g, "%s %s\n", newnick, s1);
	} else
	  fprintf(g, "%s %s\n", to, s1);
      } else
	fprintf(g, "%s\n", s);
    }
  }
  fclose(f);
  fclose(g);
  unlink(notefile);
  sprintf(s, "%s~new", notefile);
  movefile(s, notefile);
  putlog(LOG_MISC, "*", NOTES_SWITCHED_NOTES, tot, tot == 1 ? "" : "s",
         oldnick, newnick);
}

/* Get rid of old useless notes.
 */
static void expire_notes()
{
  FILE *f, *g;
  char s[513], *to, *from, *ts, *s1;
  int tot = 0, lapse;

  if (!notefile[0])
    return;
  f = fopen(notefile, "r");
  if (f == NULL)
    return;
  sprintf(s, "%s~new", notefile);
  g = fopen(s, "w");
  if (g == NULL) {
    fclose(f);
    return;
  }
  chmod(s, userfile_perm);	/* Use userfile permissions. */
  while (!feof(f)) {
    fgets(s, 512, f);
    if (!feof(f)) {
      if (s[strlen(s) - 1] == '\n')
	s[strlen(s) - 1] = 0;
      rmspace(s);
      if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* Not comment */
	s1 = s;
	to = newsplit(&s1);
	from = newsplit(&s1);
	ts = newsplit(&s1);
	lapse = (now - (time_t) atoi(ts)) / 86400;
	if (lapse > note_life)
	  tot++;
	else if (!get_user_by_handle(userlist, to))
	  tot++;
	else
	  fprintf(g, "%s %s %s %s\n", to, from, ts, s1);
      } else
	fprintf(g, "%s\n", s);
    }
  }
  fclose(f);
  fclose(g);
  unlink(notefile);
  sprintf(s, "%s~new", notefile);
  movefile(s, notefile);
  if (tot > 0)
    putlog(LOG_MISC, "*", NOTES_EXPIRED, tot, tot == 1 ? "" : "s");
}

/* Add note to notefile.
 */
static int storenote(char *argv1, char *argv2, char *argv3, int idx, char *who, int bufsize)
{
  FILE *f;
  char u[20], *f1, *to = NULL, work[1024];
  struct userrec *ur;
  struct userrec *ur2;

  if (who && bufsize > 0) who[0] = 0;
  ur = get_user_by_handle(userlist, argv2);
  if (ur && allow_fwd && (f1 = get_user(&USERENTRY_FWD, ur))) {
    char fwd[161], fwd2[161], *f2, *p, *q, *r;
    int ok = 1;
    /* User is valid & has a valid forwarding address */
     strcpy(fwd, f1);		/* Only 40 bytes are stored in the userfile */
     p = strchr(fwd, '@');
    if (p && !egg_strcasecmp(p + 1, botnetnick)) {
      *p = 0;
      if (!egg_strcasecmp(fwd, argv2))
	/* They're forwarding to themselves on the same bot, llama's */
	ok = 0;
      strcpy(fwd2, fwd);
      splitc(fwd2, fwd2, '@');
      /* Get the user record of the user that we're forwarding to locally */
      ur2 = get_user_by_handle(userlist, fwd2);
      if (!ur2)
	ok = 0;
      if ((f2 = get_user(&USERENTRY_FWD, ur2))) {
	strcpy(fwd2, f2);
	splitc(fwd2, fwd2, '@');
	if (!egg_strcasecmp(fwd2, argv2))
	/* They're forwarding to someone who forwards back to them! */
	ok = 0;
      }
      p = NULL;
    }
    if ((argv1[0] != '@') && ((argv3[0] == '<') || (argv3[0] == '>')))
       ok = 0;			/* Probablly fake pre 1.3 hax0r */

    if (ok && (!p || in_chain(p + 1))) {
      if (p)
	p++;
      q = argv3;
      while (ok && q && (q = strchr(q, '<'))) {
	q++;
	if ((r = strchr(q, ' '))) {
	  *r = 0;
	  if (!egg_strcasecmp(fwd, q))
	    ok = 0;
	  *r = ' ';
	}
      }
      if (ok) {
	if (p && strchr(argv1, '@')) {
	  simple_sprintf(work, "<%s@%s >%s %s", argv2, botnetnick,
			 argv1, argv3);
	  simple_sprintf(u, "@%s", botnetnick);
	  p = u;
	} else {
	  simple_sprintf(work, "<%s@%s %s", argv2, botnetnick,
			 argv3);
	  p = argv1;
	}
      }
    } else
      ok = 0;
    if (ok) {
      if ((add_note(fwd, p, work, idx, 0) == NOTE_OK) && (idx >= 0))
	dprintf(idx, "Not online; forwarded to %s.\n", f1);
      if (who) strncpy(who, f1, bufsize);
      to = NULL;
    } else {
      strcpy(work, argv3);
      to = argv2;
    }
  } else
    to = argv2;
  if (to) {
    if (notefile[0] == 0) {
      if (idx >= 0)
	dprintf(idx, "%s\n", "Notes are not supported by this bot.");
    } else if (num_notes(to) >= maxnotes) {
      if (idx >= 0)
	dprintf(idx, "%s\n", "Sorry, that user has too many notes already.");
    } else {			/* Time to unpack it meaningfully */
      f = fopen(notefile, "a");
      if (f == NULL)
	f = fopen(notefile, "w");
      if (f == NULL) {
	if (idx >= 0)
	  dprintf(idx, "%s\n", "Cant create notefile.  Sorry.");
	putlog(LOG_MISC, "*", "%s", "Notefile unreachable!");
      } else {
	char *p, *from = argv1;
	int l = 0;

	chmod(notefile, userfile_perm);	/* Use userfile permissions. */
	while ((argv3[0] == '<') || (argv3[0] == '>')) {
	  p = newsplit(&(argv3));
	  if (*p == '<')
	    l += simple_sprintf(work + l, "via %s, ", p + 1);
	  else if (argv1[0] == '@')
	    from = p + 1;
	}
	fprintf(f, "%s %s %lu %s%s\n", to, from, now,
		l ? work : "", argv3);
	fclose(f);
	if (idx >= 0)
	  dprintf(idx, "%s.\n", "Stored message");
      }
    }
  }
  return 0;
}

/* Convert a string like "2-4;8;16-"
 * in an array {2, 4, 8, 8, 16, maxnotes, -1}
 */
static void notes_parse(int dl[], char *s)
{
  int i = 0;
  int idl = 0;

  do {
    while (s[i] == ';')
      i++;
    if (s[i]) {
      if (s[i] == '-')
	dl[idl] = 1;
      else
	dl[idl] = atoi(s + i);
      idl++;
      while ((s[i]) && (s[i] != '-') && (s[i] != ';'))
	i++;
      if (s[i] == '-') {
	dl[idl] = atoi(s + i + 1);	/* Will be 0 if not a number */
	if (dl[idl] == 0)
	  dl[idl] = maxnotes;
      } else
	dl[idl] = dl[idl - 1];
      idl++;
      while ((s[i]) && (s[i] != ';'))
	i++;
    }
  }
  while ((s[i]) && (idl < 124));
  dl[idl] = -1;
}

/* Return true if 'in' is in intervals of 'dl'
 */
static int notes_in(int dl[], int in)
{
  int i = 0;

  while (dl[i] != -1) {
    if ((dl[i] <= in) && (in <= dl[i + 1]))
      return 1;
    i += 2;
  }
  return 0;
}

/*
 * srd="+" : index
 * srd="-" : read all msgs
 * else    : read msg in list : (ex: .notes read 5-9;12;13;18-)
 * idx=-1  : /msg
 */
static void notes_read(char *hand, char *nick, char *srd, int idx)
{
  FILE *f;
  char s[601], *to, *dt, *from, *s1, wt[100];
  time_t tt;
  int ix = 1;
  int ir = 0;
  int rd[128];			/* Is it enough ? */
  int i;

  if (srd[0] == 0)
    srd = "-";
  if (!notefile[0]) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
    return;
  }
  f = fopen(notefile, "r");
  if (f == NULL) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
    return;
  }
  notes_parse(rd, srd);
  while (!feof(f)) {
    fgets(s, 600, f);
    i = strlen(s);
    if (i > 0 && s[i - 1] == '\n')
      s[i - 1] = 0;
    if (!feof(f)) {
      rmspace(s);
      if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* Not comment */
	s1 = s;
	to = newsplit(&s1);
	if (!egg_strcasecmp(to, hand)) {
	  int lapse;

	  from = newsplit(&s1);
	  dt = newsplit(&s1);
	  tt = atoi(dt);
#ifdef S_UTCTIME
	  egg_strftime(wt, sizeof wt, "%b %d %H:%M %Z", gmtime(&tt));
#else /* !S_UTCTIME */
	  egg_strftime(wt, sizeof wt, "%b %d %H:%M %Z", localtime(&tt));
#endif /* S_UTCTIME */
	  dt = wt;
	  lapse = (int) ((now - tt) / 86400);
	  if (lapse > note_life - 7) {
	    if (lapse >= note_life)
	      strcat(dt, NOTES_EXPIRE_TODAY);
	    else
	      sprintf(&dt[strlen(dt)], NOTES_EXPIRE_XDAYS, note_life - lapse,
		      (note_life - lapse) == 1 ? "" : "S");
	  }
	  if (srd[0] == '+') {
	    if (idx >= 0) {
	      if (ix == 1)
		dprintf(idx, "### %s:\n", NOTES_WAITING);
	      dprintf(idx, "  %2d. %s (%s)\n", ix, from, dt);
	    } else {
	      dprintf(DP_HELP, "NOTICE %s :%2d. %s (%s)\n",
		      nick, ix, from, dt);
	    }
	  } else if (notes_in(rd, ix)) {
	    if (idx >= 0)
	      dprintf(idx, "%2d. %s (%s): %s\n", ix, from, dt, s1);
	    else
	      dprintf(DP_HELP, "NOTICE %s :%2d. %s (%s): %s\n",
		      nick, ix, from, dt, s1);
	    ir++;
	  }
	  ix++;
	}
      }
    }
  }
  fclose(f);
  if ((srd[0] != '+') && (ir == 0) && (ix > 1)) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NOT_THAT_MANY);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NOT_THAT_MANY);
  }
  if (srd[0] == '+') {
    if (ix == 1) {
      if (idx >= 0)
	dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
      else
	dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
    } else {
      if (idx >= 0)
	dprintf(idx, "### %s.\n", NOTES_DCC_USAGE_READ);
      else
	dprintf(DP_HELP, "NOTICE %s :(%d %s)\n", nick, ix - 1, MISC_TOTAL);
    }
  } else if ((ir == 0) && (ix == 1)) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
  }
}

/*
 * sdl="-" : erase all msgs
 * else    : erase msg in list : (ex: .notes erase 2-4;8;16-)
 * idx=-1  : /msg
 */
static void notes_del(char *hand, char *nick, char *sdl, int idx)
{
  FILE *f, *g;
  char s[513], *to, *s1;
  int in = 1;
  int er = 0;
  int dl[128];			/* Is it enough ? */

  if (sdl[0] == 0)
    sdl = "-";
  if (!notefile[0]) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
    return;
  }
  f = fopen(notefile, "r");
  if (f == NULL) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
    return;
  }
  sprintf(s, "%s~new", notefile);
  g = fopen(s, "w");
  if (g == NULL) {
    if (idx >= 0)
      dprintf(idx, "%s. :(\n", NOTES_FAILED_CHMOD);
    else
      dprintf(DP_HELP, "NOTICE %s :%s. :(\n", nick, NOTES_FAILED_CHMOD);
    fclose(f);
    return;
  }
  chmod(s, userfile_perm);	/* Use userfile permissions. */
  notes_parse(dl, sdl);
  while (!feof(f)) {
    fgets(s, 512, f);
    if (s[strlen(s) - 1] == '\n')
      s[strlen(s) - 1] = 0;
    if (!feof(f)) {
      rmspace(s);
      if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* Not comment */
	s1 = s;
	to = newsplit(&s1);
	if (!egg_strcasecmp(to, hand)) {
	  if (!notes_in(dl, in))
	    fprintf(g, "%s %s\n", to, s1);
	  else
	    er++;
	  in++;
	} else
	  fprintf(g, "%s %s\n", to, s1);
      } else
	fprintf(g, "%s\n", s);
    }
  }
  fclose(f);
  fclose(g);
  unlink(notefile);
  sprintf(s, "%s~new", notefile);
  movefile(s, notefile);
  if ((er == 0) && (in > 1)) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NOT_THAT_MANY);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NOT_THAT_MANY);
  } else if (in == 1) {
    if (idx >= 0)
      dprintf(idx, "%s.\n", NOTES_NO_MESSAGES);
    else
      dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_NO_MESSAGES);
  } else {
    if (er == (in - 1)) {
      if (idx >= 0)
	dprintf(idx, "%s.\n", NOTES_ERASED_ALL);
      else
	dprintf(DP_HELP, "NOTICE %s :%s.\n", nick, NOTES_ERASED_ALL);
    } else {
      if (idx >= 0)
	dprintf(idx, "%s %d note%s; %d %s.\n", NOTES_ERASED, er,
		(er != 1) ? "s" : "", in - 1 - er, NOTES_LEFT);
      else
	dprintf(DP_HELP, "NOTICE %s :%s %d note%s; %d %s.\n", nick, MISC_ERASED,
		er, (er != 1) ? "s" : "", in - 1 - er, NOTES_LEFT);
    }
  }
}

/* notes <pass> <func>
 */
static int msg_notes(char *nick, char *host, struct userrec *u, char *par)
{
  char *pwd, *fcn;

  if (!u)
    return 0;
  if (u->flags & (USER_BOT))
    return 1;
  if (!par[0]) {
    dprintf(DP_HELP, "NOTICE %s :%s: NOTES <pass> INDEX\n", nick, NOTES_USAGE);
    dprintf(DP_HELP, "NOTICE %s :       NOTES <pass> TO <hand> <msg>\n", nick);
    dprintf(DP_HELP, "NOTICE %s :       NOTES <pass> READ <# or ALL>\n", nick);
    dprintf(DP_HELP, "NOTICE %s :       NOTES <pass> ERASE <# or ALL>\n", nick);
    dprintf(DP_HELP, "NOTICE %s :       %s\n", nick, NOTES_MAYBE);
    dprintf(DP_HELP, "NOTICE %s :       ex: NOTES mypass ERASE 2-4;8;16-\n", nick);
    return 1;
  }
  if (!u_pass_match(u, "-")) {
    /* they have a password set */
    pwd = newsplit(&par);
    if (!u_pass_match(u, pwd))
      return 0;
  }
  fcn = newsplit(&par);
  if (!egg_strcasecmp(fcn, "INDEX"))
    notes_read(u->handle, nick, "+", -1);
  else if (!egg_strcasecmp(fcn, "READ")) {
    if (!egg_strcasecmp(par, "ALL"))
      notes_read(u->handle, nick, "-", -1);
    else
      notes_read(u->handle, nick, par, -1);
  } else if (!egg_strcasecmp(fcn, "ERASE")) {
    if (!egg_strcasecmp(par, "ALL"))
      notes_del(u->handle, nick, "-", -1);
    else
      notes_del(u->handle, nick, par, -1);
  } else if (!egg_strcasecmp(fcn, "TO")) {
    char *to;
    int i;
    FILE *f;
    struct userrec *u2;

    to = newsplit(&par);
    if (!par[0]) {
      dprintf(DP_HELP, "NOTICE %s :%s: NOTES <pass> TO <hand> <message>\n",
	      nick, NOTES_USAGE);
      return 0;
    }
    u2 = get_user_by_handle(userlist, to);
    if (!u2) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, NOTES_USERF_UNKNOWN);
      return 1;
    } else if (is_bot(u2)) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, NOTES_NOTTO_BOT);
      return 1;
    }
    for (i = 0; i < dcc_total; i++) {
      if ((!egg_strcasecmp(dcc[i].nick, to)) &&
	  (dcc[i].type->flags & DCT_GETNOTES)) {
	int aok = 1;

	if (dcc[i].type->flags & DCT_CHAT)
	  if (dcc[i].u.chat->away != NULL)
	    aok = 0;
	if (!(dcc[i].type->flags & DCT_CHAT))
	  aok = 0;		/* Assume non dcc-chat == something weird, so
				 * store notes for later */
	if (aok) {
	  dprintf(i, "\007%s [%s]: %s\n", u->handle, NOTES_OUTSIDE, par);
	  dprintf(DP_HELP, "NOTICE %s :%s\n", nick, NOTES_DELIVERED);
	  return 1;
	}
      }
    }
    if (notefile[0] == 0) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, NOTES_UNSUPPORTED);
      return 1;
    }
    f = fopen(notefile, "a");
    if (f == NULL)
      f = fopen(notefile, "w");
    if (f == NULL) {
      dprintf(DP_HELP, "NOTICE %s :%s", nick, NOTES_NOTEFILE_FAILED);
      putlog(LOG_MISC, "*", "* %s", NOTES_NOTEFILE_UNREACHABLE);
      return 1;
    }
    chmod(notefile, userfile_perm);	/* Use userfile permissions. */
    fprintf(f, "%s %s %lu %s\n", to, u->handle, now, par);
    fclose(f);
    dprintf(DP_HELP, "NOTICE %s :%s\n", nick, NOTES_DELIVERED);
    return 1;
  } else
    dprintf(DP_HELP, "NOTICE %s :%s INDEX, READ, ERASE, TO\n",
	    nick, NOTES_DCC_USAGE_READ);
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! NOTES %s %s", nick, host, u->handle, fcn,
	 par[0] ? "..." : "");
  return 1;
}

static void notes_hourly()
{
  expire_notes();
  if (notify_users) {
    register struct chanset_t	*chan;
    register memberlist		*m;
    int				 k;
    register int		 l;
    char			 s1[256];
    struct userrec		*u;

    for (chan = chanset; chan; chan = chan->next) {
      for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
	sprintf(s1, "%s!%s", m->nick, m->userhost);
	u = get_user_by_host(s1);
	if (u) {
	  k = num_notes(u->handle);
	  for (l = 0; l < dcc_total; l++)
	    if ((dcc[l].type->flags & DCT_CHAT) &&
		!egg_strcasecmp(dcc[l].nick, u->handle)) {
	      k = 0;		/* They already know they have notes */
	      break;
	    }
	  if (k) {
	    dprintf(DP_HELP, "NOTICE %s :You have %d note%s waiting on %s.\n",
		    m->nick, k, k == 1 ? "" : "s", botname);
	    dprintf(DP_HELP, "NOTICE %s :%s /MSG %s NOTES <pass> INDEX\n",
		        m->nick, NOTES_FORLIST, botname);
	  }
	}
      }
    }
    for (l = 0; l < dcc_total; l++) {
      k = num_notes(dcc[l].nick);
      if ((k > 0) && (dcc[l].type->flags & DCT_CHAT)) {
	dprintf(l, NOTES_WAITING2, k, k == 1 ? "" : "s");
	dprintf(l, NOTES_DCC_USAGE_READ2);
      }
    }
  }
}

static void away_notes(char *bot, int idx, char *msg)
{
  if (egg_strcasecmp(bot, botnetnick))
    return;
  if (msg && msg[0])
    dprintf(idx, "%s\n", NOTES_STORED);
  else
    notes_read(dcc[idx].nick, 0, "+", idx);
}

static int chon_notes(char *nick, int idx)
{
  if (dcc[idx].type == &DCC_CHAT)
    notes_read(nick, 0, "+", idx);
  return 0;
}

static cmd_t notes_nkch[] =
{
  {"*",		"",	(Function) notes_change,	"notes"},
  {NULL,	NULL,	NULL,				NULL}
};

static cmd_t notes_away[] =
{
  {"*",		"",	(Function) away_notes,		"notes"},
  {NULL,	NULL,	NULL,				NULL}
};

static cmd_t notes_chon[] =
{
  {"*",		"",	(Function) chon_notes,		"notes"},
  {NULL,	NULL,	NULL,				NULL}
};

static cmd_t notes_msgs[] =
{
  {"notes",	"",	(Function) msg_notes,		NULL},
  {NULL,	NULL,	NULL,				NULL}
};

static int notes_server_setup(char *mod)
{
  add_builtins("msg", notes_msgs);
  return 0;
}

static cmd_t notes_load[] =
{
  {"server",	"",	notes_server_setup,		"notes:server"},
  {NULL,	NULL,	NULL,				NULL}
};

static void notes_report(int idx, int details)
{
  if (details) {
    if (notefile[0])
      dprintf(idx, "    Notes can be stored, in: %s\n", notefile);
    else
      dprintf(idx, "    Notes can not be stored.\n");
  }
}

EXPORT_SCOPE char *notes_start();

static Function notes_table[] =
{
  (Function) notes_start,
  (Function) NULL,
  (Function) 0,
  (Function) notes_report,
  (Function) cmd_note,
  (Function) num_notes,
};

char *notes_start(Function * global_funcs)
{

  global = global_funcs;

  notefile[0] = 0;
  module_register(MODULE_NAME, notes_table, 2, 1);
  add_hook(HOOK_HOURLY, (Function) notes_hourly);
  add_hook(HOOK_STORENOTE, (Function) storenote);

  add_builtins("dcc", notes_cmds);
  add_builtins("load", notes_load);
  add_builtins("away", notes_away);
  add_builtins("chon", notes_chon);
  add_builtins("nkch", notes_nkch);

  notes_server_setup(0);
  my_memcpy(&USERENTRY_FWD, &USERENTRY_INFO, sizeof(void *) * 12);
  add_entry_type(&USERENTRY_FWD);
  return NULL;
}
