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
#undef global
#include "notes.h"

static int maxnotes = 50;	/* Maximum number of notes to allow stored
				 * for each user */
static int note_life = 60;	/* Number of DAYS a note lives */
static char notefile[121] = ".n";	/* Name of the notefile */
static int allow_fwd = 1;	/* Allow note forwarding */
static int notify_users = 1;	/* Notify users they have notes every hour? */
static int notify_onjoin = 1;   /* Notify users they have notes on join?
				   drummer */
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
    if (i && s[i - 1] == '\n')
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
	  egg_strftime(wt, 14, "%b %d %H:%M", localtime(&tt));
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

static void away_notes(char *bot, int sock, char *msg)
{
  int idx = findanyidx(sock);

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

static void join_notes(char *nick, char *uhost, char *handle, char *par)
{
  int i = -1, j;
  struct chanset_t *chan = chanset;

  if (notify_onjoin) { /* drummer */
    for (j = 0; j < dcc_total; j++)
      if ((dcc[j].type->flags & DCT_CHAT)
	  && (!egg_strcasecmp(dcc[j].nick, handle))) {
	return;			/* They already know they have notes */
      }

    while (!chan) {
      if (ismember(chan, nick))
        return;			/* They already know they have notes */
      chan = chan->next;
    }

    i = num_notes(handle);
    if (i) {
      dprintf(DP_HELP, NOTES_WAITING_ON, nick, i, i == 1 ? "" : "s",
	      botname);
      dprintf(DP_HELP, "NOTICE %s :%s /MSG %s NOTES <pass> INDEX\n",
	      nick, NOTES_FORLIST, botname);
    }
  }
}

/* Return either NULL or a pointer to the xtra_key structure
 * where the not ignores are kept.
 */
static struct xtra_key *getnotesentry(struct userrec *u)
{
  struct user_entry *ue = find_user_entry(&USERENTRY_XTRA, u);
  struct xtra_key *xk, *nxk = NULL;

  if (!ue)
    return NULL;
  /* Search for the notes ignore list entry */
  for (xk = ue->u.extra; xk; xk = xk->next)
    if (xk->key && !egg_strcasecmp(xk->key, NOTES_IGNKEY)) {
      nxk = xk;
      break;
    }
  if (!nxk || !nxk->data || !(nxk->data[0]))
    return NULL;
  return nxk;
}

/* Parse the NOTES_IGNKEY xtra field. You must free the memory allocated
 * in here: the buffer 'ignores[0]' and the array 'ignores'.
 */
int get_note_ignores(struct userrec *u, char ***ignores)
{
  struct xtra_key *xk;
  char *buf, *p;
  int ignoresn;

  /* Hullo? sanity? */
  if (!u)
    return 0;
  xk = getnotesentry(u);
  if (!xk)
    return 0;

  rmspace(xk->data);
  buf = user_malloc(strlen(xk->data) + 1);
  strcpy(buf, xk->data);
  p = buf;

  /* Split up the string into small parts */
  *ignores = nmalloc(sizeof(char *) + 100);
  **ignores = p;
  ignoresn = 1;
  while ((p = strchr(p, ' ')) != NULL) {
    *ignores = nrealloc(*ignores, sizeof(char *) * (ignoresn+1));
    (*ignores)[ignoresn] = p + 1;
    ignoresn++;
    *p = 0;
    p++;
  }
  return ignoresn;
}

int add_note_ignore(struct userrec *u, char *mask)
{
  struct xtra_key *xk;
  char **ignores;
  int ignoresn, i;

  ignoresn = get_note_ignores(u, &ignores);
  if (ignoresn > 0) {
    /* Search for existing mask */
    for (i = 0; i < ignoresn; i++)
      if (!strcmp(ignores[i], mask)) {
        nfree(ignores[0]);	/* Free the string buffer	*/
        nfree(ignores);		/* Free the ptr array		*/
	/* The mask already exists, exit. */
        return 0;
      }
    nfree(ignores[0]);		/* Free the string buffer	*/
    nfree(ignores);		/* Free the ptr array		*/
  }

  xk = getnotesentry(u);
  /* First entry? */
  if (!xk) {
    struct xtra_key *mxk = user_malloc(sizeof(struct xtra_key));
    struct user_entry *ue = find_user_entry(&USERENTRY_XTRA, u);

    if (!ue)
      return 0;
    mxk->next = 0;
    mxk->data = user_malloc(strlen(mask) + 1);
    strcpy(mxk->data, mask);
    mxk->key = user_malloc(strlen(NOTES_IGNKEY) + 1);
    strcpy(mxk->key, NOTES_IGNKEY);
    xtra_set(u, ue, mxk);
  } else { /* ... else, we already have other entries. */
    xk->data = user_realloc(xk->data, strlen(xk->data) + strlen(mask) + 2);
    strcat(xk->data, " ");
    strcat(xk->data, mask);
  }
  return 1;
}

int del_note_ignore(struct userrec *u, char *mask)
{
  struct user_entry *ue;
  struct xtra_key *xk;
  char **ignores, *buf = NULL;
  int ignoresn, i, size = 0, foundit = 0;

  ignoresn = get_note_ignores(u, &ignores);
  if (!ignoresn)
    return 0;

  buf = user_malloc(1);
  buf[0] = 0;
  for (i = 0; i < ignoresn; i++) {
    if (strcmp(ignores[i], mask)) {
      size += strlen(ignores[i]);
      if (buf[0])
	size++;
      buf = user_realloc(buf, size+1);
      if (buf[0])
	strcat(buf, " ");
      strcat(buf, ignores[i]);
    } else
      foundit = 1;
  }
  nfree(ignores[0]);		/* Free the string buffer	*/
  nfree(ignores);		/* Free the ptr array		*/
  /* Entry not found */
  if (!foundit) {
    nfree(buf);
    return 0;
  }
  ue = find_user_entry(&USERENTRY_XTRA, u);
  /* Delete the entry if the buffer is empty */

  xk = user_malloc(sizeof(struct xtra_key));
  xk->key = user_malloc(strlen(NOTES_IGNKEY)+1);
  xk->next = 0;

  if (!buf[0]) {
    nfree(buf); /* The allocated byte needs to be free'd too */
    strcpy(xk->key, NOTES_IGNKEY);
    xk->data = 0;
  } else {
    xk->data = buf;
    strcpy(xk->key, NOTES_IGNKEY);
  }
  xtra_set(u, ue, xk);
  return 1;
}

/* Returns 1 if the user u has an note ignore which
 * matches from
 */
int match_note_ignore(struct userrec *u, char *from)
{
  char **ignores;
  int ignoresn, i;

  ignoresn = get_note_ignores(u, &ignores);
  if (!ignoresn)
    return 0;
  for (i = 0; i < ignoresn; i++)
    if (wild_match(ignores[i], from)) {
      nfree(ignores[0]);
      nfree(ignores);
      return 1;
    }
  nfree(ignores[0]);		/* Free the string buffer	*/
  nfree(ignores);		/* Free the ptr array		*/
  return 0;
}


static cmd_t notes_join[] =
{
  {"*",		"",	(Function) join_notes,		"notes"},
  {NULL,	NULL,	NULL,				NULL}
};

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

static tcl_ints notes_ints[] =
{
  {"note-life",		&note_life},
  {"max-notes",		&maxnotes},
  {"allow-fwd",		&allow_fwd},
  {"notify-users",	&notify_users},
  {"notify-onjoin",	&notify_onjoin},
  {NULL,		NULL}
};

static tcl_strings notes_strings[] =
{
  {"notefile",		notefile,		120,	0},
  {NULL,		NULL,			0,	0}
};

static int notes_irc_setup(char *mod)
{
  p_tcl_bind_list H_temp;

  if ((H_temp = find_bind_table("join")))
    add_builtins(H_temp, notes_join);
  return 0;
}

static int notes_server_setup(char *mod)
{
  p_tcl_bind_list H_temp;

  if ((H_temp = find_bind_table("msg")))
    add_builtins(H_temp, notes_msgs);
  return 0;
}

static cmd_t notes_load[] =
{
  {"server",	"",	notes_server_setup,		"notes:server"},
  {"irc",	"",	notes_irc_setup,		"notes:irc"},
  {NULL,	NULL,	NULL,				NULL}
};

static int notes_expmem()
{
  return 0;
}

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
  (Function) notes_expmem,
  (Function) notes_report,
  (Function) cmd_note,
};

char *notes_start(Function * global_funcs)
{

  global = global_funcs;

  notefile[0] = 0;
  module_register(MODULE_NAME, notes_table, 2, 1);
  add_hook(HOOK_HOURLY, (Function) notes_hourly);
  add_hook(HOOK_MATCH_NOTEREJ, (Function) match_note_ignore);
  add_tcl_ints(notes_ints);
  add_tcl_strings(notes_strings);
  add_builtins_dcc(H_dcc, notes_cmds);
  add_builtins(H_chon, notes_chon);
  add_builtins(H_away, notes_away);
  add_builtins(H_nkch, notes_nkch);
  add_builtins(H_load, notes_load);
  notes_server_setup(0);
  notes_irc_setup(0);
  my_memcpy(&USERENTRY_FWD, &USERENTRY_INFO, sizeof(void *) * 12);
  add_entry_type(&USERENTRY_FWD);
  return NULL;
}
