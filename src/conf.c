/*
 * conf.c -- handles:
 * 
 * all of the conf handling
 */

#include "common.h"
#include "conf.h"
#include "shell.h"
#include "debug.h"
#include "chanprog.h"
#include "crypt.h"
#include "main.h"
#include "salt.h"
#include "misc.h"
#include "misc_file.h"

#include <errno.h>
#include <paths.h>
#include <sys/types.h>
#ifdef S_CONFEDIT
#  include <sys/wait.h>
#endif /* S_CONFEDIT */
#include <sys/stat.h>
#include <signal.h>

char cfile[DIRMAX] = "";
conf_t conf;                    /* global conf struct */
conf_t conffile;                /* just some config options only avail during loading */

#ifdef LEAF
void
spawnbots()
{
  conf_bot *bot = NULL;

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next) {
    if (bot->nick[0] == '/') {
      /* kill it if running */
      if (bot->pid)
        kill(bot->pid, SIGKILL);
      else
        continue;
    } else if (!strcmp(bot->nick, conf.bot->nick) || (bot->pid && !updating)) {
      continue;
    } else {
      char *run = NULL;
      size_t size = 0;

      if (updating && bot->pid) {
        kill(bot->pid, SIGKILL);
        /* remove the pid incase we start the new bot before the old dies */
        unlink(bot->pid_file);
      }

      size = strlen(bot->nick) + strlen(binname) + 20;
      run = calloc(1, size);
      egg_snprintf(run, size, "%s -B %s", binname, bot->nick);
      sdprintf("Spawning '%s': %s", bot->nick, run);
      system(run);
      free(run);
    }
  }
}

int
killbot(char *botnick)
{
  conf_bot *bot = NULL;

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next) {
    if (bot->nick[0] == '/')
      continue;
    else if (!egg_strcasecmp(botnick, bot->nick)) {
      if (bot->pid)
        return kill(bot->pid, SIGKILL);
    }
  }
  return -1;
}
#endif /* LEAF */

#ifdef S_CONFEDIT
static uid_t save_euid, save_egid;
static int
swap_uids()
{
  save_euid = geteuid();
  save_egid = getegid();
  return (setegid(getgid()) || seteuid(getuid()))? -1 : 0;
}
static int
swap_uids_back()
{
  return (setegid(save_egid) || seteuid(save_euid)) ? -1 : 0;
}

void
confedit(char *cfile)
{
  FILE *f = NULL;
  char s[DIRMAX] = "", *editor = NULL;
  int fd;
  mode_t um;
  int waiter;
  pid_t pid, xpid;

  egg_snprintf(s, sizeof s, "%s.conf-XXXXXX", tempdir);
  um = umask(077);

  if ((fd = mkstemp(s)) == -1 || (f = fdopen(fd, "w")) == NULL) {
    fatal("Can't create temp conffile!", 0);
  }

  writeconf(NULL, f, CONF_COMMENT);

  (void) umask(um);

  if (!can_stat(s))
    fatal("Cannot stat tempfile", 0);

  /* Okay, edit the file */

  if ((!((editor = getenv("VISUAL")) && strlen(editor)))
      && (!((editor = getenv("EDITOR")) && strlen(editor)))
    ) {
    editor = "vi";
/*
            #if defined(DEBIAN)
              editor = "/usr/bin/editor";
            #elif defined(_PATH_VI)
              editor = _PATH_VI;
            #else
              editor = "/usr/ucb/vi";
            #endif
*/
  }
  fclose(f);

  (void) signal(SIGINT, SIG_IGN);
  (void) signal(SIGQUIT, SIG_IGN);

  swap_uids();

  switch (pid = fork()) {
    case -1:
      fatal("Cannot fork", 0);
    case 0:
    {
      char *run = NULL;
      size_t size = strlen(s) + strlen(editor) + 5;

      setgid(getgid());
      setuid(getuid());
      run = calloc(1, size);
      /* child */
      egg_snprintf(run, size, "%s %s", editor, s);
      execlp("/bin/sh", "/bin/sh", "-c", run, NULL);
      perror(editor);
      exit(1);
     /*NOTREACHED*/}
    default:
      /* parent */
      break;
  }

  /* parent */
  while (1) {
    xpid = waitpid(pid, &waiter, WUNTRACED);
    if (xpid == -1) {
      fprintf(stderr, "waitpid() failed waiting for PID %d from \"%s\": %s\n", pid, editor, strerror(errno));
    } else if (xpid != pid) {
      fprintf(stderr, "wrong PID (%d != %d) from \"%s\"\n", xpid, pid, editor);
      goto fatal;
    } else if (WIFSTOPPED(waiter)) {
      /* raise(WSTOPSIG(waiter)); Not needed and breaks in job control shell */
    } else if (WIFEXITED(waiter) && WEXITSTATUS(waiter)) {
      fprintf(stderr, "\"%s\" exited with status %d\n", editor, WEXITSTATUS(waiter));
      goto fatal;
    } else if (WIFSIGNALED(waiter)) {
      fprintf(stderr, "\"%s\" killed; signal %d (%score dumped)\n", editor, WTERMSIG(waiter),
#  ifdef CYGWIN_HACKS
              0
#  else
              WCOREDUMP(waiter)
#  endif
       /* CYGWIN_HACKS */
              ? "" : "no ");
      goto fatal;
    } else {
      break;
    }
  }

  (void) signal(SIGINT, SIG_DFL);
  (void) signal(SIGQUIT, SIG_DFL);

  swap_uids_back();

  if (!can_stat(s))
    fatal("Error reading new config file", 0);

  unlink(cfile);
  Encrypt_File(s, cfile);
  unlink(s);
  fatal("New config file saved, restart bot to use", 0);

fatal:
  unlink(s);
  exit(1);
}
#endif /* S_CONFEDIT */

