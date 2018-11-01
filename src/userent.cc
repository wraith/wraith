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
 * userent.c -- handles:
 *   user-entry handling, new stylem more versatile.
 *
 */


#include "common.h"
#include "users.h"
#include "src/mod/share.mod/share.h"
#include "misc.h"
#include "chanprog.h"
#include "main.h"
#include "debug.h"
#include "userrec.h"
#include "match.h"
#include "dccutil.h"
#include "crypt.h"
#include "botmsg.h"
#include "botnet.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

static struct user_entry_type *entry_type_list = NULL;

void init_userent()
{
  add_entry_type(&USERENTRY_COMMENT);
  add_entry_type(&USERENTRY_INFO);
  add_entry_type(&USERENTRY_LASTON);
  add_entry_type(&USERENTRY_BOTADDR);
  add_entry_type(&USERENTRY_OS);
  add_entry_type(&USERENTRY_NODENAME);
  add_entry_type(&USERENTRY_USERNAME);
  add_entry_type(&USERENTRY_ARCH);
  add_entry_type(&USERENTRY_OSVER);
  add_entry_type(&USERENTRY_PASS);
  add_entry_type(&USERENTRY_PASS1);
  add_entry_type(&USERENTRY_SECPASS);
  add_entry_type(&USERENTRY_HOSTS);
  add_entry_type(&USERENTRY_STATS);
  add_entry_type(&USERENTRY_ADDED);
  add_entry_type(&USERENTRY_MODIFIED);
  add_entry_type(&USERENTRY_SET);
  add_entry_type(&USERENTRY_FFLAGS);
}

void list_type_kill(struct list_type *t)
{
  struct list_type *u = NULL;

  while (t) {
    u = t->next;
    free(t->extra);
    free(t);
    t = u;
  }
}

bool def_unpack(struct userrec *u, struct user_entry *e)
{
  char *tmp = e->u.list->extra;

  e->u.list->extra = NULL;
  list_type_kill(e->u.list);
  e->u.string = tmp;
  return 1;
}

bool def_kill(struct user_entry *e)
{
  free(e->u.string);
  free(e);
  return 1;
}

void write_userfile_protected(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  /* only write if saving local, or if sending to hub, or if sending to same user as entry */
  if (idx == -1 || dcc[idx].hub || dcc[idx].user == u) {
    stream << bd::String::printf("--%s %s\n", e->type->name, e->u.string);
  }
}

void def_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  stream << bd::String::printf("--%s %s\n", e->type->name, e->u.string);
}

void *def_get(struct userrec *u, struct user_entry *e)
{
  return e->u.string;
}

bool def_set_real(struct userrec *u, struct user_entry *e, void *buf, bool protect)
{
  char *string = (char *) buf;

  if (string && !string[0])
    string = NULL;
  if (!string && !e->u.string)
    return 1;
  if (string) {
    size_t l = strlen (string);
    char *i = NULL;

    if (l > 160)
      l = 160;


    e->u.string = (char *) realloc (e->u.string, l + 1);

    strlcpy (e->u.string, string, l + 1);

    for (i = e->u.string; *i; i++)
      /* Allow bold, inverse, underline, color text here...
       * But never add cr or lf!! --rtc
       */
     if ((unsigned int) *i < 32 && !strchr ("\002\003\026\037", *i))
        *i = '?';
  } else { /* string == NULL && e->u.string != NULL */
    free(e->u.string);
    e->u.string = NULL;
  }
  if (!noshare) {
    if (protect)
      shareout_prot(u, "c %s %s %s\n", e->type->name, u->handle, e->u.string ? e->u.string : "");
    else
      shareout("c %s %s %s\n", e->type->name, u->handle, e->u.string ? e->u.string : "");
  }
  return 1;
}

bool def_set(struct userrec *u, struct user_entry *e, void *buf)
{
  return (def_set_real(u, e, buf, 0));
}

bool set_protected(struct userrec *u, struct user_entry *e, void *buf)
{
  return (def_set_real(u, e, buf, 1));
}

bool def_gotshare(struct userrec *u, struct user_entry *e, char *data, int idx)
{
  if (conf.bot->hub)
    putlog(LOG_DEBUG, "@", "%s: change %s %s", dcc[idx].nick, e->type->name, u->handle);
  return e->type->set(u, e, data);
}

