/*
 * userent.c -- handles:
 *   user-entry handling, new stylem more versatile.
 *
 */

#include "common.h"
#include "users.h"
#include "src/mod/share.mod/share.h"
#include "misc.h"
#include "main.h"
#include "debug.h"
#include "userrec.h"
#include "match.h"
#include "dccutil.h"
#include "crypt.h"
#include "botmsg.h"

static struct user_entry_type *entry_type_list;

void init_userent()
{
  entry_type_list = 0;
  add_entry_type(&USERENTRY_COMMENT);
  add_entry_type(&USERENTRY_INFO);
  add_entry_type(&USERENTRY_LASTON);
  add_entry_type(&USERENTRY_BOTADDR);
  add_entry_type(&USERENTRY_OS);
  add_entry_type(&USERENTRY_NODENAME);
  add_entry_type(&USERENTRY_USERNAME);
  add_entry_type(&USERENTRY_PASS);
  add_entry_type(&USERENTRY_SECPASS);
  add_entry_type(&USERENTRY_HOSTS);
  add_entry_type(&USERENTRY_STATS);
  add_entry_type(&USERENTRY_ADDED);
  add_entry_type(&USERENTRY_MODIFIED);
  add_entry_type(&USERENTRY_CONFIG);
}

void list_type_kill(struct list_type *t)
{
  struct list_type *u = NULL;

  while (t) {
    u = t->next;
    if (t->extra)
      free(t->extra);
    free(t);
    t = u;
  }
}

int def_unpack(struct userrec *u, struct user_entry *e)
{
  char *tmp = NULL;

  tmp = e->u.list->extra;
  e->u.list->extra = NULL;
  list_type_kill(e->u.list);
  e->u.string = tmp;
  return 1;
}

int def_kill(struct user_entry *e)
{
  free(e->u.string);
  free(e);
  return 1;
}

#ifdef HUB
int def_write_userfile(FILE * f, struct userrec *u, struct user_entry *e)
{
  if (lfprintf(f, "--%s %s\n", e->type->name, e->u.string) == EOF)
    return 0;
  return 1;
}
#endif /* HUB */

void *def_get(struct userrec *u, struct user_entry *e)
{
  return e->u.string;
}

int def_set(struct userrec *u, struct user_entry *e, void *buf)
{
  char *string = (char *) buf;

  if (string && !string[0])
    string = NULL;
  if (!string && !e->u.string)
    return 1;
  if (string) {
    int l = strlen (string);
    char *i;

    if (l > 160)
      l = 160;


    e->u.string = (char *) realloc (e->u.string, l + 1);

    strncpyz (e->u.string, string, l + 1);

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
    shareout("c %s %s %s\n", e->type->name, u->handle, e->u.string ? e->u.string : "");
  }
  return 1;
}

int def_gotshare(struct userrec *u, struct user_entry *e, char *data, int idx)
{
#ifdef HUB
  putlog(LOG_DEBUG, "@", "%s: change %s %s", dcc[idx].nick, e->type->name, u->handle);
#endif
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
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
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
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  def_set,
  def_display,
  "INFO"
};

void added_display(int idx, struct user_entry *e, struct userrec *u)
{
  /* format: unixtime handle */
  if (dcc[idx].user && (dcc[idx].user->flags & USER_OWNER)) {
    char tmp[30] = "", tmp2[70] = "", *hnd = NULL;
    time_t tm;

    strncpyz(tmp, e->u.string, sizeof(tmp));
    hnd = strchr(tmp, ' ');
    if (hnd)
      *hnd++ = 0;
    tm = atoi(tmp);

    egg_strftime(tmp2, sizeof(tmp2), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&tm));
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
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  def_set,
  added_display,
  "ADDED"
};

int config_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct xtra_key *curr = NULL, *old = NULL, *mynew = (struct xtra_key *) buf;

  for (curr = (struct xtra_key *) e->u.extra; curr; curr = curr->next) {
    if (curr->key && !egg_strcasecmp(curr->key, mynew->key)) {
      old = curr;
      break;
    }
  }
  if (!old && (!mynew->data || !mynew->data[0])) {
    /* delete non-existant entry */
    free(mynew->key);
    if (mynew->data)
      free(mynew->data);
    free(mynew);
    return 1;
  }

  /* we will possibly free new below, so let's send the information
   * to the botnet now */
  if (!noshare && !cfg_noshare)
    shareout("c CONFIG %s %s %s\n", u->handle, mynew->key, mynew->data ? mynew->data : "");
  if ((old && old != mynew) || !mynew->data || !mynew->data[0]) {
    list_delete((struct list_type **) (&e->u.extra), (struct list_type *) old);

    free(old->key);
    free(old->data);
    free(old);
  }
  if (old != mynew && mynew->data) {
    if (mynew->data[0]) {
      list_insert((struct xtra_key **) (&e->u.extra), mynew);
    } else {
      if (mynew->data)
        free(mynew->data);
      free(mynew->key);
      free(mynew);
    }
  }
  return 1;
}

