
/* 
 * userrec.c -- handles:
 *   add_q() del_q() str2flags() flags2str() str2chflags() chflags2str()
 *   a bunch of functions to find and change user records
 *   change and check user (and channel-specific) flags
 * 
 * dprintf'ized, 10nov1995
 * 
 * $Id: userrec.c,v 1.23 2000/01/17 16:14:45 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#include "main.h"
#include <sys/stat.h>
#include "users.h"
#include "chan.h"
#include "hook.h"
#include "tandem.h"

extern struct dcc_t *dcc;
extern struct chanset_t *chanset;

#ifdef G_DCCPASS
extern struct cmd_pass *cmdpass;
#endif

extern int default_flags,
  default_uflags,
  quiet_save,
  dcc_total,
  share_greet;
extern char 
  ver[],
  botnetnick[],
  localkey[];
extern time_t now;
extern struct logcategory *logcat;

int noshare = 1;		/* don't send out to sharebots */
struct userrec *userlist = NULL;	/* user records are stored here */
struct userrec *lastuser = NULL;	/* last accessed user record */
struct maskrec *global_bans = NULL,
 *global_exempts = NULL,
 *global_invites = NULL;
struct igrec *global_ign = NULL;
int cache_hit = 0,
  cache_miss = 0;		/* temporary cache accounting */
int strict_host = 1;
char BUF[512];			/* lame buffer for get_user_by_host() */

#ifdef DEBUG_MEM
void *_user_malloc(int size, char *file, int line)
{
  char x[1024];

  simple_sprintf(x, STR("userrec.c:%s"), file);
  return n_malloc(size, x, line);
}

void *_user_realloc(void *ptr, int size, char *file, int line)
{
  char x[1024];

  simple_sprintf(x, STR("userrec.c:%s"), file);
  return n_realloc(ptr, size, x, line);
}
#else
void *_user_malloc(int size, char *file, int line)
{
  return nmalloc(size);
}

void *_user_realloc(void *ptr, int size, char *file, int line)
{
  return nrealloc(ptr, size);
}
#endif

inline int expmem_mask(struct maskrec *m)
{
  int result = 0;

  while (m) {
    result += sizeof(struct maskrec);

    result += strlen(m->mask) + 1;
    if (m->user)
      result += strlen(m->user) + 1;
    if (m->desc)
      result += strlen(m->desc) + 1;

    m = m->next;
  }

  return result;
}

/* memory we should be using */
int expmem_users()
{
  int tot;
  struct userrec *u;
  struct chanuserrec *ch;
  struct chanset_t *chan;
  struct user_entry *ue;
  struct igrec *i;

  Context;
  tot = 0;
  u = userlist;
  while (u != NULL) {
    ch = u->chanrec;
    while (ch) {
      tot += sizeof(struct chanuserrec);

      if (ch->info != NULL)
	tot += strlen(ch->info) + 1;
      ch = ch->next;
    }
    tot += sizeof(struct userrec);

    for (ue = u->entries; ue; ue = ue->next) {
      tot += sizeof(struct user_entry);

      if (ue->name) {
	tot += strlen(ue->name) + 1;
	tot += list_type_expmem(ue->u.list);
      } else {
	tot += ue->type->expmem(ue);
      }
    }
    u = u->next;
  }
  /* account for each channel's masks */
  for (chan = chanset; chan; chan = chan->next) {

    /* account for each channel's ban-list user */
    tot += expmem_mask(chan->bans);

    /* account for each channel's exempt-list user */
    tot += expmem_mask(chan->exempts);

    /* account for each channel's invite-list user */
    tot += expmem_mask(chan->invites);
  }

  tot += expmem_mask(global_bans);
  tot += expmem_mask(global_exempts);
  tot += expmem_mask(global_invites);

  for (i = global_ign; i; i = i->next) {
    tot += sizeof(struct igrec);

    tot += strlen(i->igmask) + 1;
    if (i->user)
      tot += strlen(i->user) + 1;
    if (i->msg)
      tot += strlen(i->msg) + 1;
  }
  return tot;
}