void def_display(int idx, struct user_entry *e, struct userrec *u)
{
  dprintf(idx, "  %s: %s\n", e->type->name, e->u.string);
}


static void comment_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (dcc[idx].user && (dcc[idx].user->flags & USER_MASTER))
    dprintf(idx, "  COMMENT: %s\n", e->u.string);
}

struct user_entry_type USERENTRY_COMMENT =
{
  0,				/* always 0 ;) */
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  comment_display,
  "COMMENT"
};

struct user_entry_type USERENTRY_INFO =
{
  0,				/* always 0 ;) */
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  def_display,
  "INFO"
};

static void added_display(int idx, struct user_entry *e, struct userrec *u)
{
  /* format: unixtime handle */
  if (dcc[idx].user && (dcc[idx].user->flags & USER_OWNER)) {
    char tmp[30] = "", tmp2[70] = "", *hnd = NULL;
    time_t tm;

    strlcpy(tmp, e->u.string, sizeof(tmp));
    hnd = strchr(tmp, ' ');
    if (hnd)
      *hnd++ = 0;
    tm = atoi(tmp);

    strftime(tmp2, sizeof(tmp2), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&tm));
    if (hnd)
      dprintf(idx, "  -- Added %s by %s\n", tmp2, hnd);
    else
      dprintf(idx, "  -- Added %s\n", tmp2);
  }
}

struct user_entry_type USERENTRY_ADDED = {
  0,				/* always 0 ;) */
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  added_display,
  "ADDED"
};


static bool set_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct xtra_key *curr = e->u.xk,
                  *newxk = (struct xtra_key *) buf, *old = NULL;

  /* find the curr key if it exists */
  for (; curr; curr = curr->next) {
    if (curr->key && !strcasecmp(curr->key, newxk->key)) {
      old = curr;
      break;
    }
  }

  /* Nothing to do if the old and new entry match. Can this even happen? */
  if (old == newxk)
    return 1;
  
  /* we will possibly free new below, so let's send the information to the botnet now */
  if (!noshare && !set_noshare) {
    /* Always share groups to all bots. */
    if (!strcmp(newxk->key, "groups")) {
      shareout("c %s %s %s %s\n", e->type->name, u->handle, newxk->key, newxk->data ? newxk->data : "");
    } else {
      /* only share to pertinent bots */
      shareout_prot(u, "c %s %s %s %s\n", e->type->name, u->handle, newxk->key, newxk->data ? newxk->data : "");
    }
  }

  /* if we have a new entry and an old entry.. or our new entry is empty -> clear out the old entry */
  if (old) {
    list_delete((struct list_type **) (&e->u.extra), (struct list_type *) old);

    free(old->key);
    free(old->data);
    free(old);
    old = NULL;
  }

  /* add the new entry if it's not empty */
  if (newxk->data && newxk->data[0]) {
    list_insert((&e->u.xk), newxk);
  } else {
    free(newxk->data);
    free(newxk->key);
    free(newxk);
  }
  return 1;
}

static bool set_unpack(struct userrec *u, struct user_entry *e)
{
  struct list_type *curr = NULL, *head = NULL;

  head = curr = e->u.list;
  e->u.extra = NULL;

  struct xtra_key *t = NULL;
  char *key = NULL, *data = NULL;

  while (curr) {
    data = curr->extra;
    key = newsplit(&data);
    if (data[0]) {
      t = (struct xtra_key *) calloc(1, sizeof(struct xtra_key));
      t->key = strdup(key);
      t->data = strdup(data);
      list_insert((&e->u.xk), t);
    }
    curr = curr->next;
  }

  list_type_kill(head);
  return 1;
}

static void set_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (conf.bot->hub) {
    struct xtra_key *xk = e->u.xk;
    struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

    dprintf(idx, "  BOTSET:\n");
    get_user_flagrec(dcc[idx].user, &fr, NULL);
    /* scan thru xtra field, searching for matches */
    for (; xk; xk = xk->next) {
      /* ok, it's a valid xtra field entry */
      if (glob_owner(fr))
        dprintf(idx, "    %s: %s\n", xk->key, xk->data ? xk->data : "");
    }
  }
}

