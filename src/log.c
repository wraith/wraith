#include "main.h"
#include <sys/stat.h>
#include "hook.h"
#include "tandem.h"

extern time_t now;
extern struct dcc_t *dcc;
extern int dcc_total,
  backgrd,
  noshare;
extern struct userrec *userlist;
extern char botnetnick[], *binname, netpass[];
extern int maxqmsg;
#ifdef LEAF
extern struct msgq_head hq;
extern void * qserver;
extern void * null_func;
#endif

#define logchan (CFG_LOGCHAN.gdata ? CFG_LOGCHAN.gdata : "")
#define logbot (CFG_LOGBOT.gdata ? CFG_LOGBOT.gdata : "")

struct logcategory *logcat = NULL;
struct user_entry_type USERENTRY_LOG;
struct cfg_entry CFG_LOGCHAN,
  CFG_LOGBOT;

void log_addcategory(char *name, char *desc, int tochan, int tofile, int broadcast, int flags)
{
  struct logcategory *lc,
   *lc2;
  lc = nmalloc(sizeof(struct logcategory));
  bzero(lc, sizeof(struct logcategory));

  lc->name = nmalloc(strlen(name) + 1);
  strcpy(lc->name, name);
  lc->desc = nmalloc(strlen(desc) + 1);
  strcpy(lc->desc, desc);
  lc->logtochan = tochan;
  lc->logtofile = tofile;
  lc->broadcast = broadcast;
  lc->flags = flags;
  if (logcat) {
    if (strcmp(lc->name, logcat->name) < 0) {
      lc->next = logcat;
      logcat = lc;
    } else {
      lc2 = logcat;
      while ((lc2->next) && (strcmp(lc->name, lc2->next->name) > 0))
	lc2 = lc2->next;
      lc->next = lc2->next;
      lc2->next = lc;
    }
  } else
    logcat = lc;
}

#ifdef LEAF
int queueoverrun=0;
#endif

void do_logtochan(char * from, struct logcategory *lc, char *msg) {
#ifdef LEAF
  struct tm *T = gmtime(&now);
  if (!from)
    from=botnetnick;
  if (qserver!=null_func) {
    if (hq.tot>50) {
      if (!queueoverrun) {
	struct msgq *q,
	  *qq;
	queueoverrun=1;
	q = hq.head;
	while (q) {
	  qq = q->next;
	  nfree(q->msg);
	  nfree(q);
	  q = qq;
	}
	hq.tot = hq.warned = 0;
	hq.head = hq.last = 0;
	dprintf(DP_HELP, STR("PRIVMSG %s :\002QUEUE OVERRUN - FLUSHED - CHANNEL LOG MESSAGES LOST\002\n"), logchan);
      }
    } else {
      dprintf(DP_HELP, STR("PRIVMSG %s :[%s] (%02d:%02d) \002%s\002: %s\n"), logchan,
	      from, T->tm_hour, T->tm_min, lc->name, msg);
      queueoverrun=0;
    }
  }
#endif
}

void logtochan(char *from, struct logcategory *lc, char *msg)
{
  char *p1, *p2, *tmp;
  int logbots=0;
  if (!logbot[0] || !logchan[0])
    return;
  tmp=nmalloc(strlen(logbot)+1);
  strcpy(tmp, logbot);
  p1=tmp;
  p2=newsplit(&p1);
  while (p2 && p2[0]) {
    if (!strcasecmp2(p2, botnetnick)) {
      do_logtochan(from, lc, msg);
      nfree(tmp);
      return;
    }
    if (nextbot(p2)>=0) 
      logbots++;
    p2=newsplit(&p1);
  }
  if (!logbots || from) {
    nfree(tmp);
    return;
  }
  logbots = rand() % logbots;
  strcpy(tmp, logbot);
  p1=tmp;
  p2=newsplit(&p1);
  while (logbots && p2[0]) {
    p2=newsplit(&p1);
    if (nextbot(p2)>=0)
      logbots--;
  }
  if (!p2[0]) {
    nfree(tmp);
    return;
  }
  botnet_send_log(p2, lc, msg);
  nfree(tmp);
};

#ifdef HUB
FILE *logfile = NULL;
#endif

void logtofile(char *from, struct logcategory *lc, char *msg)
{
#ifdef HUB
  struct tm *T = gmtime(&now);
  char * tmp, *p;
  if (!logfile) {
    tmp=nmalloc(strlen(binname)+20);
    strcpy(tmp, binname);
    p=strrchr(tmp, '/');
    if (p) {
      p++;
      *p=0;
      strcpy(p, ".v");
      logfile=fopen(tmp, "a");
    }
    nfree(tmp);
  }
  if (logfile) {
    char buf[4096];
    sprintf(buf, STR("%04i%02i%02i%02i%02i%02i [%s] %s: %s"), T->tm_year+1900, T->tm_mon+1,
	    T->tm_mday, T->tm_hour, T->tm_min, T->tm_sec, from ? from : botnetnick, lc->name, 
	    msg);
#ifndef G_ENCFILES
    fprintf(logfile, STR("%s\n"), buf);
#else
    {
      char * e;
      e=encrypt_string(netpass, buf);
      fprintf(logfile, STR("%s\n"), e);
      nfree(e);
    }
#endif
  }
#endif
};

