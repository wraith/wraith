/*
 * conf.c -- handles:
 * 
 * all of the conf handling
 */


#include "common.h"
#include "conf.h"
#include "shell.h"
#include "binary.h"
#include "debug.h"
#include "chanprog.h"
#include "crypt.h"
#include "main.h"
#include "settings.h"
#include "misc.h"
#include "users.h"
#include "misc_file.h"
#include "socket.h"
#include "userrec.h"
#include <errno.h>
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif /* HAVE_PATHS_H */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef CYGWIN_HACKS
char cfile[DIRMAX] = "";
#endif /* CYGWIN_HACKS */
conf_t conf;                    /* global conf struct */

static void
tellconf()
{
  conf_bot *bot = NULL;
  int i = 0;

  sdprintf("tempdir: %s\n", replace(tempdir, conf.homedir, "~"));
  sdprintf("uid: %d\n", conf.uid);
  sdprintf("uname: %s\n", conf.uname);
  sdprintf("homedir: %s\n", conf.homedir);
  sdprintf("binpath: %s\n", replace(conf.binpath, conf.homedir, "~"));
  sdprintf("binname: %s\n", conf.binname);
  sdprintf("portmin: %d\n", conf.portmin);
  sdprintf("portmax: %d\n", conf.portmax);
  sdprintf("pscloak: %d\n", conf.pscloak);
  sdprintf("autocron: %d\n", conf.autocron);
  sdprintf("autouname: %d\n", conf.autouname);
  sdprintf("watcher: %d\n", conf.watcher);
  sdprintf("bots:\n");
  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    i++;
    sdprintf("%d: %s%s IP: %s HOST: %s IP6: %s HOST6: %s v6: %d HUB: %d PID: %d\n", i,
             bot->disabled ? "/" : "",
             bot->nick,
             bot->net.ip ? bot->net.ip : "",
             bot->net.host ? bot->net.host : "", bot->net.ip6 ? bot->net.ip6 : "", bot->net.host6 ? bot->net.host6 : "", 
             bot->net.v6,
             bot->hub,
             bot->pid);
  }
  if (conf.bot && ((bot = conf.bot))) {
    sdprintf("me:\n");
    sdprintf("%s%s IP: %s HOST: %s IP6: %s HOST6: %s v6: %d HUB: %d PID: %d\n",
             bot->disabled ? "/" : "",
             bot->nick,
             bot->net.ip ? bot->net.ip : "",
             bot->net.host ? bot->net.host : "", bot->net.ip6 ? bot->net.ip6 : "", bot->net.host6 ? bot->net.host6 : "", 
             bot->net.v6,
             bot->hub,
             bot->pid);
  }
}

void spawnbot(const char *nick)
{
  size_t size = strlen(nick) + strlen(binname) + 20;
  char *run = (char *) my_calloc(1, size);
  int status = 0;

  simple_snprintf(run, size, "%s -B %s", binname, nick);
  sdprintf("Spawning '%s': %s", nick, run);
  status = system(run);
  if (status == -1 || WEXITSTATUS(status))
    sdprintf("Failed to spawn '%s': %s", nick, strerror(errno));
  free(run);
}

/* spawn and kill bots accordingly
 * bots prefixxed with '/' will be killed auto if running.
 * if (updating) then we were called with -L and -P, we need to restart all running bots except our parent and localhub */
void
spawnbots()
{
  conf_bot *bot = NULL;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    sdprintf("checking bot: %s", bot->nick);

    if (bot->disabled) {
      /* kill it if running */
      if (bot->pid)
        kill(bot->pid, SIGKILL);
      else
        continue;
    /* if we're updating automatically, we were called with -L -P, and are only supposed to kill non-localhubs
      -if updating and we find our nick, skip
      -if pid exists and not updating, bot is running and we have nothing more to do, skip.
     */
    } else if ((!strcmp(bot->nick, conf.bot->nick) && updating == UPDATE_AUTO) || (bot->pid && !updating)) {
      sdprintf(" ... skipping. Updating: %d, pid: %d", updating, bot->pid);
      continue;
    } else {
      /* if we are updating with -L -P, then we need to restart ALL bots */
      if (updating == UPDATE_AUTO && bot->pid) {
        kill(bot->pid, SIGKILL);
        /* remove the pid incase we start the new bot before the old dies */
        unlink(bot->pid_file);
      }

      spawnbot(bot->nick);
    }
  }
}