static bool set_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  char *name = newsplit(&buf);

  ASSERT(e == NULL, "set_gotshare should not be passed a user_entry");

  if (!name || !name[0])
    return 1;

  if (!strcasecmp(u->handle, conf.bot->nick)) {
    set_noshare = 1;
    /* This will also call set_user(). */
    var_set_by_name(conf.bot->nick, name, buf[0] ? buf : NULL);
    set_noshare = 0;
  } else if (conf.bot->hub || conf.bot->localhub || !strcmp(name, "groups")) {
    var_set_userentry(u->handle, name, buf);
  }
  return 1;
}

static void set_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  int localhub = nextbot(u->handle);
  struct xtra_key *x = e->u.xk;

  for (; x; x = x->next) {
    /*
     * only write if saving local, or if sending to hub, or if sending to
     * same user as entry, or they are not connected,
     * or the localhub in the chain, or sending 'groups'.
     * SA shareout_prot
     */
    if (idx == -1 || dcc[idx].hub || dcc[idx].user == u ||
        (localhub == -1 || idx == localhub) || !strcmp(x->key, "groups")) {
      stream << bd::String::printf("--%s %s %s\n", e->type->name, x->key, x->data ? x->data : "");
    }
  }
}

static bool set_kill(struct user_entry *e)
{
  struct xtra_key *x = e->u.xk, *y = NULL;

  for (; x; x = y) {
    y = x->next;
    free(x->key);
    free(x->data);
    free(x);
  }
  free(e);
  return 1;
}

struct user_entry_type USERENTRY_SET = {
  0,
  set_gotshare,
  set_unpack,
  set_write_userfile,
  set_kill,
  def_get,
  set_set,
  set_display,
  "SET"
};

static void botmisc_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (conf.bot->hub) {
    struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

    get_user_flagrec(dcc[idx].user, &fr, NULL);
    if (glob_admin(fr))
      dprintf(idx, "  %s: %s\n", e->type->name, e->u.string ? e->u.string : "");
  }
}

struct user_entry_type USERENTRY_USERNAME = {
 0,
 def_gotshare,
 def_unpack,
 write_userfile_protected,
 def_kill,
 def_get,
 set_protected,
 botmisc_display,
 "USERNAME"
};

struct user_entry_type USERENTRY_NODENAME = {
 0,
 def_gotshare,
 def_unpack,
 write_userfile_protected,
 def_kill,
 def_get,
 set_protected,
 botmisc_display,
 "NODENAME"
};

struct user_entry_type USERENTRY_OS = {
 0,
 def_gotshare,
 def_unpack,
 write_userfile_protected,
 def_kill,
 def_get,
 set_protected,
 botmisc_display,
 "OS"
};

struct user_entry_type USERENTRY_OSVER = {
 0,
 def_gotshare,
 def_unpack,
 write_userfile_protected,
 def_kill,
 def_get,
 set_protected,
 botmisc_display,
 "OSVER"
};

struct user_entry_type USERENTRY_ARCH = {
 0,
 def_gotshare,
 def_unpack,
 write_userfile_protected,
 def_kill,
 def_get,
 set_protected,
 botmisc_display,
 "ARCH"
};

bool fflags_unpack(struct userrec *u, struct user_entry *e)
{
  bool ret = def_unpack(u, e);
  /* Cache the value in the user record. */
  u->fflags = atoi((char *) def_get(u, e));
  return ret;
}

bool fflags_set(struct userrec *u, struct user_entry *e, void *buf)
{
  bool ret;

  /* No need to share since it is sent over the tandem/botlink. */
  noshare = 1;
  ret = def_set(u, e, buf);
  noshare = 0;
  /* Cache the value in the user record. */
  u->fflags = atoi((char *) def_get(u, e));
  return ret;
}

bool fflags_gotshare(struct userrec *u, struct user_entry *e, char *data,
    int idx)
{
  /* Don't let another bot dictate our features. */
  if (u == conf.bot->u) {
    return false;
  }
  return def_gotshare(u, e, data, idx);
}