void
init_conf()
{
  conffile.bots = (conf_bot *) calloc(1, sizeof(conf_bot));
  conffile.bots->nick = NULL;
  conffile.bots->next = NULL;
  conffile.bot = NULL;

#ifdef CYGWIN_HACKS
  conffile.autocron = 0;
#else
  conffile.autocron = 1;
#endif /* CYGWIN_HACKS */
  conffile.autouname = 0;
#ifdef CYGWIN_HACKS
  conffile.binpath = strdup(homedir());
#else /* !CYGWIN_HACKS */
#  ifdef LEAF
  conffile.binpath = strdup(STR("~/"));
#  endif /* LEAF */
#  ifdef HUB
  conffile.binpath = strdup(dirname(binname));
#  endif /* HUB */
#endif /* CYGWIN_HACKS */
#ifdef LEAF
  conffile.binname = strdup(STR(".sshrc"));
#endif /* LEAF */
#ifdef HUB
  {
    char *p = NULL;

    p = strrchr(binname, '/');
    p++;
    conffile.binname = strdup(p);
  }
#endif /* HUB */
  conffile.portmin = 0;
  conffile.portmax = 0;
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

pid_t
checkpid(char *nick, conf_bot * bot)
{
  FILE *f = NULL;
  char buf[DIRMAX] = "", s[11] = "", *tmpnick = NULL, *tmp_ptr = NULL;


  tmpnick = tmp_ptr = strdup(nick);

  if (tmpnick[0] == '/')
    tmpnick++;

  egg_snprintf(buf, sizeof buf, "%s.pid.%s", tempdir, tmpnick);
  free(tmp_ptr);

  if (bot && !(bot->pid_file))
    bot->pid_file = strdup(buf);
  else if (bot && strcmp(bot->pid_file, buf))
    str_redup(&bot->pid_file, buf);

  if ((f = fopen(buf, "r"))) {
    pid_t xx = 0;

    fgets(s, 10, f);
    s[10] = 0;
    fclose(f);
    xx = atoi(s);
    if (bot) {
      int x = 0;

      x = kill(xx, SIGCHLD);
      if (x == -1 && errno == ESRCH)
        return 0;
      else if (x == 0)
        return xx;
    } else {
      return xx ? xx : 0;
    }
  }

  return 0;
}

static void
conf_addbot(char *nick, char *ip, char *host, char *ip6)
{
  conf_bot *bot = NULL;

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next) ;

  bot->next = (conf_bot *) calloc(1, sizeof(conf_bot));
  bot->next->next = NULL;
  bot->pid_file = NULL;
  bot->nick = strdup(nick);
#ifdef LEAF
  if (bot == conffile.bots) {
    bot->localhub = 1;          /* first bot */
    conffile.localhub = strdup(nick ? nick : origbotname);
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
  } else if (host && strcmp(host, ".")) {
    bot->host = strdup(host);
  }

  if (ip && strcmp(ip, "."))
    bot->ip = strdup(ip);
  if (ip6 && strcmp(ip, "."))
    bot->ip6 = strdup(ip6);

  bot->u = NULL;
  bot->pid = checkpid(nick, bot);
}