int
killbot(char *botnick, int signal)
{
  conf_bot *bot = NULL;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    if (!egg_strcasecmp(botnick, bot->nick)) {
      if (bot->pid)
        return kill(bot->pid, signal);
    }
  }
  return -1;
}

#ifndef CYGWIN_HACKS
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
confedit()
{
  Tempfile tmpconf = Tempfile("conf");
  char *editor = NULL;
  mode_t um;
  int waiter;
  pid_t pid, xpid, localhub_pid = 0;


  um = umask(077);

  writeconf(NULL, tmpconf.f, CONF_COMMENT);
  fclose(tmpconf.f);
  (void) umask(um);

  if (!can_stat(tmpconf.file))
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

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGCONT, SIG_DFL);

  swap_uids();

  switch (pid = fork()) {
    case -1:
      fatal("Cannot fork", 0);
    case 0:
    {
      char *run = NULL;
      size_t size = tmpconf.len + strlen(editor) + 5;

      setgid(getgid());
      setuid(getuid());
      run = (char *) my_calloc(1, size);
      /* child */
      simple_snprintf(run, size, "%s %s", editor, tmpconf.file);
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

  if (!can_stat(tmpconf.file))
    fatal("Error reading new config file", 0);

  if (conf.bots && conf.bots->pid)
    localhub_pid = conf.bots->pid;

  readconf((const char *) tmpconf.file, 0);               /* read cleartext conf tmp into &settings */
  fix_tilde(&conf.binpath);
  unlink(tmpconf.file);
  conf_to_bin(&conf, 0, -1);
  if (localhub_pid)
    kill(localhub_pid, SIGUSR1);
  exit(0);

fatal:
  unlink(tmpconf.file);
  exit(1);
}
#endif /* !CYGWIN_HACKS */

void
init_conf()
{
//  conf.bots = (conf_bot *) my_calloc(1, sizeof(conf_bot));
//  conf.bots->nick = NULL;
//  conf.bots->next = NULL;
  conf.bots = NULL;
  conf.bot = NULL;

  conf.watcher = 0;
#ifdef CYGWIN_HACKS
  conf.autocron = 0;
#else
  conf.autocron = 1;
#endif /* !CYGWIN_HACKS */
  conf.autouname = 0;
#ifdef CYGWIN_HACKS
  conf.binpath = strdup(homedir());
#else /* !CYGWIN_HACKS */
  conf.binpath = strdup(dirname(binname));
#endif /* CYGWIN_HACKS */
  char *p = strrchr(binname, '/');

  p++;
  conf.binname = strdup(p);

  conf.portmin = 0;
  conf.portmax = 0;
  conf.pscloak = 0;
  conf.uid = -1;
  conf.uname = NULL;
  conf.username = NULL;
  conf.homedir = NULL;
}

void conf_checkpids()
{
  conf_bot *bot = NULL;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next)
    bot->pid = checkpid(bot->nick, bot);
}

/*
 * Return the PID of a bot if it is running, otherwise return 0
 */

pid_t
checkpid(const char *nick, conf_bot *bot)
{
  FILE *f = NULL;
  char buf[DIRMAX] = "", s[11] = "", *tmpnick = NULL, *tmp_ptr = NULL;

  tmpnick = tmp_ptr = strdup(nick);

  simple_snprintf(buf, sizeof buf, "%s.pid.%s", tempdir, tmpnick);
  free(tmp_ptr);

  if (bot && !(bot->pid_file))
    bot->pid_file = strdup(buf);
  else if (bot && strcmp(bot->pid_file, buf))
    str_redup(&bot->pid_file, buf);

  if ((f = fopen(buf, "r"))) {
    pid_t xx = 0;

    fgets(s, 10, f);
    remove_crlf(s);
    fclose(f);
    xx = atoi(s);
//    if (bot) {
      int x = 0;

      x = kill(xx, SIGCHLD);
      if (x == -1 && errno == ESRCH)
        return 0;
      else if (x == 0)
        return xx;
//    } else {
//      return xx ? xx : 0;
//    }
  }

  return 0;
}