struct user_entry_type USERENTRY_FFLAGS = {
 0,
 fflags_gotshare,
 fflags_unpack,
 def_write_userfile,
 def_kill,
 def_get,
 fflags_set,
 botmisc_display,
 "FFLAGS"
};

void stats_add(struct userrec *u, int islogin, int op)
{
  if (!u || u->bot)
    return;

  char *s = (char *) get_user(&USERENTRY_STATS, u), s2[50] = "";
  int sl, so;

  if (s) {
    strlcpy(s2, s, sizeof(s2));
  } else
    strlcpy(s2, "0 0", sizeof(s2));
  s = strchr(s2, ' ');
  if (s) {
    s++;
    so = atoi(s);
  } else
    so = 0;
  sl = atoi(s2);
  if (islogin)
    sl++;
  if (op)
    so++;
  simple_snprintf(s2, sizeof(s2), "%i %i", sl, so);
  set_user(&USERENTRY_STATS, u, s2);
}

char s2_8[3] = "",s1_14[3] = "",s2_1[3] = "";

static void stats_display(int idx, struct user_entry *e, struct userrec *u)
{
  /* format: logincount opcount */
  if (!u->bot && dcc[idx].user && (dcc[idx].user->flags & USER_OWNER)) {
    char *p = strchr(e->u.string, ' ');

    if (p)
      dprintf(idx, "  -- %i logins, %i ops\n", atoi(e->u.string), atoi(p));
  }
}

struct user_entry_type USERENTRY_STATS = {
  0,				/* always 0 ;) */
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  stats_display,
  "STATS"
};

void update_mod(char *handle, char *nick, char *cmd, char *par)
{
  char tmp[100] = "";

  simple_snprintf(tmp, sizeof tmp, "%li, %s (%s %s)", (long) now, nick, cmd, (par && par[0]) ? par : "");
  set_user(&USERENTRY_MODIFIED, get_user_by_handle(userlist, handle), tmp);
}

static void modified_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (e && dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    char tmp[1024] = "", tmp2[1024] = "", *hnd = NULL;
    time_t tm;

    strlcpy(tmp, e->u.string, sizeof(tmp));
    hnd = strchr(tmp, ' ');
    if (hnd)
      *hnd++ = 0;
    tm = atoi(tmp);
    strftime(tmp2, sizeof(tmp2), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&tm));
    if (hnd)
      dprintf(idx, "  -- Modified %s by %s\n", tmp2, hnd);
    else
      dprintf(idx, "  -- Modified %s\n", tmp2);
  }
}

struct user_entry_type USERENTRY_MODIFIED =
{
  0,
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  modified_display,
  "MODIFIED"
};

static bool pass_set(struct userrec *u, struct user_entry *e, void *buf)
{
  char *newpass = NULL;
  char *pass = (char *) buf;

  free(e->u.extra);
  if (!pass || !pass[0] || (pass[0] == '-'))
    e->u.extra = NULL;
  else {
    unsigned char *p = (unsigned char *) pass;

    if (u->bot || (pass[0] == '+'))
      newpass = strdup(pass);
    else {
      while (*p) {
        if ((*p <= 32) || (*p == 127))
          *p = '?';
        p++;
      }
      newpass = salted_sha1(pass);
    }
    e->u.extra = strdup(newpass);
  }
  if (!noshare)
    shareout("c %s %s %s\n", e->type->name, u->handle, newpass ? newpass : "");
  free(newpass);

  /* clear old records */
  noshare = 1;
  set_user(&USERENTRY_PASS1, u, NULL);
  noshare = 0;
  return 1;
}

struct user_entry_type USERENTRY_PASS =
{
  0,
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  pass_set,
  NULL,
  "PASS2"
};

static bool pass1_set(struct userrec *u, struct user_entry *e, void *buf)
{
  char *pass = (char *) buf;

  free(e->u.extra);
  if (!pass || !pass[0] || (pass[0] == '-'))
    e->u.extra = NULL;
  else {
    unsigned char *p = (unsigned char *) pass;

    while (*p) {
      if ((*p <= 32) || (*p == 127))
	*p = '?';
      p++;
    }
    if (u->bot || (pass[0] == '+'))
      e->u.extra = strdup(pass);
    else
      e->u.extra = encrypt_string(u->handle, pass);
  }
  if (!noshare)
    shareout("c %s %s %s\n", e->type->name, u->handle, pass ? pass : "");
  return 1;
}