void
free_conf()
{
  conf_bot *bot = NULL, *bot_n = NULL;

  for (bot = conffile.bots; bot; bot = bot_n) {
    bot_n = bot->next;
    free(bot->nick);
    free(bot->pid_file);
    if (bot->ip)
      free(bot->ip);
    if (bot->host)
      free(bot->host);
    if (bot->ip6)
      free(bot->ip6);
    if (bot->host6)
      free(bot->host6);
    /* must also free() anything malloc`d in conf_addbot() */
    free(bot);
  }
  free(conffile.uname);
  free(conffile.username);
  free(conffile.homedir);
  free(conffile.binname);
  free(conffile.binpath);
}

int
parseconf()
{
  if (!conffile.bots->nick && !conffile.bots->next)     /* no bots ! */
    werr(ERR_NOBOTS);

  if (conffile.username) {
    str_redup(&conffile.username, my_username());
  } else {
    conffile.username = strdup(my_username());
  }

#ifndef CYGWIN_HACKS
  if (conffile.uid && conffile.uid != myuid) {
    sdprintf("wrong uid, conf: %d :: %d", conffile.uid, myuid);
    werr(ERR_WRONGUID);
  } else if (!conffile.uid)
    conffile.uid = myuid;

  if (conffile.uname && strcmp(conffile.uname, my_uname()) && !conffile.autouname) {
    baduname(conffile.uname, my_uname());       /* its not auto, and its not RIGHT, bail out. */
    sdprintf("wrong uname, conf: %s :: %s", conffile.uname, my_uname());
    werr(ERR_WRONGUNAME);
  } else if (conffile.uname && conffile.autouname) {    /* if autouname, dont bother comparing, just set uname to output */
    str_redup(&conffile.uname, my_uname());
  } else if (!conffile.uname) { /* if not set, then just set it, wont happen again next time... */
    conffile.uname = strdup(my_uname());
  }

  if (conffile.homedir) {
    str_redup(&conffile.homedir, homedir());
  } else {
    conffile.homedir = strdup(homedir());
  }

  if (strchr(conffile.binpath, '~')) {
    char *p = NULL;

    if (conffile.binpath[strlen(conffile.binpath) - 1] == '/')
      conffile.binpath[strlen(conffile.binpath) - 1] = 0;

    if ((p = replace(conffile.binpath, "~", homedir())))
      str_redup(&conffile.binpath, p);
    else
      fatal("Unforseen error expanding '~'", 0);
  }
#endif /* !CYGWIN_HACKS */
  return 0;
}