void
conf_addbot(char *nick, char *ip, char *host, char *ip6)
{
  conf_bot *bot = (conf_bot *) my_calloc(1, sizeof(conf_bot));

  bot->next = NULL;
  bot->pid_file = NULL;
  if (nick[0] == '/') {
    bot->disabled = 1;
    nick++;
    sdprintf("%s is disabled.", nick);
  }
  bot->nick = strdup(nick);
  bot->net.ip = NULL;
  bot->net.host = NULL;
  bot->net.ip6 = NULL;
  bot->net.host6 = NULL;

  if (host && host[0] == '+') {
    host++;
    bot->net.host6 = strdup(host);
  } else if (host && strcmp(host, ".")) {
    bot->net.host = strdup(host);
  }

  if (ip && strcmp(ip, ".") && is_dotted_ip(ip) == AF_INET)
    bot->net.ip = strdup(ip);
  if (ip6 && strcmp(ip6, ".") && is_dotted_ip(ip6) == AF_INET6)
    bot->net.ip6 = strdup(ip6);

  if (bot->net.ip6 || bot->net.host6)
    bot->net.v6 = 1;

  bot->u = NULL;
//  bot->pid = checkpid(nick, bot);

  if (settings.hubs) {
    char *p = settings.hubs, *p2 = NULL;
    size_t len = 0;

    while (p && *p) {
      if ((p2 = strchr(p, ' '))) {
        len = p2 - p;
        if (!egg_strncasecmp(p, bot->nick, len)) {
          bot->hub = 1;
          break;
        }
      }
      if ((p = strchr(p, ',')))
        p++;
    }
  }

  /* not a hub 
   AND
   * no bots added yet (first bot) yet, not disabled.
   OR
   * bots already listed but we dont have a localhub yet, so we're it!
   */
  if (!bot->hub && ((!conf.bots && !bot->disabled) || (conf.bots && !conf.localhub))) {
    bot->localhub = 1;          /* first bot */
    conf.localhub = strdup(bot->nick);
  }

  list_append((struct list_type **) &(conf.bots), (struct list_type *) bot);
}

void
free_bot(conf_bot *bot)
{
  if (bot) {
    list_delete((struct list_type **) &(conf.bots), (struct list_type *) bot);

    free(bot->nick);
    free(bot->pid_file);
    if (bot->net.ip)
      free(bot->net.ip);
    if (bot->net.host)
      free(bot->net.host);
    if (bot->net.ip6)
      free(bot->net.ip6);
    if (bot->net.host6)
      free(bot->net.host6);
    free(bot);
  }
}

int
conf_delbot(char *botn)
{
  conf_bot *bot = NULL;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    if (!strcmp(bot->nick, botn)) {     /* found it! */
      bot->pid = checkpid(bot->nick, bot);
      killbot(bot->nick, SIGKILL);
      free_bot(bot);
      return 0;
    }
  }
  return 1;
}

void
free_conf()
{
  free_conf_bots();
  free_bot(conf.bot);
  conf.bot = NULL;
  free(conf.localhub);
  free(conf.uname);
  free(conf.username);
  free(conf.homedir);
  free(conf.binname);
  free(conf.binpath);
}

void
free_conf_bots(void)
{
  conf_bot *bot = NULL, *bot_n = NULL;

  for (bot = conf.bots; bot; bot = bot_n) {
    bot_n = bot->next;
    free_bot(bot);
  }
  conf.bots = NULL;
}

int
parseconf(bool error)
{
  if (conf.username) {
    str_redup(&conf.username, my_username());
  } else {
    conf.username = strdup(my_username());
  }

#ifndef CYGWIN_HACKS
  if (error && conf.uid != (signed) myuid) {
    sdprintf("wrong uid, conf: %d :: %d", conf.uid, myuid);
    werr(ERR_WRONGUID);
  } else if (!conf.uid)
    conf.uid = myuid;

  if (conf.uname && strcmp(conf.uname, my_uname()) && !conf.autouname) {
    baduname(conf.uname, my_uname());       /* its not auto, and its not RIGHT, bail out. */
    sdprintf("wrong uname, conf: %s :: %s", conf.uname, my_uname());
    if (error)
      werr(ERR_WRONGUNAME);
  } else if (conf.uname && conf.autouname) {    /* if autouname, dont bother comparing, just set uname to output */
    str_redup(&conf.uname, my_uname());
  } else if (!conf.uname) { /* if not set, then just set it, wont happen again next time... */
    conf.uname = strdup(my_uname());
  }

  if (conf.homedir) {
    str_redup(&conf.homedir, homedir());
  } else {
    conf.homedir = strdup(homedir());
  }

#endif /* !CYGWIN_HACKS */
  return 0;
}