struct user_entry_type USERENTRY_PASS1 =
{
  0,
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  pass1_set,
  NULL,
  "PASS1"
};


static void secpass_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  if (!strcmp(u->handle, dcc[idx].nick) || (glob_admin(fr) && isowner(dcc[idx].nick))) {
    if (conf.bot->hub)
      dprintf(idx, "  %s: %s\n", e->type->name, e->u.string);
    else {
      dprintf(idx, "  %s: Hidden on leaf bots.", e->type->name);
      if (dcc[idx].u.chat->su_nick)
        dprintf(idx, " Nice try, %s.", dcc[idx].u.chat->su_nick);
      dprintf(idx, "\n");
    }
  }
}

struct user_entry_type USERENTRY_SECPASS =
{
  0,
  def_gotshare,
  def_unpack,
  def_write_userfile,
  def_kill,
  def_get,
  def_set,
  secpass_display,
  "SECPASS"
};

static bool laston_unpack(struct userrec *u, struct user_entry *e)
{
  char *par = e->u.list->extra, *arg = newsplit(&par);
  struct laston_info *li = (struct laston_info *) calloc(1, sizeof(struct laston_info));

  if (!par[0])
    par = "???";
  li->laston = atoi(arg);
  li->lastonplace = strdup(par);
  list_type_kill(e->u.list);
  e->u.extra = li;
  return 1;
}

static void laston_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  struct laston_info *li = (struct laston_info *) e->u.extra;

  stream << bd::String::printf("--LASTON %li %s\n", (long) li->laston, li->lastonplace ? li->lastonplace : "");
}

static bool laston_kill(struct user_entry *e)
{
  free(((struct laston_info *) (e->u.extra))->lastonplace);
  free(e->u.extra);
  free(e);
  return 1;
}

static bool laston_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct laston_info *li = (struct laston_info *) e->u.extra;

  if (li != buf) {
    if (li) {
      free(li->lastonplace);
      free(li);
    }

    li = (struct laston_info *) buf;
    e->u.extra = (struct laston_info *) buf;
  }

  if (!noshare)
    shareout_hub("c LASTON %s %s %li\n", u->handle, li->lastonplace ? li->lastonplace : "-", (long) li->laston);

  return 1;
}

static bool laston_gotshare(struct userrec *u, struct user_entry *e, char *par, int idx)
{
  char *where = NULL;
  time_t timeval = 0;

  if (par[0])
    where = newsplit(&par);
  if (where && !strcmp(where, "-"))
    where = NULL;
  if (par[0])
    timeval = atol(newsplit(&par));
  touch_laston(u, where, timeval);

  return 1;
}


struct user_entry_type USERENTRY_LASTON =
{
  0,				/* always 0 ;) */
  laston_gotshare,
  laston_unpack,
  laston_write_userfile,
  laston_kill,
  def_get,
  laston_set,
  0,
  "LASTON"
};

static bool botaddr_unpack(struct userrec *u, struct user_entry *e)
{
  char p[1024] = "", *q1 = NULL, *q2 = NULL;
  struct bot_addr *bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

  /* address:port/port:hublevel:uplink */

  strlcpy(p, e->u.list->extra, sizeof(p));
  q1 = strchr(p, ':');
  if (q1)
    *q1++ = 0;
  bi->address = strdup(p);
  if (q1) {
    q2 = strchr(q1, ':');
    if (q2)
      *q2++ = 0;
    bi->telnet_port = atoi(q1);
    q1 = strchr(q1, '/');
    if (q1) {
      q1++;
      bi->relay_port = atoi(q1);
    }
    if (q2) {
      q1 = strchr(q2, ':');
      if (q1) {
        *q1++ = 0;
        bi->uplink = strdup(q1);
      }
      bi->hublevel = atoi(q2);
    }
  }
  if (!bi->telnet_port)
    bi->telnet_port = 3333;
  if (!bi->relay_port)
    bi->relay_port = bi->telnet_port;
  if (!bi->uplink) {
    bi->uplink = (char *) calloc(1, 1);
  }
  list_type_kill(e->u.list);
  e->u.extra = bi;
  return 1;

}