int count_users(struct userrec *bu)
{
  int tot = 0;
  struct userrec *u = bu;

  while (u != NULL) {
    tot++;
    u = u->next;
  }
  return tot;
}

/* make nick!~user@host into nick!user@host if necessary */

/* also the new form: nick!+user@host or nick!-user@host */

/* new: returns a statically allocated buffer with the result (drummer) */
char *fixfrom(char *s)
{
  char *p;

  if (s == NULL)
    return NULL;
  strncpy0(BUF, s, sizeof(BUF));
  if (strict_host)
    return BUF;
  if ((p = strchr(BUF, '!')))
    p++;
  else
    p = s;			/* sometimes we get passed just a
				 * user@host here... */
  /* these are ludicrous. */
  if (strchr("~+-^=", *p) && (p[1] != '@'))	/* added check for @ - drummer */
    strcpy(p, p + 1);
  /* bug was: n!~@host -> n!@host  now: n!~@host */
  return BUF;
}

struct userrec *check_dcclist_hand(char *handle)
{
  int i;

  for (i = 0; i < dcc_total; i++)
    if (!strcasecmp(dcc[i].nick, handle))
      return dcc[i].user;
  return NULL;
}

struct userrec *get_user_by_handle(struct userrec *bu, char *handle)
{
  struct userrec *u = bu,
   *ret;

  if (!handle || !bu)
    return NULL;
  rmspace(handle);
  if (!handle[0] || (handle[0] == '*'))
    return NULL;
  if (strlen(handle)>HANDLEN)
    return NULL;
  if (bu == userlist) {
    if (lastuser && !strcasecmp(lastuser->handle, handle)) {
      cache_hit++;
      return lastuser;
    }
    ret = check_dcclist_hand(handle);
    if (ret) {
      cache_hit++;
      return ret;
    }
    ret = check_chanlist_hand(handle);
    if (ret) {
      cache_hit++;
      return ret;
    }
    cache_miss++;
  }
  while (u) {
    if (!strcasecmp(u->handle, handle)) {
      if (bu == userlist)
	lastuser = u;
      return u;
    }
    u = u->next;
  }
  return NULL;
}

/* fix capitalization, etc */
void correct_handle(char *handle)
{
  struct userrec *u;

  u = get_user_by_handle(userlist, handle);
  if (u == NULL)
    return;
  strcpy(handle, u->handle);
}

/*        This will be usefull in a lot of places, much more code re-use so we
 *      endup with a smaller executable bot. <cybah> 
 */
void clear_masks(struct maskrec *m)
{
  struct maskrec *temp = NULL;

  while (m) {
    temp = m->next;

    if (m->mask)
      nfree(m->mask);
    if (m->user)
      nfree(m->user);
    if (m->desc)
      nfree(m->desc);

    nfree(m);
    m = temp;
  }
}

void clear_userlist(struct userrec *bu)
{
  struct userrec *u = bu,
   *v;
  int i;

  Context;
  while (u != NULL) {
    v = u->next;
    freeuser(u);
    u = v;
  }
  if (userlist == bu) {
    struct chanset_t *cst;

    for (i = 0; i < dcc_total; i++)
      dcc[i].user = NULL;
    clear_chanlist();
    lastuser = NULL;

    while (global_ign)
      delignore(global_ign->igmask);

    clear_masks(global_bans);
    clear_masks(global_exempts);
    clear_masks(global_invites);
    global_exempts = global_invites = global_bans = NULL;

    for (cst = chanset; cst; cst = cst->next) {
      clear_masks(cst->bans);
      clear_masks(cst->exempts);
      clear_masks(cst->invites);

      cst->bans = cst->exempts = cst->invites = NULL;
    }
  }
  /* remember to set your userlist to NULL after calling this */
  Context;
}

/* find CLOSEST host match */

/* (if "*!*@*" and "*!*@*clemson.edu" both match, use the latter!) */