int
readconf(const char *fname, int bits)
{
  FILE *f = NULL;
  int i = 0, enc = (bits & CONF_ENC) ? 1 : 0;
  char *inbuf = NULL;

  sdprintf("readconf(%s, %d)", fname, enc);
  Context;
  if (!(f = fopen(fname, "r")))
    fatal("Cannot read config", 0);

  free_conf_bots();
  inbuf = (char *) my_calloc(1, 201);
  while (fgets(inbuf, 201, f) != NULL) {
    char *line = NULL, *temp_ptr = NULL;

    remove_crlf(inbuf);
    if (enc)
      line = temp_ptr = decrypt_string(settings.salt1, inbuf);
    else
      line = inbuf;

    if ((line && !line[0]) || line[0] == '\n') {
      if (enc)
        free(line);
      continue;
    }

    i++;

    sdprintf("CONF LINE: %s", line);
// !strchr("_`|}][{*/#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ", line[0])) {
    if (enc && line[0] > '~') {
      sdprintf("line %d, char %c ", i, line[0]);
      fatal("Bad encryption", 0);
    } else {                    /* line is good to parse */
      /* - uid */
      if (line[0] == '-') {
        newsplit(&line);
        if (!conf.uid)
          conf.uid = atoi(line);

        /* + uname */
      } else if (line[0] == '+') {
        newsplit(&line);
        if (!conf.uname)
          conf.uname = strdup(line);

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
            conf.autocron = atoi(line);

        } else if (!strcmp(option, "autouname")) {      /* auto update uname contents? */
          if (egg_isdigit(line[0]))
            conf.autouname = atoi(line);

        } else if (!strcmp(option, "username")) {       /* shell username */
          conf.username = strdup(line);

        } else if (!strcmp(option, "homedir")) {        /* homedir */
          conf.homedir = strdup(line);

        } else if (!strcmp(option, "binpath")) {        /* path that the binary should move to? */
          str_redup(&conf.binpath, line);

        } else if (!strcmp(option, "binname")) {        /* filename of the binary? */
          str_redup(&conf.binname, line);

        } else if (!strcmp(option, "portmin")) {
          if (egg_isdigit(line[0]))
            conf.portmin = atoi(line);

        } else if (!strcmp(option, "portmax")) {
          if (egg_isdigit(line[0]))
            conf.portmax = atoi(line);

        } else if (!strcmp(option, "pscloak")) {        /* should bots on this shell pscloak? */
          if (egg_isdigit(line[0]))
            conf.pscloak = atoi(line);

        } else if (!strcmp(option, "uid")) {    /* new method uid */
          if (egg_isdigit(line[0]))
            conf.uid = atoi(line);

        } else if (!strcmp(option, "uname")) {  /* new method uname */
          if (!conf.uname)
            conf.uname = strdup(line);
          else
            str_redup(&conf.uname, line);

        } else if (!strcmp(option, "watcher")) {
          if (egg_isdigit(line[0]))
            conf.watcher = atoi(line);

        } else {
          putlog(LOG_MISC, "*", "Unrecognized config option '%s'", option);

        }
        /* read in portmin */
      } else if (line[0] == '>') {
        newsplit(&line);
        conf.portmin = atoi(line);

      } else if (line[0] == '<') {
        newsplit(&line);
        conf.portmax = atoi(line);

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
    inbuf[0] = 0;
    if (enc)
      free(temp_ptr);
  }                             /* while(fgets()) */
  fclose(f);
  free(inbuf);

  return 0;
}

int
writeconf(char *filename, FILE * stream, int bits)
{
  FILE *f = NULL;
  conf_bot *bot = NULL;
  int (*my_write) (FILE *, const char *, ... ) = NULL;
  char *p = NULL;

  if (bits & CONF_ENC)
    my_write = lfprintf;
  else if (!(bits & CONF_ENC))
    my_write = fprintf;

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
  comment("# They are simply comments and are not parsed at all.\n");

  my_write(f, "! uid %d\n", conf.uid);

  if ((bits & CONF_COMMENT) && conf.uid != (signed) myuid)
    my_write(f, "#! uid %d\n\n", myuid);

  if (!conf.uname || (conf.uname && conf.autouname && strcmp(conf.uname, my_uname()))) {
    comment("# Automatic");
    my_write(f, "! uname %s\n", my_uname());
  } else if (conf.uname && !conf.autouname && strcmp(conf.uname, my_uname())) {
    my_write(f, "! uname %s\n", conf.uname);
    comment("# autouname is OFF");
    my_write(f, "#! uname %s\n\n", my_uname());
  } else
    my_write(f, "! uname %s\n", conf.uname);

  comment("");

  my_write(f, "! username %s\n", conf.username ? conf.username : my_username());
  if (conf.username && strcmp(conf.username, my_username()))
    my_write(f, "#! username %s\n", my_username());

  my_write(f, "! homedir %s\n", conf.homedir ? conf.homedir : homedir());
  if (conf.homedir && strcmp(conf.homedir, homedir()))
    my_write(f, "#! homedir %s\n", homedir());

  comment("\n# binpath needs to be full path unless it begins with '~', which uses 'homedir', ie, '~/'");

  if (strstr(conf.binpath, homedir())) {
    p = replace(conf.binpath, homedir(), "~");
    my_write(f, "! binpath %s\n", p);
  } else
    my_write(f, "! binpath %s\n", conf.binpath);

  comment("# binname is relative to binpath, if you change this, you'll need to manually remove the old one from crontab.");
  my_write(f, "! binname %s\n", conf.binname);

  comment("");

  comment("# portmin/max are for incoming connections (DCC) [0 for any]");
  my_write(f, "! portmin %d\n", conf.portmin);
  my_write(f, "! portmax %d\n", conf.portmax);

  comment("");

  comment("# Attempt to \"cloak\" the process name in `ps` for Linux?");
  my_write(f, "! pscloak %d\n", conf.pscloak);

  comment("");

  comment("# Automatically add the bot to crontab?");
  my_write(f, "! autocron %d\n", conf.autocron);

  comment("");

  comment("# Automatically update 'uname' if it changes? (DANGEROUS)");
  my_write(f, "! autouname %d\n", conf.autouname);

  comment("");

  comment("# This will spawn a child process for EACH BOT that will block ALL process hijackers.");
  my_write(f, "! watcher %d\n", conf.watcher);

  comment("");

  comment("# '|' means OR, [] means the enclosed is optional");
  comment("# A '+' in front of HOST means the HOST is ipv6");
  comment("# A '/' in front of BOT will disable that bot.");
  comment("#[/]BOT IP|. [+]HOST|. [IPV6-IP]");
#endif /* CYGWIN_HACKS */
  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    my_write(f, "%s%s %s %s%s %s\n", 
             bot->disabled ? "/" : "", bot->nick,
             bot->net.ip ? bot->net.ip : ".", bot->net.host6 ? "+" : "",
             bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "."), bot->net.ip6 ? bot->net.ip6 : "");
  }

  fflush(f);

  if (!stream)
    fclose(f);
  return 0;
}