int config_unpack(struct userrec *u, struct user_entry *e)
{
  struct list_type *curr = NULL, *head = NULL;
  struct xtra_key *t = NULL;
  char *key = NULL, *data = NULL;

  head = curr = e->u.list;
  e->u.extra = NULL;
  while (curr) {
    t = (struct xtra_key *) calloc(1, sizeof(struct xtra_key));

    data = curr->extra;
    key = newsplit(&data);
    if (data[0]) {
      t->key = strdup(key);
      t->data = strdup(data);
      list_insert((struct xtra_key **) (&e->u.extra), t);
    }
    curr = curr->next;
  }
  list_type_kill(head);
  return 1;
}

void config_display(int idx, struct user_entry *e, struct userrec *u)
{
#ifdef HUB
  struct xtra_key *xk = NULL;
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  /* scan thru xtra field, searching for matches */
  for (xk = e->u.extra; xk; xk = xk->next) {
    /* ok, it's a valid xtra field entry */
    if (glob_owner(fr))
      dprintf(idx, "  %s: %s\n", xk->key, xk->data);
  }
#endif /* HUB */
}

int config_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  char *arg = NULL;
  struct xtra_key *xk = NULL;
  int l;

  arg = newsplit(&buf);
  if (!arg[0])
    return 1;
  if (!strcmp(u->handle, conf.bot->nick)) {
    struct cfg_entry *cfgent = NULL;
    int i;

    cfg_noshare = 1;
    for (i = 0; !cfgent && (i < cfg_count); i++)
      if (!strcmp(arg, cfg[i]->name) && (cfg[i]->flags & CFGF_LOCAL))
	cfgent = cfg[i];
    if (cfgent) {
      set_cfg_str(conf.bot->nick, cfgent->name, (buf && buf[0]) ? buf : NULL);
    }
    cfg_noshare = 0;
    return 1;
  }

  xk = (struct xtra_key *) calloc(1, sizeof(struct xtra_key));

  l = strlen(arg);
  if (l > 1500)
    l = 1500;
  xk->key = (char *) calloc(1, l + 1);
  strncpy(xk->key, arg, l + 1);

  if (buf && buf[0]) {
    int k = strlen(buf);

    if (k > 1500 - l)
      k = 1500 - l;
    xk->data = (char *) calloc(1, k + 1);
    strncpy(xk->data, buf, k + 1);
  }
  config_set(u, e, xk);

  return 1;
}

#ifdef HUB
int config_write_userfile(FILE *f, struct userrec *u, struct user_entry *e)
{
  struct xtra_key *x = NULL;

  for (x = e->u.extra; x; x = x->next)
    lfprintf(f, "--CONFIG %s %s\n", x->key, x->data);
  return 1;
}
#endif /* HUB */

int config_kill(struct user_entry *e)
{
  struct xtra_key *x = NULL, *y = NULL;

  for (x = (struct xtra_key *) e->u.extra; x; x = y) {
    y = x->next;
    free(x->key);
    free(x->data);
    free(x);
  }
  free(e);
  return 1;
}

struct user_entry_type USERENTRY_CONFIG = {
  0,
  config_gotshare,
  config_unpack,
#ifdef HUB
  config_write_userfile,
#endif /* HUB */
  config_kill,
  def_get,
  config_set,
  config_display,
  "CONFIG"
};

#ifdef HUB
static void botmisc_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  if (glob_admin(fr))
    dprintf(idx, "  %s: %s\n", e->type->name, e->u.string ? e->u.string : "");
}
#endif /* HUB */

struct user_entry_type USERENTRY_USERNAME = {
 0,
 def_gotshare,
 def_unpack,
#ifdef HUB
 def_write_userfile,
#endif /* HUB */
 def_kill,
 def_get,
 def_set,
#ifdef HUB
 botmisc_display,
#else
 NULL,
#endif /* HUB */
 "USERNAME"
};