/* 26feb: CHECK THE CHANLIST FIRST to possibly avoid needless search */
struct userrec *get_user_by_host(char *host)
{
  struct userrec *u = userlist,
   *ret;
  struct list_type *q;
  int cnt,
    i;

  if (host == NULL)
    return NULL;
  rmspace(host);
  if (!host[0])
    return NULL;
  ret = check_chanlist(host);
  cnt = 0;
  if (ret != NULL) {
    cache_hit++;
    return ret;
  }
  cache_miss++;
  host = fixfrom(host);
  while (u != NULL) {
    q = get_user(&USERENTRY_HOSTS, u);
    while (q != NULL) {
      i = wild_match(q->extra, host);
      if (i > cnt) {
	ret = u;
	cnt = i;
      }
      q = q->next;
    }
    u = u->next;
  }
  if (ret != NULL) {
    lastuser = ret;
    set_chanlist(host, ret);
  }
  return ret;
}

/* use fixfrom() or dont? (drummer) */
struct userrec *get_user_by_equal_host(char *host)
{
  struct userrec *u = userlist;
  struct list_type *q;

  while (u != NULL) {
    q = get_user(&USERENTRY_HOSTS, u);
    while (q != NULL) {
      if (!rfc_casecmp(q->extra, host))
	return u;
      q = q->next;
    }
    u = u->next;
  }
  return NULL;
}

/* try: pass_match_by_host("-",host)
 * will return 1 if no password is set for that host */
int u_pass_match(struct userrec *u, char *pass)
{
  char *cmp,
    new[32];

  if (!u)
    return 0;
  cmp = get_user(&USERENTRY_PASS, u);
  if (!cmp)
    return 1;
  if (!pass || !pass[0] || (pass[0] == '-'))
    return 0;
  if (u->flags & USER_BOT) {
    if (!strcmp(cmp, pass))
      return 1;
  } else {
    if (strlen(pass) > 15)
      pass[15] = 0;
    encrypt_pass(pass, new);
    if (!strcmp(cmp, new))
      return 1;
  }
  return 0;
}

int write_user(struct userrec *u, char *key, stream str, int idx)
{
  char s[181];
  struct chanuserrec *ch;
  struct chanset_t *cst;
  struct user_entry *ue;
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };
  char * p;
  fr.global = u->flags;

  fr.udef_global = u->flags_udef;
  build_flags(s, &fr, NULL);
  if ((p=strchr(s, ' ')))
    *p=0;
  enc_stream_printf(str, key, STR("%-10s - %-24s\n"), u->handle, s);
  for (ch = u->chanrec; ch; ch = ch->next) {
    cst = findchan(ch->channel);
    if (cst && ((idx < 0))) {
      if (idx >= 0) {
	fr.match = (FR_CHAN);
	get_user_flagrec(dcc[idx].user, &fr, ch->channel);
      } else
	fr.match = FR_CHAN;
      fr.chan = ch->flags;
      fr.udef_chan = ch->flags_udef;
      build_flags(s, &fr, NULL);
      enc_stream_printf(str, key, STR("! %-20s %lu %-10s %s\n"), ch->channel, ch->laston, s, (((idx < 0) || share_greet) && ch->info) ? ch->info : "");
    }
  }
  for (ue = u->entries; ue; ue = ue->next) {
    if (ue->name) {
      struct list_type *lt;

      for (lt = ue->u.list; lt; lt = lt->next)
	enc_stream_printf(str, key, STR("--%s %s\n"), ue->name, lt->extra);
    } else {
      if (!ue->type->write_userfile(str, key, u, ue))
	return 0;
    }
  }
  return 1;
}

#ifdef HUB

void stream_writeuserfile(stream s, struct userrec * bu, char * key, int idx) {
  struct userrec *u;

  enc_stream_printf(s, key, "#4v:\n");
  u=bu;
  while (u) {
    write_user(u, key, s, idx);
    u = u->next;
  }
  write_bans(s, key, idx);
  write_exempts(s, key, idx);
  write_invites(s, key, idx);
  write_config(s, key, idx);
}