static void
conf_bot_dup(conf_bot *dest, conf_bot *src)
{
  if (dest && src) {
    dest->nick = src->nick ? strdup(src->nick) : NULL;
    dest->pid_file = src->pid_file ? strdup(src->pid_file) : NULL;
    dest->net.ip = src->net.ip ? strdup(src->net.ip) : NULL;
    dest->net.host = src->net.host ? strdup(src->net.host) : NULL;
    dest->net.ip6 = src->net.ip6 ? strdup(src->net.ip6) : NULL;
    dest->net.host6 = src->net.host6 ? strdup(src->net.host6) : NULL;
    dest->net.v6 = src->net.v6;
    dest->u = src->u ? src->u : NULL;
    dest->pid = src->pid;
    dest->hub = src->hub;
    dest->localhub = src->localhub;
    dest->disabled = src->disabled;
    dest->next = NULL;
  }
}


void
fill_conf_bot()
{
  if (!conf.bots || !conf.bots->nick)
    return;

  char *mynick = NULL;
  conf_bot *me = NULL;

  if (!used_B && conf.bots && conf.bots->nick) {
    mynick = strdup(conf.bots->nick);
    strlcpy(origbotname, conf.bots->nick, NICKLEN + 1);
  } else
    mynick = strdup(origbotname);

  for (me = conf.bots; me && me->nick; me = me->next)
    if (!egg_strcasecmp(me->nick, mynick))
      break;

  if (!me || (me->nick && egg_strcasecmp(me->nick, mynick)))
    werr(ERR_BADBOT);

  free(mynick);
  /* for future, we may just want to make this a pointer to ->bots if we do an emech style currentbot-> */
  conf.bot = (conf_bot *) my_calloc(1, sizeof(conf_bot));
  conf_bot_dup(conf.bot, me);
}