struct user_entry_type USERENTRY_NODENAME = {
 0,
 def_gotshare,
 def_unpack,
#ifdef HUB
 def_write_userfile,
#endif /* HUB */
 def_kill,
 def_get,
 def_set,
#ifdef HUB
 botmisc_display,
#else
 NULL,
#endif /* HUB */
 "NODENAME"
};

struct user_entry_type USERENTRY_OS = {
 0,
 def_gotshare,
 def_unpack,
#ifdef HUB
 def_write_userfile,
#endif /* HUB */
 def_kill,
 def_get,
 def_set,
#ifdef HUB
 botmisc_display,
#else
 NULL,
#endif /* HUB */
 "OS"
};


void stats_add(struct userrec *u, int login, int op)
{
  char *s = NULL, s2[50] = "";
  int sl, so;

  if (!u)
    return;
  s = (char *) get_user(&USERENTRY_STATS, u);
  if (s) {
    strncpyz(s2, s, sizeof(s2));
  } else
    strcpy(s2, "0 0");
  s = strchr(s2, ' ');
  if (s) {
    s++;
    so = atoi(s);
  } else
    so = 0;
  sl = atoi(s2);
  if (login)
    sl++;
  if (op)
    so++;
  sprintf(s2, "%i %i", sl, so);
  set_user(&USERENTRY_STATS, u, s2);
}

void stats_display(int idx, struct user_entry *e, struct userrec *u)
{
  /* format: logincount opcount */
  if (dcc[idx].user && (dcc[idx].user->flags & USER_OWNER)) {
    char *p;

    p = strchr(e->u.string, ' ');
    if (p)
      dprintf(idx, "  -- %i logins, %i ops\n", atoi(e->u.string), atoi(p));
  }
}

struct user_entry_type USERENTRY_STATS = {
  0,				/* always 0 ;) */
  def_gotshare,
  def_unpack,
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  def_set,
  stats_display,
  "STATS"
};

void update_mod(char *handle, char *nick, char *cmd, char *par)
{
  char tmp[100] = "";

  egg_snprintf(tmp, sizeof tmp, "%li, %s (%s %s)", now, nick, cmd, (par && par[0]) ? par : "");
  set_user(&USERENTRY_MODIFIED, get_user_by_handle(userlist, handle), tmp);
}

void modified_display(int idx, struct user_entry *e, struct userrec *u)
{
  if (e && dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    char tmp[1024] = "", tmp2[1024] = "", *hnd = NULL;
    time_t tm;

    strncpyz(tmp, e->u.string, sizeof(tmp));
    hnd = strchr(tmp, ' ');
    if (hnd)
      *hnd++ = 0;
    tm = atoi(tmp);
    egg_strftime(tmp2, sizeof(tmp2), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&tm));
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
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  def_set,
  modified_display,
  "MODIFIED"
};

int pass_set(struct userrec *u, struct user_entry *e, void *buf)
{
  char newpass[32] = "";
  register char *pass = (char *) buf;

  if (e->u.extra)
    free(e->u.extra);
  if (!pass || !pass[0] || (pass[0] == '-'))
    e->u.extra = NULL;
  else {
    unsigned char *p = (unsigned char *) pass;

    if (strlen(pass) > MAXPASSLEN)
      pass[MAXPASSLEN] = 0;
    while (*p) {
      if ((*p <= 32) || (*p == 127))
	*p = '?';
      p++;
    }
    if (u->bot || (pass[0] == '+'))
      strcpy(newpass, pass);
    else
      encrypt_pass(pass, newpass);
    e->u.extra = strdup(newpass);
  }
  if (!noshare)
    shareout("c PASS %s %s\n", u->handle, pass ? pass : "");
  return 1;
}

struct user_entry_type USERENTRY_PASS =
{
  0,
  def_gotshare,
  def_unpack,
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  pass_set,
  NULL,
  "PASS"
};


void secpass_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  if (!strcmp(u->handle, dcc[idx].nick) || glob_admin(fr)) {
#ifdef HUB
    dprintf(idx, "  %s: %s\n", e->type->name, e->u.string);
#else
    dprintf(idx, "  %s: Hidden on leaf bots.", e->type->name);
    if (dcc[idx].u.chat->su_nick)
      dprintf(idx, " Nice try, %s.", dcc[idx].u.chat->su_nick);
    dprintf(idx, "\n");
#endif /* HUB */
  }
}


struct user_entry_type USERENTRY_SECPASS =
{
  0,
  def_gotshare,
  def_unpack,
#ifdef HUB
  def_write_userfile,
#endif /* HUB */
  def_kill,
  def_get,
  def_set,
  secpass_display,
  "SECPASS"
};