int
readconf(char *cfile, int bits)
{
  FILE *f = NULL;
  int i = 0, enc = (bits & CONF_ENC) ? 1 : 0;
  char inbuf[201] = "";

  sdprintf("readconf(%s, %d)", cfile, enc);
  Context;
  if (!(f = fopen(cfile, "r")))
    fatal("Cannot read config", 0);

  while (fgets(inbuf, sizeof inbuf, f) != NULL) {
    char *line = NULL, *temp_ptr = NULL, *p = NULL;

    /* fucking DOS */
    if ((p = strchr(inbuf, '\n')))
      *p = 0;
    if ((p = strchr(inbuf, '\r')))
      *p = 0;

    if (enc)
      line = temp_ptr = decrypt_string(SALT1, inbuf);
    else
      line = inbuf;

    if ((line && !line[0]) || line[0] == '\n') {
      if (enc)
        free(line);
      continue;
    }

    i++;

    sdprintf("CONF LINE: %s", line);
    if (enc && !strchr("*/#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ", line[0])) {
      sdprintf("line %d, char %c ", i, line[0]);
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
        if (line[0])
          option = newsplit(&line);

        if (!option)
          continue;

        if (!strcmp(option, "autocron")) {      /* automatically check/create crontab? */
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

        } else if (!strcmp(option, "portmin")) {
          if (egg_isdigit(line[0]))
            conffile.portmin = atoi(line);

        } else if (!strcmp(option, "portmax")) {
          if (egg_isdigit(line[0]))
            conffile.portmax = atoi(line);

        } else if (!strcmp(option, "pscloak")) {        /* should bots on this shell pscloak? */
          if (egg_isdigit(line[0]))
            conffile.pscloak = atoi(line);

        } else if (!strcmp(option, "uid")) {    /* new method uid */
          if (!conffile.uid && egg_isdigit(line[0]))
            conffile.uid = atoi(line);

        } else if (!strcmp(option, "uname")) {  /* new method uname */
          if (!conffile.uname)
            conffile.uname = strdup(line);

        } else {
          putlog(LOG_MISC, "*", "Unrecognized config option '%s'", option);

        }
        /* read in portmin */
      } else if (line[0] == '>') {
        newsplit(&line);
        conffile.portmin = atoi(line);

      } else if (line[0] == '<') {
        newsplit(&line);
        conffile.portmax = atoi(line);

        /* now to parse nick/hosts */
      } else if (line[0] != '#') {
        char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL;

        nick = newsplit(&line);
        if (!nick || (nick && !nick[0]))
          werr(ERR_BADCONF);

        if (line[0])
          ip = newsplit(&line);
        if (line[0])
          host = newsplit(&line);
        if (line[0])
          ipsix = newsplit(&line);

        conf_addbot(nick, ip, host, ipsix);
      }
    }
    if (enc)
      free(temp_ptr);
  }                             /* while(fgets()) */
  fclose(f);

  return 0;
}

int
writeconf(char *filename, FILE * stream, int bits)
{
  FILE *f = NULL;
  conf_bot *bot = NULL;
  Function my_write = NULL;
  char *p = NULL;

  if (bits & CONF_ENC)
    my_write = (Function) lfprintf;
  else if (!(bits & CONF_ENC))
    my_write = (Function) fprintf;

#define comment(text)			\
	if (bits & CONF_COMMENT)	\
	  my_write(f, "%s\n", text);

  if (stream) {
    f = stream;
  } else if (filename) {
    if (!(f = fopen(filename, "w")))
      return 1;
  }
#ifndef CYGWIN_HACKS
  comment("# Lines beginning with # are what the preceeding line SHOULD be");
  comment("# They are also ignored during parsing\n");

  my_write(f, "! uid %d\n", conffile.uid);

  if ((bits & CONF_COMMENT) && conffile.uid != myuid)
    my_write(f, "#! uid %d\n\n", myuid);

  if (!conffile.uname || (conffile.uname && conffile.autouname && strcmp(conffile.uname, my_uname()))) {
    comment("# Automatic");
    my_write(f, "! uname %s\n", my_uname());
  } else if (conffile.uname && !conffile.autouname && strcmp(conffile.uname, my_uname())) {
    my_write(f, "! uname %s\n", conffile.uname);
    comment("# autouname is OFF");
    my_write(f, "#! uname %s\n\n", my_uname());
  } else
    my_write(f, "! uname %s\n", conffile.uname);

  my_write(f, "! username %s\n", conffile.username ? conffile.username : my_username());
  if (conffile.username && strcmp(conffile.username, my_username()))
    my_write(f, "#! username %s\n", my_username());

  my_write(f, "! homedir %s\n", conffile.homedir ? conffile.homedir : homedir());
  if (conffile.homedir && strcmp(conffile.homedir, homedir()))
    my_write(f, "#! homedir %s\n", homedir());

  comment("\n# binpath needs to be full path unless it begins with '~', which uses 'homedir', ie, '~/'");

  if (strstr(conffile.binpath, homedir())) {
    p = replace(conffile.binpath, homedir(), "~");
    my_write(f, "! binpath %s\n", p);
  } else
    my_write(f, "! binpath %s\n", conffile.binpath);

  comment("# binname is relative to binpath, if you change this, you'll need to manually remove the old one from crontab.");
  my_write(f, "! binname %s\n", conffile.binname);

  comment("");

  comment("# portmin/max are for incoming connections (DCC) [0 for any]");
  my_write(f, "! portmin %d\n", conffile.portmin);
  my_write(f, "! portmax %d\n", conffile.portmax);

  comment("");

  comment("# Attempt to \"cloak\" the process name in `ps` for Linux?");
  my_write(f, "! pscloak %d\n", conffile.pscloak);

  comment("");

  comment("# Automatically add the bot to crontab?");
  my_write(f, "! autocron %d\n", conffile.autocron);

  comment("");

  comment("# Automatically update 'uname' if it changes? (DANGEROUS)");
  my_write(f, "! autouname %d\n", conffile.autouname);

  comment("");

  comment("# '|' means OR, [] means the enclosed is optional");
  comment("# A '+' in front of HOST means the HOST is ipv6");
  comment("# A '/' in front of BOT will disable that bot.");
  comment("#[/]BOT IP|. [+]HOST|. [IPV6-IP]");
#endif /* CYGWIN_HACKS */
  for (bot = conffile.bots; bot && bot->nick; bot = bot->next) {
    my_write(f, "%s %s %s%s %s\n", bot->nick,
             bot->ip ? bot->ip : ".", bot->host6 ? "+" : "", bot->host ? bot->host : (bot->host6 ? bot->host6 : "."), bot->ip6 ? bot->ip6 : "");
  }

  fflush(f);

  if (!stream)
    fclose(f);
  return 0;
}