static bool botaddr_kill(struct user_entry *e)
{
  free(((struct bot_addr *) (e->u.extra))->address);
  free(((struct bot_addr *) (e->u.extra))->uplink);
  free(e->u.extra);
  free(e);
  return 1;
}

static void botaddr_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  struct bot_addr *bi = (struct bot_addr *) e->u.extra;

  stream << bd::String::printf("--%s %s:%u/%u:%u:%s\n", e->type->name, bi->address, bi->telnet_port, bi->relay_port, bi->hublevel, bi->uplink);
}

static bool botaddr_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct bot_addr *bi = (struct bot_addr *) e->u.extra;

  if (!bi && !buf)
    return 1;
  if (bi != buf) {
    if (bi) {
      free(bi->address);
      free(bi->uplink);
      free(bi);
    }
    ContextNote("botaddr_set", "(sharebug) occurred in botaddr_set");
    bi = (struct bot_addr *) buf;
    e->u.extra = (struct bot_addr *) buf;
  }
  if (bi && !noshare) {
    shareout("c BOTADDR %s %s %d %d %d %s\n",u->handle, 
            (bi->address && bi->address[0]) ? bi->address : "127.0.0.1", 
            bi->telnet_port, bi->relay_port, bi->hublevel, bi->uplink);
  }
  return 1;
}

static void botaddr_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (conf.bot->hub) {
    struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

    get_user_flagrec(dcc[idx].user, &fr, NULL);
    if (glob_admin(fr)) {
      struct bot_addr *bi = (struct bot_addr *) e->u.extra;

      if (bi->hublevel && bi->hublevel != 999) {
        if (bi->address) {
          dprintf(idx, "  ADDRESS: %.70s\n", bi->address);
          dprintf(idx, "     port: %d\n", bi->telnet_port);
        }
        dprintf(idx, "  HUBLEVEL: %d\n", bi->hublevel);
      }
      if (bi->uplink && bi->uplink[0])
        dprintf(idx, "  UPLINK: %s\n", bi->uplink);
    }
  }
}

static bool botaddr_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  struct bot_addr *bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));
  char *arg = newsplit(&buf);

  bi->address = strdup(arg);
  arg = newsplit(&buf);
  bi->telnet_port = atoi(arg);
  arg = newsplit(&buf);
  bi->relay_port = atoi(arg);
  arg = newsplit(&buf);
  bi->hublevel = atoi(arg);
  bi->uplink = strdup(buf);
  if (!bi->telnet_port)
    bi->telnet_port = 3333;
  if (!bi->relay_port)
    bi->relay_port = bi->telnet_port;
  return botaddr_set(u, e, bi);
}

struct user_entry_type USERENTRY_BOTADDR =
{
  0,				/* always 0 ;) */
  botaddr_gotshare,
  botaddr_unpack,
  botaddr_write_userfile,
  botaddr_kill,
  def_get,
  botaddr_set,
  botaddr_display,
  "BOTADDR"
};

static void hosts_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  struct list_type *h = NULL;

  for (h = (struct list_type *) e->u.extra; h; h = h->next)
    stream << bd::String::printf("--HOSTS %s\n", h->extra);
}

static bool hosts_null(struct userrec *u, struct user_entry *e)
{
  return 1;
}

static bool hosts_kill(struct user_entry *e)
{
  list_type_kill(e->u.list);
  free(e);
  return 1;
}

static void hosts_display(int idx, struct user_entry *e, struct userrec *u)
{
  /* if this is a su, dont show hosts
   * otherwise, let users see their own hosts */
  if (conf.bot->hub || 
     (!conf.bot->hub && (dcc[idx].simul || (!strcmp(u->handle,dcc[idx].nick) && !dcc[idx].u.chat->su_nick)))) { 
    char s[1024] = "";
    struct list_type *q = NULL;

    strlcpy(s, "  HOSTS: ", sizeof(s));
    for (q = e->u.list; q; q = q->next) {
      if (s[0] && !s[9])
        strlcat(s, q->extra, sizeof(s));
      else if (!s[0])
        simple_snprintf(s, sizeof(s), "         %s", q->extra);
      else {
        if (strlen(s) + strlen(q->extra) + 2 > 65) {
  	  dprintf(idx, "%s\n", s);
  	  simple_snprintf(s, sizeof(s), "         %s", q->extra);
        } else {
  	  strlcat(s, ", ", sizeof(s));
  	  strlcat(s, q->extra, sizeof(s));
        }
      }
    }
    if (s[0])
      dprintf(idx, "%s\n", s);
  } else if (!conf.bot->hub) {
    dprintf(idx, "  HOSTS:          Hidden on leaf bots.");
    if (dcc[idx].u.chat->su_nick)
      dprintf(idx, " Nice try, %s.", dcc[idx].u.chat->su_nick);
    dprintf(idx, "\n");
  }
}