static int laston_unpack(struct userrec *u, struct user_entry *e)
{
  char *par = NULL, *arg = NULL;
  struct laston_info *li = NULL;

  par = e->u.list->extra;
  arg = newsplit (&par);
  if (!par[0])
    par = "???";
  li = (struct laston_info *) calloc(1, sizeof(struct laston_info));
  li->laston = atoi(arg);
  li->lastonplace = strdup(par);
  list_type_kill(e->u.list);
  e->u.extra = li;
  return 1;
}

#ifdef HUB
static int laston_write_userfile(FILE * f, struct userrec *u, struct user_entry *e)
{
  struct laston_info *li = (struct laston_info *) e->u.extra;

  if (lfprintf(f, "--LASTON %lu %s\n", li->laston,
	      li->lastonplace ? li->lastonplace : "") == EOF)
    return 0;
  return 1;
}
#endif /* HUB */

static int laston_kill(struct user_entry *e)
{
  if (((struct laston_info *) (e->u.extra))->lastonplace)
    free(((struct laston_info *) (e->u.extra))->lastonplace);
  free(e->u.extra);
  free(e);
  return 1;
}

static int laston_set(struct userrec *u, struct user_entry *e, void *buf)
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

  /* FIXME: laston sharing is disabled until a better solution is found
  if (!noshare)
    shareout("c LASTON %s %s %li\n", u->handle, li->lastonplace ? li->lastonplace : "-", li->laston);
  */

  return 1;
}

static int laston_gotshare(struct userrec *u, struct user_entry *e, char *par, int idx)
{
  char *where = NULL;
  time_t timeval = 0;

  if (par[0])
    where = newsplit(&par);
  if (!strcmp(where, "-"))
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
#ifdef HUB
  laston_write_userfile,
#endif /* HUB */
  laston_kill,
  def_get,
  laston_set,
  0,
  "LASTON"
};

static int botaddr_unpack(struct userrec *u, struct user_entry *e)
{
  char p[1024] = "", *q1 = NULL, *q2 = NULL;
  struct bot_addr *bi = NULL;

  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

  /* address:port/port:hublevel:uplink */
  Context;
  Assert(e);
  Assert(e->name);

  strcpy(p, e->u.list->extra);
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

static int botaddr_kill(struct user_entry *e)
{
  free(((struct bot_addr *) (e->u.extra))->address);
  free(((struct bot_addr *) (e->u.extra))->uplink);
  free(e->u.extra);
  free(e);
  return 1;
}

#ifdef HUB
static int botaddr_write_userfile(FILE *f, struct userrec *u, struct user_entry *e)
{
  register struct bot_addr *bi = (struct bot_addr *) e->u.extra;

  if (lfprintf(f,  "--%s %s:%u/%u:%u:%s\n", e->type->name, bi->address,
	      bi->telnet_port, bi->relay_port, bi->hublevel, bi->uplink) == EOF)
    return 0;
  return 1;
}
#endif /* HUB */

static int botaddr_set(struct userrec *u, struct user_entry *e, void *buf)
{
  register struct bot_addr *bi = (struct bot_addr *) e->u.extra;

  Context;
  if (!bi && !buf)
    return 1;
  if (bi != buf) {
    if (bi) {
      Assert(bi->address);
      free(bi->address);
      Assert(bi->uplink);
      free(bi->uplink);
      free(bi);
    }
    ContextNote("(sharebug) occurred in botaddr_set");
    bi = (struct bot_addr *) buf;
    e->u.extra = (struct bot_addr *) buf;
  }
  Assert(u);
  if (bi && !noshare) {
    shareout("c BOTADDR %s %s %d %d %d %s\n",u->handle, 
            (bi->address && bi->address[0]) ? bi->address : "127.0.0.1", 
            bi->telnet_port, bi->relay_port, bi->hublevel, bi->uplink);
  }
  return 1;
}

#ifdef HUB
static void botaddr_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  if (glob_admin(fr)) {
    register struct bot_addr *bi = (struct bot_addr *) e->u.extra;
    if (bi->address && bi->hublevel && bi->hublevel != 0) {
      dprintf(idx, "  ADDRESS: %.70s\n", bi->address);
      dprintf(idx, "     port: %d\n", bi->telnet_port);
    }
    if (bi->hublevel && bi->hublevel != 0)
      dprintf(idx, "  HUBLEVEL: %d\n", bi->hublevel);
    if (bi->uplink && bi->uplink[0])
      dprintf(idx, "  UPLINK: %s\n", bi->uplink);
  }
}
#endif /* HUB */