/* rewrite the entire user file */
void write_userfile(int idx)
{
  stream s;
  FILE *f;

  Context;
  /* also write the channel file at the same time */
  if (userlist == NULL)
    return;			/* no point in saving userfile */

  f = fopen(STR(".cn"), "w");
  chmod(STR(".cn"), 0600);		/* make it -rw------- */
  if (f == NULL) {
    log(LCAT_ERROR, STR("Error writing user file"));
    return;
  }
  log(LCAT_INFO, STR("Saving users & channels"));
  s=stream_create();
  stream_writeuserfile(s, userlist, localkey, idx);
  if ((fwrite(stream_buffer(s), 1, stream_size(s), f) != stream_size(s)) || (fflush(f))) {
    stream_kill(s);
    fclose(f);
    log(LCAT_ERROR, STR("Error writing user file (%s)"), strerror(ferror(f)));
  }
  stream_kill(s);
  fclose(f);
  write_channels(localkey);
  check_tcl_event(STR("save"));
  unlink(".c");
  movefile(STR(".cn"), ".c");
}
#endif

int change_handle(struct userrec *u, char *newh)
{
  int i;
  char s[16];

  if (!u)
    return 0;
  /* nothing that will confuse the userfile */
  if ((newh[1] == 0) && strchr(BADHANDCHARS, newh[0]))
    return 0;
  check_tcl_nkch(u->handle, newh);
  /* yes, even send bot nick changes now: */
  if ((!noshare) && !(u->flags & USER_UNSHARED))
    shareout(NULL, STR("h %s %s\n"), u->handle, newh);
  strcpy(s, u->handle);
  strcpy(u->handle, newh);
  for (i = 0; i < dcc_total; i++) {
    if (!strcasecmp(dcc[i].nick, s) && (dcc[i].type != &DCC_BOT)) {
      strcpy(dcc[i].nick, newh);
      if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel >= 0)) {
	chanout_but(-1, dcc[i].u.chat->channel, STR("*** Nick change: %s -> %s\n"), s, newh);
	if (dcc[i].u.chat->channel < 100000)
	  botnet_send_nkch(i, s);
      }
    }
  }
  return 1;
}

extern int noxtra;

struct userrec *adduser(struct userrec *bu, char *handle, char *host, char *pass, int flags)
{
  struct userrec *u,
   *x;
  struct xtra_key *xk;
  int oldshare = noshare;

  noshare = 1;
  u = (struct userrec *) nmalloc(sizeof(struct userrec));

  /* u->next=bu; bu=u; */
  strncpy0(u->handle, handle, HANDLEN+1);
  u->next = NULL;
  u->chanrec = NULL;
  u->entries = NULL;
  if (flags != USER_DEFAULT) {	/* drummer */
    u->flags = flags;
    u->flags_udef = 0;
  } else {
    u->flags = default_flags;
    u->flags_udef = default_uflags;
  }
  set_user(&USERENTRY_PASS, u, pass);
  if (!noxtra) {
    xk = nmalloc(sizeof(struct xtra_key));

    xk->key = nmalloc(8);
    strcpy(xk->key, STR("created"));
    xk->data = nmalloc(10);
    sprintf(xk->data, STR("%09lu"), now);
    set_user(&USERENTRY_XTRA, u, xk);
  }
  /* strip out commas -- they're illegal */
  /* about this fixfrom():
   * we should use this fixfrom before every call of adduser()
   * but its much easier to use here...
   * (drummer)
   * only use it if we have a host :) (dw) 
   */
  if (host && host[0]) {
    char *p;

    host = fixfrom(host);
    p = strchr(host, ',');

    while (p != NULL) {
      *p = '?';
      p = strchr(host, ',');
    }
    set_user(&USERENTRY_HOSTS, u, host);
  } else
    set_user(&USERENTRY_HOSTS, u, STR("none"));
  if (bu == userlist)
    clear_chanlist();
  noshare = oldshare;
  if ((!noshare) && (handle[0] != '*') && (!(flags & USER_UNSHARED)) && (bu == userlist)) {
    struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };
    char x[100];