void
bin_to_conf(void)
{
/* printf("Converting binary data to conf struct\n"); */
  conf.uid = atol(settings.uid);
  conf.username = strdup(settings.username);
  conf.uname = strdup(settings.uname);
  conf.homedir = strdup(settings.homedir);
  conf.binpath = strdup(settings.binpath);
  fix_tilde(&conf.binpath);
  conf.binname = strdup(settings.binname);
  conf.portmin = atol(settings.portmin);
  conf.portmax = atol(settings.portmax);
  conf.autouname = atoi(settings.autouname);
  conf.autocron = atoi(settings.autocron);
  conf.watcher = atoi(settings.watcher);
  conf.pscloak = atoi(settings.pscloak);

  /* BOTS */
  {
    char *p = NULL, *tmp = NULL, *tmpp = NULL;
 
    tmp = tmpp = strdup(settings.bots);
    while ((p = strchr(tmp, ','))) {
      char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL;

      *p++ = 0;
      if (!tmp[0])
        break;
      nick = newsplit(&tmp);
      if (!nick || (nick && !nick[0]))
        werr(ERR_BADCONF);


      if (tmp[0])
        ip = newsplit(&tmp);
      if (tmp[0])
        host = newsplit(&tmp);
      if (tmp[0])
        ipsix = newsplit(&tmp);

      conf_addbot(nick, ip, host, ipsix);
      tmp = p++;
    }
    free(tmpp);
  }

  /* this is temporary until we make tmpdir customizable */
  if (conf.bots && conf.bots->nick && conf.bots->hub)
    simple_snprintf(tempdir, DIRMAX, "%s/tmp/", conf.binpath);
  else
    simple_snprintf(tempdir, DIRMAX, "%s/.ssh/.../", conf.homedir);

#ifdef CYGWIN_HACKS
  simple_snprintf(tempdir, DIRMAX, "tmp/");
#endif /* CYGWIN_HACKS */

  check_tempdir();      /* ensure we can access tmpdir if it changed */
  clear_tmp();          /* clear out the tmp dir, no matter if we are localhub or not */

  conf_checkpids();

  tellconf();
}

void conf_add_userlist_bots()
{
  conf_bot *bot = NULL;
  struct userrec *u = NULL;
  struct bot_addr *bi = NULL;
  char tmp[81] = "", uhost[UHOSTLEN] = "";

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    /* Don't auto-add hubs. */
    if (!bot->hub) {
      u = get_user_by_handle(userlist, bot->nick);
      if (!u) {
        userlist = adduser(userlist, bot->nick, "none", "-", USER_OP, 1);
        u = get_user_by_handle(userlist, bot->nick);

        egg_snprintf(tmp, sizeof(tmp), "%li [internal]", now);
        set_user(&USERENTRY_ADDED, u, tmp);

        bi = (struct bot_addr *) my_calloc(1, sizeof(struct bot_addr));

        bi->address = (char *) my_calloc(1, 1);
        bi->uplink = (char *) my_calloc(1, 1);
        bi->telnet_port = bi->relay_port = 3333;
        bi->hublevel = 999;
        set_user(&USERENTRY_BOTADDR, u, bi);

      }
      if (bot->net.ip) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.ip);
        if (!user_has_host(NULL, u, uhost))
          addhost_by_handle(bot->nick, uhost);

        simple_snprintf(uhost, sizeof(uhost), "*!~%s@%s", bot->nick, bot->net.ip);
        if (!user_has_host(NULL, u, uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.host) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.host);
        if (!user_has_host(NULL, u, uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.host6) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.host6);
        if (!user_has_host(NULL, u, uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.ip6) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.ip6);
        if (!user_has_host(NULL, u, uhost))
          addhost_by_handle(bot->nick, uhost);
      }
    }
  }

}