static int botaddr_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  struct bot_addr *bi = NULL;
  char *arg = NULL;

  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));
  arg = newsplit(&buf);
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
#ifdef HUB
  botaddr_write_userfile,
#endif /* HUB */
  botaddr_kill,
  def_get,
  botaddr_set,
#ifdef HUB
  botaddr_display,
#else
  NULL,
#endif /* HUB */
  "BOTADDR"
};

#ifdef HUB
static int hosts_write_userfile(FILE *f, struct userrec *u, struct user_entry *e)
{
  struct list_type *h = NULL;

  for (h = e->u.extra; h; h = h->next)
    if (lfprintf(f, "--HOSTS %s\n", h->extra) == EOF)
      return 0;
  return 1;
}
#endif /* HUB */

static int hosts_null(struct userrec *u, struct user_entry *e)
{
  return 1;
}

static int hosts_kill(struct user_entry *e)
{
  list_type_kill(e->u.list);
  free(e);
  return 1;
}

static void hosts_display(int idx, struct user_entry *e, struct userrec *u)
{
#ifdef LEAF
  /* if this is a su, dont show hosts
   * otherwise, let users see their own hosts */
  if (dcc[idx].simul || (!strcmp(u->handle,dcc[idx].nick) && !dcc[idx].u.chat->su_nick)) { 
#endif /* LEAF */
    char s[1024] = "";
    struct list_type *q = NULL;

    strcpy(s, "  HOSTS: ");
    for (q = e->u.list; q; q = q->next) {
      if (s[0] && !s[9])
        strcat(s, q->extra);
      else if (!s[0])
        sprintf(s, "         %s", q->extra);
      else {
        if (strlen(s) + strlen(q->extra) + 2 > 65) {
  	  dprintf(idx, "%s\n", s);
  	  sprintf(s, "         %s", q->extra);
        } else {
  	  strcat(s, ", ");
  	  strcat(s, q->extra);
        }
      }
    }
    if (s[0])
      dprintf(idx, "%s\n", s);
#ifdef LEAF
  } else {
    dprintf(idx, "  HOSTS:          Hidden on leaf bots.");
    if (dcc[idx].u.chat->su_nick)
      dprintf(idx, " Nice try, %s.", dcc[idx].u.chat->su_nick);
    dprintf(idx, "\n");
  }
#endif /* LEAF */
}

static int hosts_set(struct userrec *u, struct user_entry *e, void *buf)
{
  if (!buf || !egg_strcasecmp((const char *) buf, "none")) {
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
	if (listu->extra)
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

static int hosts_gotshare(struct userrec *u, struct user_entry *e, char *buf, int idx)
{
  /* doh, try to be too clever and it bites your butt */
  return 0;
}

struct user_entry_type USERENTRY_HOSTS =
{
  0,
  hosts_gotshare,
  hosts_null,
#ifdef HUB
  hosts_write_userfile,
#endif /* HUB */
  hosts_kill,
  def_get,
  hosts_set,
  hosts_display,
  "HOSTS"
};

int list_append(struct list_type **h, struct list_type *i)
{
  for (; *h; h = &((*h)->next));
  *h = i;
  return 1;
}

int list_delete(struct list_type **h, struct list_type *i)
{
  for (; *h; h = &((*h)->next))
    if (*h == i) {
      *h = i->next;
      return 1;
    }
  return 0;
}

int list_contains(struct list_type *h, struct list_type *i)
{
  for (; h; h = h->next)
    if (h == i) {
      return 1;
    }
  return 0;
}

int add_entry_type(struct user_entry_type *type)
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

struct user_entry_type *find_entry_type(char *name)
{
  struct user_entry_type *p = NULL;

  for (p = entry_type_list; p; p = p->next) {
    if (!egg_strcasecmp(name, p->name))
      return p;
  }
  return NULL;
}

struct user_entry *find_user_entry(struct user_entry_type *et, struct userrec *u)
{
  struct user_entry **e = NULL, *t = NULL;

  for (e = &(u->entries); *e; e = &((*e)->next)) {
    if (((*e)->type == et) ||
	((*e)->name && !egg_strcasecmp((*e)->name, et->name))) {
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

int set_user(struct user_entry_type *et, struct userrec *u, void *d)
{
  struct user_entry *e = NULL;
  int r;

  if (!u || !et)
    return 0;

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