    fr.global = u->flags;

    fr.udef_global = u->flags_udef;
    build_flags(x, &fr, 0);
    shareout(NULL, STR("n %s %s %s %s\n"), handle, host ? host : STR("none"), pass, x);
  }
  if (bu == NULL)
    bu = u;
  else {
    if ((bu == userlist) && (lastuser != NULL))
      x = lastuser;
    else
      x = bu;
    while (x->next != NULL)
      x = x->next;
    x->next = u;
    if (bu == userlist)
      lastuser = u;
  }
  return bu;
}

void freeuser(struct userrec *u)
{
  struct user_entry *ue,
   *ut;
  struct chanuserrec *ch,
   *z;

  if (u == NULL)
    return;
  ch = u->chanrec;
  while (ch) {
    z = ch;
    ch = ch->next;
    if (z->info != NULL)
      nfree(z->info);
    nfree(z);
  }
  u->chanrec = NULL;
  for (ue = u->entries; ue; ue = ut) {
    ut = ue->next;
    if (ue->name) {
      struct list_type *lt,
       *ltt;

      for (lt = ue->u.list; lt; lt = ltt) {
	ltt = lt->next;
	nfree(lt->extra);
	nfree(lt);
      }
      nfree(ue->name);
      nfree(ue);
    } else {
      ue->type->kill(ue);
    }
  }
  nfree(u);
}

int deluser(char *handle)
{
  struct userrec *u = userlist,
   *prev = NULL;
  int fnd = 0;

  while ((u != NULL) && (!fnd)) {
    if (!strcasecmp(u->handle, handle))
      fnd = 1;
    else {
      prev = u;
      u = u->next;
    }
  }
  if (!fnd)
    return 0;
  if (prev == NULL)
    userlist = u->next;
  else
    prev->next = u->next;
  if (!noshare && (handle[0] != '*') && !(u->flags & USER_UNSHARED))
    shareout(NULL, STR("k %s\n"), handle);
  for (fnd = 0; fnd < dcc_total; fnd++)
    if (dcc[fnd].user == u) {
      if (dcc[fnd].user->flags & USER_BOT) 
	botunlink(-2, dcc[fnd].nick, STR("-user"));
      else
	do_boot(fnd, botnetnick, STR("-user"));
      dcc[fnd].user = 0;	/* clear any dcc users for this entry,
				 * null is safe-ish */
    }
  clear_chanlist();
  freeuser(u);
  lastuser = NULL;
  return 1;
}

int delhost_by_handle(char *handle, char *host)
{
  struct userrec *u;
  struct list_type *q,
   *qnext,
   *qprev;
  struct user_entry *e = NULL;
  int i = 0;

  Context;
  u = get_user_by_handle(userlist, handle);
  if (!u)
    return 0;
  q = get_user(&USERENTRY_HOSTS, u);
  qprev = q;
  if (q) {
    if (!rfc_casecmp(q->extra, host)) {
      e = find_user_entry(&USERENTRY_HOSTS, u);
      e->u.extra = q->next;
      nfree(q->extra);
      nfree(q);
      i++;
      qprev = NULL;
      q = e->u.extra;
    } else
      q = q->next;
    while (q) {
      qnext = q->next;
      if (!rfc_casecmp(q->extra, host)) {
	if (qprev)
	  qprev->next = q->next;
	else if (e) {
	  e->u.extra = q->next;
	  qprev = NULL;
	}
	nfree(q->extra);
	nfree(q);
	i++;
      } else
	qprev = q;
      q = qnext;
    }
  }
  if (!qprev)
    set_user(&USERENTRY_HOSTS, u, STR("none"));
  if (!noshare && i && !(u->flags & USER_UNSHARED))
    shareout(NULL, STR("-h %s %s\n"), handle, host);
  clear_chanlist();
  return i;
}