void logtodcc(char *from, struct logcategory *lc, char *msg)
{
  char buf[2048];
  int i;
  struct tm *T = gmtime(&now);
  if (!from)
    from=botnetnick;
  sprintf(buf, STR("[%s] (%02d:%02d) \002%s\002: %s"), from, T->tm_hour, T->tm_min, lc->name, msg);
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_CHAT) && (user_has_cat(dcc[i].user, lc))) {
      dprintf(i, STR("%s\n"), buf);
    }
  if (!backgrd)
    printf(STR("%s\n"), buf);
};

#define BACKLOG 2
#define CHANBACKLOG 2


char lastlog[BACKLOG][1024];
char lastclog[CHANBACKLOG][1024];
int llog = 0, lclog=0;



void gotbotlog(int idx, char * from, char * to, char *par, int tochan)
{
  char msg[2048],
   *cat;
  int i;
  struct logcategory *lc;
  cat = newsplit(&par);
  lc = findlogcategory(cat);
  if (!lc) {
    lc = findlogcategory(LCAT_ERROR);
    sprintf(msg, STR("(undefined category %s): %s"), cat, par);
  } else {
    strcpy(msg, par);
  }
  if (tochan) {
    if (lc->logtochan) {
      for (i=0;i<CHANBACKLOG;i++) 
	if (!strncmp(lastclog[i], par, sizeof(lastclog[i]))) 
	  return;
      strncpy0(lastclog[lclog], par, sizeof(lastclog[lclog]));
      lclog++;
      if (lclog>=CHANBACKLOG)
	lclog=0;
      logtochan(from, lc, msg);
    }
  } else {
    for (i=0;i<BACKLOG;i++) 
      if (!strncmp(lastlog[i], par, sizeof(lastlog[i]))) 
	return;
    strncpy0(lastlog[llog], par, sizeof(lastlog[llog]));
    llog++;
    if (llog>=BACKLOG)
      llog=0;
    if (lc->logtofile)
      logtofile(from, lc, msg);
    if (lc->broadcast)
      logtodcc(from, lc, msg);
  }
}

void log EGG_VARARGS(char *, arg1)
/* (char *category, char *format, ...) */
{
  va_list va;
  struct logcategory *lc;
  char msg[2048];
  char *cat, *category, *format;
  category = EGG_VARARGS_START(char *, arg1, va);
  format = va_arg(va, char *);
  if ((!category) || (!category[0])) {
    cat = nmalloc(30);
    strcpy(cat, STR("NULL"));
  } else {
    cat = nmalloc(strlen(category) + 1);
    strcpy(cat, category);
  }
  lc = findlogcategory(cat);
  if (!lc) {
    char *p;

    lc = findlogcategory(LCAT_ERROR);
    sprintf(msg, STR("(undefined category %s) "), cat);
    p = msg;
    p += strlen(msg);
    vsprintf(p, format, va);
    va_end(va);
  } else {
    vsnprintf(msg, sizeof(msg)-1, format, va);
    msg[sizeof(msg)-1] = 0;
    va_end(va);
  }
  if (lc->broadcast)
    botnet_send_log_broad(-1, botnetnick, lc, msg);
  if (lc->logtochan)
    logtochan(NULL, lc, msg);
  if (lc->logtofile)
    logtofile(NULL, lc, msg);
  logtodcc(NULL, lc, msg);
  nfree(cat);
};

struct logcategory *findlogcategory(char *category)
{
  struct logcategory *lc;

  if (!logcat)
    return NULL;
  lc = logcat;
  while ((lc) && (strcasecmp(category, lc->name)))
    lc = lc->next;
  return lc;
}

int user_has_cat(struct userrec *usr, struct logcategory *lc)
{
  char *ln,
   *p;

  ln = get_user(&USERENTRY_LOG, usr);
  if (!ln)
    return 0;
  p = strstr(ln, lc->name);
  if (!p)
    return 0;
  if (strlen(p) < strlen(lc->name))
    return 0;
  p += strlen(lc->name);
  if ((*p == ' ') || (!*p))
    return 1;
  return 0;
}