static bool hosts_set(struct userrec *u, struct user_entry *e, void *buf)
{
  if (!buf || !strcasecmp((const char *) buf, "none")) {
    /* When the bot crashes, it's in this part, not in the 'else' part */
    list_type_kill(e->u.list);
    e->u.list = NULL;
  } else {
    char *host = (char *) buf, *p = strchr(host, ',');
    struct list_type **t;

    /* Can't have ,'s in hostmasks */
    while (p) {
      *p = '?';
      p = strchr(host, ',');
    }
    /* fred1: check for redundant hostmasks with
     * controversial "superpenis" algorithm ;) */
    /* I'm surprised Raistlin hasn't gotten involved in this controversy */
    t = &(e->u.list);
    while (*t) {
      if (wild_match(host, (*t)->extra)) {
	struct list_type *listu;

	listu = *t;
	*t = (*t)->next;
        free(listu->extra);
	free(listu);
      } else
	t = &((*t)->next);
    }
    *t = (struct list_type *) calloc(1, sizeof(struct list_type));

    (*t)->next = NULL;
    (*t)->extra = strdup(host);
  }
  return 1;
}

static bool hosts_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  /* doh, try to be too clever and it bites your butt */
  return 0;
}

struct user_entry_type USERENTRY_HOSTS =
{
  0,
  hosts_gotshare,
  hosts_null,
  hosts_write_userfile,
  hosts_kill,
  def_get,
  hosts_set,
  hosts_display,
  "HOSTS"
};

bool add_entry_type(struct user_entry_type *type)
{
  struct userrec *u = NULL;

  list_insert((&entry_type_list), type);
  for (u = userlist; u; u = u->next) {
    struct user_entry *e = find_user_entry(type, u);

    if (e && e->name) {
      e->type = type;
      e->type->unpack(u, e);
      free(e->name);
      e->name = NULL;
    }
  }
  return 1;
}

struct user_entry_type *find_entry_type(const char *name)
{
  struct user_entry_type *p = NULL;

  for (p = entry_type_list; p; p = p->next) {
    if (!strcasecmp(name, p->name))
      return p;
  }
  return NULL;
}

struct user_entry *find_user_entry(struct user_entry_type *et, struct userrec *u)
{
  struct user_entry **e = NULL, *t = NULL;

  for (e = &(u->entries); *e; e = &((*e)->next)) {
    if (((*e)->type == et) || ((*e)->name && !strcasecmp((*e)->name, et->name))) {
      t = *e;
      *e = t->next;
      t->next = u->entries;
      u->entries = t;
      return t;
    }
  }
  return NULL;
}

void *get_user(struct user_entry_type *et, struct userrec *u)
{
  struct user_entry *e = NULL;

  if (u && (e = find_user_entry(et, u)))
    return et->get(u, e);
  return NULL;
}

bool set_user(struct user_entry_type *et, struct userrec *u, void *d)
{
  if (!u || !et)
    return 0;

  struct user_entry *e = NULL;
  bool r;

  if (!(e = find_user_entry(et, u))) {
    e = (struct user_entry *) calloc(1, sizeof(struct user_entry));

    e->type = et;
    e->name = NULL;
    e->u.list = NULL;
    list_insert((&(u->entries)), e);
  }
  r = et->set(u, e, d);
  if (!e->u.list) {
    list_delete((struct list_type **) &(u->entries), (struct list_type *) e);
    free(e);
  }
  return r;
}
/* vim: set sts=2 sw=2 ts=8 et: */