void addhost_by_handle(char *handle, char *host)
{
  struct userrec *u = get_user_by_handle(userlist, handle);

  set_user(&USERENTRY_HOSTS, u, host);
  /* u will be cached, so really no overhead, even tho this looks dumb: */
  if ((!noshare) && !(u->flags & USER_UNSHARED)) {
    if (u->flags & USER_BOT)
      shareout(NULL, STR("+bh %s %s\n"), handle, host);
    else
      shareout(NULL, STR("+h %s %s\n"), handle, host);
  }
  clear_chanlist();
}

void touch_laston(struct userrec *u, char *where, time_t timeval)
{
  if (!u)
    return;
  if (timeval > 1) {
    struct laston_info *li = (struct laston_info *) get_user(&USERENTRY_LASTON, u);

    if (!li)
      li = nmalloc(sizeof(struct laston_info));

    else if (li->lastonplace)
      nfree(li->lastonplace);
    li->laston = timeval;
    if (where) {
      li->lastonplace = nmalloc(strlen(where) + 1);
      strcpy(li->lastonplace, where);
    } else
      li->lastonplace = NULL;
    set_user(&USERENTRY_LASTON, u, li);
  } else if (timeval == 1) {
    set_user(&USERENTRY_LASTON, u, 0);
  }
}

/*  Go through all channel records and try to find a matching
 *  nick. Will return the user's user record if that is known
 *  to the bot.  (Fabian)
 *  
 *  Warning: This is unreliable by concept!
 */
struct userrec *get_user_by_nick(char *nick)
{
  struct chanset_t *chan = chanset;
  memberlist *m;

  Context;
  while (chan) {
    m = chan->channel.member;
    while (m && m->nick[0]) {
      if (!rfc_casecmp(nick, m->nick)) {
	char word[512];

	sprintf(word, STR("%s!%s"), m->nick, m->userhost);
	/* no need to check the return value ourself */
	return get_user_by_host(word);;
      }
      m = m->next;
    }
    chan = chan->next;
  }
  /* sorry, no matches */
  return NULL;
}

void user_del_chan(char *name)
{
  struct chanuserrec *ch,
   *och;
  struct userrec *u;

  for (u = userlist; u; u = u->next) {
    ch = u->chanrec;
    och = NULL;
    while (ch) {
      if (!rfc_casecmp(name, ch->channel)) {
	if (och)
	  och->next = ch->next;
	else
	  u->chanrec = ch->next;

	if (ch->info)
	  nfree(ch->info);
	nfree(ch);
	break;
      }
      och = ch;
      ch = ch->next;
    }
  }
}


struct cfg_entry
  CFG_BADCOOKIE,
#ifdef G_MANUALOP
  CFG_MANUALOP,
#endif
#ifdef G_MEAN
  CFG_MEANDEOP,
  CFG_MEANKICK,
  CFG_MEANBAN,
#endif
  CFG_MDOP;

int deflag_dontshare=0;
char deflag_tmp[20];

#define DEFL_IGNORE 0
#define DEFL_DEOP 1
#define DEFL_KICK 2
#define DEFL_DELETE 3


void deflag_describe(struct cfg_entry *cfgent, int idx)
{
  if (cfgent == &CFG_BADCOOKIE)
    dprintf(idx, STR("bad-cookie decides what happens to a bot if it does an illegal op (no/incorrect op cookie)\n"));
#ifdef G_MANUALOP
  else if (cfgent==&CFG_MANUALOP)
    dprintf(idx, STR("manualop decides what happens to a user doing a manual op in a -manualop channel\n"));
#endif
#ifdef G_MEAN
  else if (cfgent==&CFG_MEANDEOP)
    dprintf(idx, STR("mean-deop decides what happens to a user deopping a bot in a +mean channel\n"));
  else if (cfgent==&CFG_MEANKICK)
    dprintf(idx, STR("mean-kick decides what happens to a user kicking a bot in a +mean channel\n"));
  else if (cfgent==&CFG_MEANBAN)
    dprintf(idx, STR("mean-ban decides what happens to a user banning a bot in a +mean channel\n"));
#endif
  else if (cfgent==&CFG_MDOP)
    dprintf(idx, STR("mdop decides what happens to a user doing a mass deop\n"));
  dprintf(idx, STR("Valid settings are: ignore (No flag changes), deop (give -fmnop+d), kick (give -fmnop+dk) or delete (remove from userlist)\n"));
}