static void
conf_bot_dup(conf_bot * dest, conf_bot * src)
{
  dest->nick = src->nick ? strdup(src->nick) : NULL;
  dest->pid_file = src->pid_file ? strdup(src->pid_file) : NULL;
  dest->ip = src->ip ? strdup(src->ip) : NULL;
  dest->host = src->host ? strdup(src->host) : NULL;
  dest->ip6 = src->ip6 ? strdup(src->ip6) : NULL;
  dest->host6 = src->host6 ? strdup(src->host6) : NULL;
  dest->u = src->u ? src->u : NULL;
  dest->pid = src->pid;
#ifdef LEAF
  dest->localhub = src->localhub;
#endif /* LEAF */
  dest->next = NULL;
}

void
fillconf(conf_t * inconf)
{
  conf_bot *bot = NULL;
  char *mynick = NULL;

  if (localhub && conffile.bots && conffile.bots->nick) {
    mynick = strdup(conffile.bots->nick);
    strncpyz(origbotname, conffile.bots->nick, NICKLEN + 1);
  } else
    mynick = strdup(origbotname);

  for (bot = conffile.bots; bot && bot->nick; bot = bot->next)
    if (!strcmp(bot->nick, mynick))
      break;

  if (bot->nick && bot->nick[0] == '/')
    werr(ERR_BOTDISABLED);

  if (!bot->nick || (bot->nick && strcmp(bot->nick, mynick)))
    werr(ERR_BADBOT);

  free(mynick);
  inconf->bot = (conf_bot *) calloc(1, sizeof(conf_bot));
  conf_bot_dup(inconf->bot, bot);
  inconf->localhub = conffile.localhub ? strdup(conffile.localhub) : NULL;
  inconf->binpath = conffile.binpath ? strdup(conffile.binpath) : NULL;
  inconf->binname = conffile.binname ? strdup(conffile.binname) : NULL;
  inconf->uname = conffile.uname ? strdup(conffile.uname) : NULL;
  inconf->username = conffile.username ? strdup(conffile.username) : NULL;
  inconf->homedir = conffile.homedir ? strdup(conffile.homedir) : NULL;
  inconf->autocron = conffile.autocron;
  inconf->autouname = conffile.autouname;
  inconf->portmin = conffile.portmin;
  inconf->portmax = conffile.portmax;
  inconf->pscloak = conffile.pscloak;
  inconf->uid = conffile.uid;
}