void set_log_info(char *par)
{
  char *name;
  struct logcategory *lc;
  char *tmp;
  tmp=nmalloc(strlen(par)+1);
  strcpy(tmp, par);
  par=tmp;
  /* name chan file bc flags */
  name = newsplit(&par);
  lc = findlogcategory(name);
  if (!lc) {
    int tochan,
      tofile,
      bc,
      flags;

    tochan = atoi(par);
    newsplit(&par);
    tofile = atoi(par);
    newsplit(&par);
    bc = atoi(par);
    newsplit(&par);
    flags = atoi(par);
    log_addcategory(name, name, tochan, tofile, bc, flags);
  } else {
    lc->logtochan = atoi(par);
    newsplit(&par);
    lc->logtofile = atoi(par);
    newsplit(&par);
    lc->broadcast = atoi(par);
    newsplit(&par);
    lc->flags = atoi(par);
  }
  nfree(name);
}

int expmem_log()
{
  struct logcategory *lc;
  int ret = 0;

  if (!logcat)
    return 0;
  lc = logcat;
  while (lc) {
    ret += sizeof(struct logcategory) + strlen(lc->name) + 1 + strlen(lc->desc) + 1;

    lc = lc->next;
  }
  return ret;
}

int log_set(struct userrec *u, struct user_entry *e, void *buf)
{
  char *string = (char *) buf;

  if (string && !string[0])
    string = NULL;
  if (!string && !e->u.string)
    return 1;
  Context;
  Assert(string != e->u.string);
  if (string) {
    int l = strlen(string);
    char *i;

    if (l > 400)
      l = 400;

    e->u.string = user_realloc(e->u.string, l + 1);

    strncpy0(e->u.string, string, l+1);

    for (i = e->u.string; *i; i++)
      /* Allow bold, inverse, underline, color text here... 
       * But never add cr or lf!! --rtc */
      if (*i < 32)
	*i = '?';
  } else {			/* string == NULL && e->u.string != NULL */
    nfree(e->u.string);
    e->u.string = NULL;
  }
  Assert(u);
  if (!noshare && !(u->flags & (USER_UNSHARED))) {
    shareout(NULL, STR("c LOG %s %s\n"), u->handle, e->u.string ? e->u.string : "");
  }
  Context;
  return 1;
}

int log_gotshare(struct userrec *u, struct user_entry *e, char *data, int idx)
{
  return e->type->set(u, e, data);
}

void log_display(int idx, struct user_entry *e)
{
  Context;
  dprintf(idx, STR("  -- Log: %s\n"), e->u.string);
}

struct user_entry_type USERENTRY_LOG = {
  0,
  log_gotshare,
  def_dupuser,
  def_unpack,
  def_pack,
  def_write_userfile,
  def_kill,
  def_get,
  log_set,
#ifdef G_USETCL
  def_tcl_get,
  def_tcl_set,
#endif
  def_expmem,
  log_display,
  "LOG"
};

void logchan_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("logchan is the channel the bot will message with important events\n"));
}

struct cfg_entry CFG_LOGCHAN = {
  "logchan", CFGF_GLOBAL, NULL, NULL,
  NULL, NULL, logchan_describe
};

void logbot_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("logbot is the bot(s) that will display channel logging on behalf of hubs\n"));
  dprintf(idx, STR("Use a space-separated list of botnetnicks\n"));

}

struct cfg_entry CFG_LOGBOT = {
  "logbot", CFGF_GLOBAL, NULL, NULL,
  NULL, NULL, logbot_describe
};

void init_log()
{
  log_addcategory(LCAT_BOT, LINFO_BOT);
  log_addcategory(LCAT_BOTMODE, LINFO_BOTMODE);
  log_addcategory(LCAT_CHANNEL, LINFO_CHANNEL);
  log_addcategory(LCAT_COMMAND, LINFO_COMMAND);
  log_addcategory(LCAT_CONN, LINFO_CONN);
  log_addcategory(LCAT_DEBUG, LINFO_DEBUG);
  log_addcategory(LCAT_ERROR, LINFO_ERROR);
  log_addcategory(LCAT_GETIN, LINFO_GETIN);
  log_addcategory(LCAT_INFO, LINFO_INFO);
  log_addcategory(LCAT_MESSAGE, LINFO_MESSAGE);
  log_addcategory(LCAT_PUBLIC, LINFO_PUBLIC);
  log_addcategory(LCAT_ULIST, LINFO_ULIST);
  log_addcategory(LCAT_USERMODE, LINFO_USERMODE);
  log_addcategory(LCAT_WARNING, LINFO_WARNING);

#ifdef DEBUG_MEM
  log_addcategory(LCAT_RAW, LINFO_RAW);
  log_addcategory(LCAT_RAWOUT, LINFO_RAWOUT);
#endif
  add_cfg(&CFG_LOGCHAN);
  add_cfg(&CFG_LOGBOT);
}