void deflag_changed(struct cfg_entry * entry, char * oldval, int * valid) {
  char * p = (char *) entry->gdata;
  if (!p)
    return;
  if (strcmp(p, STR("ignore")) && strcmp(p, STR("deop")) && strcmp(p, STR("kick")) && strcmp(p, STR("delete")))
    *valid=0;
}

struct cfg_entry CFG_BADCOOKIE = {
  "bad-cookie", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};

#ifdef G_MANUALOP
struct cfg_entry CFG_MANUALOP = {
  "manualop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};
#endif

#ifdef G_MEAN
struct cfg_entry CFG_MEANDEOP = {
  "mean-deop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};


struct cfg_entry CFG_MEANKICK = {
  "mean-kick", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};

struct cfg_entry CFG_MEANBAN = {
  "mean-ban", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};
#endif

struct cfg_entry CFG_MDOP = {
  "mdop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed,
  NULL,
  deflag_describe
};



void deflag_user(struct userrec *u, int why, char *msg)
{
  char tmp[256], tmp2[1024];
  struct cfg_entry * ent = NULL;
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0, 0};
  if (!u)
    return;
  switch (why) {
  case DEFLAG_BADCOOKIE:
    strcpy(tmp, STR("Bad op cookie"));
    ent=&CFG_BADCOOKIE;
    break;
#ifdef G_MANUALOP
  case DEFLAG_MANUALOP:
    strcpy(tmp, STR("Manual op in -manualop channel"));
    ent=&CFG_MANUALOP;
    break;
#endif
#ifdef G_MEAN
  case DEFLAG_MEAN_DEOP:
    strcpy(tmp, STR("Deopped bot in +mean channel"));
    ent=&CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_KICK:
    strcpy(tmp, STR("Kicked bot in +mean channel"));
    ent=&CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_BAN:
    strcpy(tmp, STR("Banned bot in +mean channel"));
    ent=&CFG_MEANDEOP;
    break;
#endif
  case DEFLAG_MDOP:
    strcpy(tmp, STR("Mass deop"));
    ent=&CFG_MDOP;
    break;
  default:
    ent=NULL;
    sprintf(tmp, STR("Reason #%i"), why);
  }
  if (ent && ent->gdata && !strcmp(ent->gdata, STR("deop"))) {
    log(LCAT_WARNING, STR("Setting %s +d (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("+d: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, NULL);
    fr.global = USER_DEOP;
    set_user_flagrec(u, &fr, NULL);
  } else if (ent && ent->gdata && !strcmp(ent->gdata, STR("kick"))) {
    log(LCAT_WARNING, STR("Setting %s +dk (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("+dk: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, NULL);
    fr.global = USER_DEOP | USER_KICK;
    set_user_flagrec(u, &fr, NULL);
  } else if (ent && ent->gdata && !strcmp(ent->gdata, STR("delete"))) {
    log(LCAT_WARNING, STR("Deleting %s (%s): %s\n"), u->handle, tmp, msg);
    deluser(u->handle);
  } else {
    log(LCAT_WARNING, STR("No user flag effects for %s (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("Warning: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
  }
}

void init_userrec() {
  add_cfg(&CFG_BADCOOKIE);
#ifdef G_MANUALOP
  add_cfg(&CFG_MANUALOP);
#endif
#ifdef G_MEAN
  add_cfg(&CFG_MEANDEOP);
  add_cfg(&CFG_MEANKICK);
  add_cfg(&CFG_MEANBAN);
#endif
  add_cfg(&CFG_MDOP);
}




