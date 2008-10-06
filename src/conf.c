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
#include "src/mod/irc.mod/irc.h"
#include "misc.h"
#include "users.h"
#include "misc_file.h"
#include "socket.h"
#include "botnet.h"
#include "userrec.h"
#include <errno.h>
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif /* HAVE_PATHS_H */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#ifdef CYGWIN_HACKS
char cfile[DIRMAX] = "";
#endif /* CYGWIN_HACKS */
conf_t conf;                    /* global conf struct */

static void
tellconf()
{
  conf_bot *bot = NULL;
  int i = 0;
  sdprintf(STR("tempdir: %s\n"), replace(tempdir, conf.homedir, "~"));
  sdprintf(STR("features: %d\n"), conf.features);
  sdprintf(STR("uid: %d\n"), conf.uid);
  sdprintf(STR("uname: %s\n"), conf.uname);
  sdprintf(STR("homedir: %s\n"), conf.homedir);
  sdprintf(STR("username: %s\n"), conf.username);
  sdprintf(STR("binpath: %s\n"), replace(conf.binpath, conf.homedir, "~"));
  sdprintf(STR("binname: %s\n"), conf.binname);
  sdprintf(STR("datadir: %s\n"), replace(conf.datadir, conf.homedir, "~"));
  sdprintf(STR("portmin: %d\n"), conf.portmin);
  sdprintf(STR("portmax: %d\n"), conf.portmax);
  sdprintf(STR("pscloak: %d\n"), conf.pscloak);
  sdprintf(STR("autocron: %d\n"), conf.autocron);
  sdprintf(STR("autouname: %d\n"), conf.autouname);
  sdprintf(STR("watcher: %d\n"), conf.watcher);
  sdprintf(STR("bots:\n"));
  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    i++;
    sdprintf(STR("%d: %s%s IP: %s HOST: %s IP6: %s HOST6: %s v6: %d HUB: %d PID: %d\n"), i,
             bot->disabled ? "/" : "",
             bot->nick,
             bot->net.ip ? bot->net.ip : "",
             bot->net.host ? bot->net.host : "", bot->net.ip6 ? bot->net.ip6 : "", bot->net.host6 ? bot->net.host6 : "", 
             bot->net.v6,
             bot->hub,
             bot->pid);
  }
  if (conf.bot && ((bot = conf.bot))) {
    sdprintf(STR("me:\n"));
    sdprintf(STR("%s%s IP: %s HOST: %s IP6: %s HOST6: %s v6: %d HUB: %d PID: %d\n"),
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
  size_t size = strlen(shell_escape(nick)) + strlen(shell_escape(binname)) + 20;
  char *run = (char *) my_calloc(1, size);
  int status = 0;

  simple_snprintf(run, size, "%s %s", shell_escape(binname), shell_escape(nick));
  sdprintf("Spawning '%s': %s", nick, run);
  status = system(run);
  if (status == -1 || WEXITSTATUS(status))
    sdprintf("Failed to spawn '%s': %s", nick, strerror(errno));
  free(run);
}

/* spawn and kill bots accordingly
 * bots prefixxed with '/' will be killed auto if running.
 * if (updating) then we were called with -U or -u */
void
spawnbots(conf_bot *bots, bool rehashed)
{
  conf_bot *bot = NULL;

  for (bot = bots; bot && bot->nick; bot = bot->next) {
    sdprintf("checking bot: %s", bot->nick);

    if (bot->disabled) {
      /* kill it if running */
      if (bot->pid) {
        kill(bot->pid, SIGKILL);
        bot->pid = 0;
      } else
        continue;
    /* if we're updating automatically, we were called with -u and are only supposed to kill non-localhubs
      -if updating and we find our nick, skip
      -if pid exists and not updating, bot is running and we have nothing more to do, skip.
     */
    } else if ((conf.bot && !egg_strcasecmp(bot->nick, conf.bot->nick) && 
               (updating == UPDATE_AUTO || rehashed)) || (bot->pid && !updating)) {
      sdprintf(STR(" ... skipping. Updating: %d, pid: %d"), updating, bot->pid);
      continue;
    } else {
      /* if we are updating with -u then we need to restart ALL bots */
      if (updating == UPDATE_AUTO && bot->pid) {
        kill(bot->pid, SIGHUP);
        continue;
      }

      spawnbot(bot->nick);
    }
  }
}

int
conf_killbot(conf_bot *bots, const char *botnick, conf_bot *bot, int signal, bool notbotnick)
{
  int ret = -1;

  if (bot) {
    if (bot->pid)
      ret = kill(bot->pid, signal);
  } else {
    for (bot = bots; bot && bot->nick; bot = bot->next) {
      /* kill all bots but myself if botnick==NULL, otherwise just kill botnick */
      if ((!conf.bot) || 
          (!botnick && 
             (conf.bot->nick && egg_strcasecmp(conf.bot->nick, bot->nick))) || 
          (botnick && 
             ((notbotnick == 0 && !egg_strcasecmp(botnick, bot->nick)) || 
              (notbotnick == 1 && egg_strcasecmp(botnick, bot->nick))
             )  
          ) 
         ) {
        if (bot->pid)
          ret = kill(bot->pid, signal);

        if (botnick && !notbotnick)
          break;
      }
    }
  }
  return ret;
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

static int
my_gettime(struct timespec *ts)
{
    int rval;
#if defined(HAVE_GETTIMEOFDAY) && (defined(HAVE_ST_MTIM) || defined(HAVE_ST_MTIMESPEC))
    struct timeval tv;

    rval = gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#else
    rval = (int)time(&ts->tv_sec);
    ts->tv_nsec = 0;
#endif
    return (rval);
}


void
confedit()
{
  Tempfile tmpconf = Tempfile("conf");
  const char *editor = NULL;
  mode_t um;
  int waiter;
  pid_t pid, xpid;
  struct stat st, sn;
  struct timespec ts1, ts2;           /* time before and after edit */
  bool autowrote = 0;
  conf_bot *oldbots = NULL;

  um = umask(077);

  autowrote = writeconf(NULL, tmpconf.f, CONF_COMMENT);
  fstat(tmpconf.fd, &st);		/* for file modification compares */
//  tmpconf.my_close();

  umask(um);

  if (!can_stat(tmpconf.file))
    fatal(STR("Cannot stat tempfile"), 0);

  /* Okay, edit the file */

  if ((!((editor = getenv(STR("EDITOR"))) && strlen(editor)))
      && (!((editor = getenv(STR("VISUAL"))) && strlen(editor)))
    ) {
    editor = STR("vi");
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

  my_gettime(&ts1);
  switch (pid = fork()) {
    case -1:
      fatal(STR("Cannot fork"), 0);
    case 0:
    {
      char *run = NULL;
      size_t size = tmpconf.len + strlen(editor) + 5;

      setgid(getgid());
      setuid(getuid());
      run = (char *) my_calloc(1, size);
      /* child */
      simple_snprintf(run, size, "%s %s", editor, tmpconf.file);
      execlp("/bin/sh", "/bin/sh", "-c", run, (char*)NULL);
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
    my_gettime(&ts2);
    if (xpid == -1) {
      fprintf(stderr, STR("waitpid() failed waiting for PID %d from \"%s\": %s\n"), pid, editor, strerror(errno));
    } else if (xpid != pid) {
      fprintf(stderr, STR("wrong PID (%d != %d) from \"%s\"\n"), xpid, pid, editor);
      goto fatal;
    } else if (WIFSTOPPED(waiter)) {
      /* raise(WSTOPSIG(waiter)); Not needed and breaks in job control shell */
    } else if (WIFEXITED(waiter) && WEXITSTATUS(waiter)) {
      fprintf(stderr, STR("\"%s\" exited with status %d\n"), editor, WEXITSTATUS(waiter));
      goto fatal;
    } else if (WIFSIGNALED(waiter)) {
      fprintf(stderr, STR("\"%s\" killed; signal %d (%score dumped)\n"), editor, WTERMSIG(waiter),
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


  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);

  swap_uids_back();
  if (fstat(tmpconf.fd, &sn))
    fatal(STR("Error reading new config file"), 0);

  if (!autowrote && st.st_size == sn.st_size &&
      mtim_getsec(st) == mtim_getsec(sn) &&
      mtim_getnsec(st) == mtim_getnsec(sn)) {
    /*
     * If mtime and size match but the user spent no measurable
     * time in the editor we can't tell if the file was changed.
     */
#ifdef HAVE_TIMESPECSUB2
    timespecsub(&ts1, &ts2);
#else
    timespecsub(&ts1, &ts2, &ts2);
#endif
    if (timespecisset(&ts2)) {
      printf(STR("* Config unchanged.\n"));
      exit(0);            
    }
  }

  tmpconf.my_close();

  oldbots = conf_bots_dup(conf.bots);
  free_conf();

  readconf((const char *) tmpconf.file, 0);               /* read cleartext conf tmp into &settings */
  expand_tilde(&conf.binpath);
  expand_tilde(&conf.datadir);
  unlink(tmpconf.file);
  conf_to_bin(&conf, 0, -1);

  /* Now signal all of the old bots with SIGUSR1
   * They will auto die and determine new localhub, etc..
   */
  conf_checkpids(oldbots);
  conf_killbot(oldbots, NULL, NULL, SIGUSR1);

  /* Now spawn new bots */
//  spawnbots(conf.bots);

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

  conf.localhub = NULL;
  conf.watcher = 0;
#ifdef CYGWIN_HACKS
  conf.autocron = 0;
#else
  conf.autocron = 1;
#endif /* !CYGWIN_HACKS */
  conf.autouname = 0;
#ifdef CYGWIN_HACKS
  if (homedir())
    conf.binpath = strdup(homedir());
#else /* !CYGWIN_HACKS */
  conf.binpath = strdup(dirname(binname));
#endif /* CYGWIN_HACKS */
  char *p = strrchr(binname, '/');

  p++;

  if (!strncmp(p, STR("wraith."), 7) && strchr(p, '-'))
    conf.binname = strdup(STR("wraith"));
  else
    conf.binname = strdup(p);

  conf.features = 0;
  conf.portmin = 0;
  conf.portmax = 0;
  conf.pscloak = 0;
  conf.uid = -1;
  conf.uname = NULL;
  conf.username = NULL;
  conf.homedir = NULL;
  conf.datadir = strdup(STR("./..."));
  expand_tilde(&conf.datadir);
}

void conf_checkpids(conf_bot *bots, bool all)
{
  conf_bot *bot = NULL;

  for (bot = bots; bot && bot->nick; bot = bot->next)
    if (all || (!all && bot->pid == 0))
      bot->pid = checkpid(bot->nick, bot);
}

/*
 * Return the PID of a bot if it is running, otherwise return 0
 */

pid_t
checkpid(const char *nick, conf_bot *bot)
{
  FILE *f = NULL;
  char buf[DIRMAX] = "", *tmpnick = NULL, *tmp_ptr = NULL;
  pid_t pid = 0;

  tmpnick = tmp_ptr = strdup(nick);

  strtolower(tmpnick);

  simple_snprintf(buf, sizeof buf, STR("%s/.pid.%s"), conf.datadir, tmpnick);
  free(tmp_ptr);

  if (bot && !(bot->pid_file))
    bot->pid_file = strdup(buf);
  else if (bot && egg_strcasecmp(bot->pid_file, buf))
    str_redup(&bot->pid_file, buf);

  if ((f = fopen(buf, "r"))) {
    char *bufp = NULL, *pids = NULL;

    fgets(buf, sizeof(buf), f);
    fclose(f);
    remove_crlf(buf);

    if (!buf[0])
      return 0;
  
    bufp = buf;
    pids = newsplit(&bufp);

    if (str_isdigit(pids)) {
      pid = atoi(pids);

      if (kill(pid, SIGCHLD))	//Problem killing, most likely it's just not running.
        pid = 0;
    }


    if (bufp[0] && pid && can_stat(bufp) && (getpid() == pid) &&
        !egg_strncasecmp(nick, origbotnick, HANDLEN)) {
      socksfile = strdup(bufp);
      return 0;
    }
  }

  return pid;
}

void
conf_addbot(char *nick, char *ip, char *host, char *ip6)
{
  conf_bot *bot = (conf_bot *) my_calloc(1, sizeof(conf_bot));

  bot->next = NULL;
  bot->pid_file = NULL;
  if (nick[0] == '/') {
    bot->disabled = 1;
    ++nick;
    sdprintf(STR("%s is disabled."), nick);
  }
  bot->nick = strldup(nick, HANDLEN);
  bot->net.ip = NULL;
  bot->net.host = NULL;
  bot->net.ip6 = NULL;
  bot->net.host6 = NULL;

  if (host && host[0] == '+') {
    ++host;
    bot->net.host6 = strdup(host);
  } else if (host && !strchr(".*", host[0])) {
    bot->net.host = strdup(host);
  }

  if (ip && !strchr(".*", ip[0])) {
    int aftype = is_dotted_ip(ip);

    if (aftype == AF_INET)
      bot->net.ip = strdup(ip);
#ifdef USE_IPV6
    else if (aftype == AF_INET6)
      bot->net.ip6 = strdup(ip);
#endif /* USE_IPV6 */
    else if (aftype == 0) { /* The idiot used a hostname in place of ip */
      if (bot->net.host)
        free(bot->net.host);
      bot->net.host = strdup(ip);
    }
  }

#ifdef USE_IPV6
  if (ip6 && !strchr(".*", ip6[0]) && is_dotted_ip(ip6) == AF_INET6)
    bot->net.ip6 = strdup(ip6);

  if (bot->net.ip6 || bot->net.host6)
    bot->net.v6 = 1;
#endif /* USE_IPV6 */

  if (userlist)
    bot->u = get_user_by_handle(userlist, nick);
  else
    bot->u = NULL;

//  bot->pid = checkpid(nick, bot);

  if (settings.hubs && is_hub(bot->nick))
    bot->hub = 1;

  /* not a hub 
   AND
    * no bots added yet (first bot) yet, not disabled.
    OR
    * bots already listed but we dont have a localhub yet, so we're it!
   */
  if (!conf.localhub && !bot->hub && !bot->disabled) {
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
conf_delbot(char *botn, bool kill)
{
  conf_bot *bot = NULL;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    if (!egg_strcasecmp(bot->nick, botn)) {     /* found it! */
      if (kill) {
        bot->pid = checkpid(bot->nick, bot);
        conf_killbot(conf.bots, NULL, bot, SIGKILL);
      }
      free_bot(bot);
      return 0;
    }
  }
  return 1;
}

void
free_conf()
{
  free_conf_bots(conf.bots);
  free_bot(conf.bot);
  conf.bot = NULL;
  if (conf.localhub)
    free(conf.localhub);
  if (conf.uname)
    free(conf.uname);
  if (conf.username)
    free(conf.username);
  if (conf.datadir)
    free(conf.datadir);
  if (conf.homedir)
    free(conf.homedir);
  if (conf.binname)
   free(conf.binname);
  if (conf.binpath)
    free(conf.binpath);
  init_conf();
}

void
free_conf_bots(conf_bot *list)
{
  if (list) {
    conf_bot *bot = NULL, *bot_n = NULL;

    for (bot = list; bot; bot = bot_n) {
      bot_n = bot->next;
      free_bot(bot);
    }
    list = NULL;
  }
}

void prep_homedir(bool error)
{
  if (!conf.homedir)
    str_redup(&conf.homedir, homedir());

  if (error && (!conf.homedir || !conf.homedir[0]))
    werr(ERR_NOHOMEDIR);
}

int
parseconf(bool error)
{
  if (error && conf.uid == -1 && !conf.uname)
    werr(ERR_NOTINIT);

  if (!conf.username)
    str_redup(&conf.username, my_username());

  if (error && (!conf.username || !conf.username[0]))
    werr(ERR_NOUSERNAME);

#ifndef CYGWIN_HACKS
  if (error && conf.uid != (signed) myuid) {
    sdprintf(STR("wrong uid, conf: %d :: %d"), conf.uid, myuid);
    werr(ERR_WRONGUID);
  } else if (!conf.uid)
    conf.uid = myuid;

  if (conf.uname && strcmp(conf.uname, my_uname()) && !conf.autouname) {
    baduname(conf.uname, my_uname());       /* its not auto, and its not RIGHT, bail out. */
    sdprintf(("wrong uname, conf: %s :: %s"), conf.uname, my_uname());
    if (error)
      werr(ERR_WRONGUNAME);
  } else if (conf.uname && conf.autouname) {    /* if autouname, dont bother comparing, just set uname to output */
    str_redup(&conf.uname, my_uname());
  } else if (!conf.uname) { /* if not set, then just set it, wont happen again next time... */
    conf.uname = strdup(my_uname());
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

  sdprintf(STR("readconf(%s, %d)"), fname, enc);
  Context;
  if (!(f = fopen(fname, "r")))
    fatal(STR("Cannot read config"), 0);

  free_conf_bots(conf.bots);
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

    rmspace(line);

    sdprintf(STR("CONF LINE: %s"), line);
// !strchr("_`|}][{*/#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ", line[0])) {
    if (enc && line[0] > '~') {
      sdprintf(STR("line %d, char %c "), i, line[0]);
      fatal(STR("Bad encryption"), 0);
    } else {                    /* line is good to parse */
      /* - uid */
      if (line[0] == '-') {
        newsplit(&line);
        if (conf.uid == -1)
          conf.uid = atoi(line);

        /* + uname */
      } else if (line[0] == '+') {
        newsplit(&line);
        if (!conf.uname)
          conf.uname = strdup(line);

        /* ! is misc options */
      } else if (line[0] == '!') {
        char *option = NULL;

        /* Only newplit if they did '! option' */
        if (line[1] == ' ')
          newsplit(&line);
        else
          ++line;

        if (line[0])
          option = newsplit(&line);

        if (!option || !line[0])
          continue;

        if (!egg_strcasecmp(option, STR("autocron"))) {      /* automatically check/create crontab? */
          if (egg_isdigit(line[0]))
            conf.autocron = atoi(line);

        } else if (!egg_strcasecmp(option, STR("autouname"))) {      /* auto update uname contents? */
          if (egg_isdigit(line[0]))
            conf.autouname = atoi(line);

        } else if (!egg_strcasecmp(option, STR("username"))) {       /* shell username */
          str_redup(&conf.username, line);

        } else if (!egg_strcasecmp(option, STR("homedir"))) {        /* homedir */
          str_redup(&conf.homedir, line);

        } else if (!egg_strcasecmp(option, STR("datadir"))) {        /* datadir */
          str_redup(&conf.datadir, line);

        } else if (!egg_strcasecmp(option, STR("binpath"))) {        /* path that the binary should move to? */
          str_redup(&conf.binpath, line);

        } else if (!egg_strcasecmp(option, STR("binname"))) {        /* filename of the binary? */
          str_redup(&conf.binname, line);

        } else if (!egg_strcasecmp(option, STR("portmin"))) {
          if (egg_isdigit(line[0]))
            conf.portmin = atoi(line);

        } else if (!egg_strcasecmp(option, STR("portmax"))) {
          if (egg_isdigit(line[0]))
            conf.portmax = atoi(line);

        } else if (!egg_strcasecmp(option, STR("pscloak"))) {        /* should bots on this shell pscloak? */
          if (egg_isdigit(line[0]))
            conf.pscloak = atoi(line);

        } else if (!egg_strcasecmp(option, STR("uid"))) {    /* new method uid */
          if (str_isdigit(line))
            conf.uid = atoi(line);

        } else if (!egg_strcasecmp(option, STR("uname"))) {  /* new method uname */
          str_redup(&conf.uname, line);

        } else if (!egg_strcasecmp(option, STR("watcher"))) {
          if (egg_isdigit(line[0]))
            conf.watcher = atoi(line);

        } else {
          putlog(LOG_MISC, "*", STR("Unrecognized config option '%s'"), option);

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
  int autowrote = 0;

  if (bits & CONF_ENC)
    my_write = lfprintf;
  else if (!(bits & CONF_ENC))
    my_write = fprintf;

#define comment(text)	do {		\
	if (bits & CONF_COMMENT)	\
	  my_write(f, STR("%s\n"), text);	\
} while(0)

  if (stream) {
    f = stream;
  } else if (filename) {
    if (!(f = fopen(filename, "w")))
      return 1;
  }
#ifndef CYGWIN_HACKS
  char *p = NULL;

  comment("");
  comment("# Lines beginning with # are what the preceeding line SHOULD be");
  comment("# They are simply comments and are not parsed at all.\n");

#define conf_com() do {							\
	if (do_confedit == CONF_AUTO) {					\
	  comment("# Automatically updated with -C");			\
	  autowrote = 1;						\
        } else								\
	  comment("#The correct line follows. (Not auto due to -c)");	\
} while(0)

  if ((bits & CONF_COMMENT) && conf.uid != (signed) myuid) {
    conf_com();
    my_write(f, STR("%s! uid %d\n"), do_confedit == CONF_AUTO ? "" : "#", myuid);
    my_write(f, STR("%s! uid %d\n"), do_confedit == CONF_STATIC ? "" : "#", conf.uid);
  } else
    my_write(f, STR("! uid %d\n"), conf.uid);

  if (!conf.uname || (conf.uname && conf.autouname && strcmp(conf.uname, my_uname()))) {
    autowrote = 1;
    if (conf.uname)
      comment("# autouname is ON");
    else
      comment("# Automatically updated empty uname");

    my_write(f, STR("! uname %s\n"), my_uname());
    if (conf.uname)
      my_write(f, STR("#! uname %s\n"), conf.uname);
  } else if (conf.uname && !conf.autouname && strcmp(conf.uname, my_uname())) {
    conf_com();
    my_write(f, STR("%s! uname %s\n"), do_confedit == CONF_AUTO ? "" : "#", my_uname());
    my_write(f, STR("%s! uname %s\n"), do_confedit == CONF_STATIC ? "" : "#", conf.uname);
  } else
    my_write(f, STR("! uname %s\n"), conf.uname);

  comment("");

  if (conf.username && my_username() && strcmp(conf.username, my_username())) {
    conf_com();
    my_write(f, STR("%s! username %s\n"), do_confedit == CONF_AUTO ? "" : "#", my_username());
    my_write(f, STR("%s! username %s\n"), do_confedit == CONF_STATIC ? "" : "#", conf.username);
  } else
    my_write(f, STR("! username %s\n"), conf.username ? conf.username : my_username() ? my_username() : "");

  if (conf.homedir && homedir(0) && strcmp(conf.homedir, homedir(0))) {
    conf_com();
    my_write(f, STR("%s! homedir %s\n"), do_confedit == CONF_AUTO ? "" : "#", homedir(0));
    my_write(f, STR("%s! homedir %s\n"), do_confedit == CONF_STATIC ? "" : "#", conf.homedir);
  } else 
    my_write(f, STR("! homedir %s\n"), conf.homedir ? conf.homedir : homedir(0) ? homedir(0) : "");

  comment("\n# binpath needs to be full path unless it begins with '~', which uses 'homedir', ie, '~/'");

  if (homedir() && strstr(conf.binpath, homedir())) {
    p = replace(conf.binpath, homedir(), "~");
    my_write(f, STR("! binpath %s\n"), p);
  } else if (conf.homedir && strstr(conf.binpath, conf.homedir)) { /* Could be an older homedir */
    p = replace(conf.binpath, conf.homedir, "~");
    my_write(f, STR("! binpath %s\n"), p);
  } else
    my_write(f, STR("! binpath %s\n"), conf.binpath);

  comment("# binname is relative to binpath, if you change this, you'll need to manually remove the old one from crontab.");
  my_write(f, STR("! binname %s\n"), conf.binname);

  comment("");

  if (conf.datadir && strcmp(conf.datadir, "./...")) {
    comment("# datadir should be set to a static directory that is writable");

    if (homedir() && strstr(conf.datadir, homedir())) {
      p = replace(conf.datadir, homedir(), "~");
      my_write(f, STR("! datadir %s\n"), p);
    } else if (conf.homedir && strstr(conf.datadir, conf.homedir)) { /* Could be an older homedir */
      p = replace(conf.datadir, conf.homedir, "~");
      my_write(f, STR("! datadir %s\n"), p);
    } else
      my_write(f, STR("! datadir %s\n"), conf.datadir);

    comment("");
  }

  if (conf.portmin || conf.portmax) {
    comment("# portmin/max are for incoming connections (DCC) [0 for any] (These only make sense for HUBS)");
    my_write(f, STR("! portmin %d\n"), conf.portmin);
    my_write(f, STR("! portmax %d\n"), conf.portmax);

    comment("");
  }


  if (conf.pscloak) {
    comment("# Attempt to \"cloak\" the process name in `ps` for Linux?");
    my_write(f, STR("! pscloak %d\n"), conf.pscloak);

    comment("");
  }

  if (conf.autocron == 0) {
    comment("# Automatically add the bot to crontab?");
    my_write(f, STR("! autocron %d\n"), conf.autocron);

    comment("");
  }

  if (conf.autouname) {
    comment("# Automatically update 'uname' if it changes? (DANGEROUS)");
    my_write(f, STR("! autouname %d\n"), conf.autouname);

    comment("");
  }

#ifdef NO
  comment("# This will spawn a child process for EACH BOT that will block ALL process hijackers.");
  my_write(f, STR("! watcher %d\n"), conf.watcher);

  comment("");
#endif

  comment("# '|' means OR, [] means the enclosed is optional");
  comment("# A '+' in front of HOST means the HOST is ipv6");
  comment("# A '/' in front of BOT will disable that bot.");
  comment("#[/]BOT IP|* [+]HOST|* [IPV6-IP]");
  comment("#bot ip vhost");
  comment("#bot2 * vhost");
  comment("#bot3 ip");
  comment("#bot4 * +ipv6.vhost.com");
  comment("#bot5 * * ip:v6:ip:goes:here::");
  comment("### Hubs should have their own binary ###");

#endif /* CYGWIN_HACKS */
  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    my_write(f, STR("%s%s %s %s%s %s\n"), 
             bot->disabled ? "/" : "", bot->nick,
             bot->net.ip ? bot->net.ip : "*", bot->net.host6 ? "+" : "",
             bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "*"), bot->net.ip6 ? bot->net.ip6 : "");
  }

  fflush(f);

  if (!stream)
    fclose(f);

  return autowrote;
}

void
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

conf_bot *conf_bots_dup(conf_bot *src)
{
  conf_bot *ret = NULL;
  if (src) {
    conf_bot *bot = NULL, *newbot = NULL;

    for (bot = src; bot && bot->nick; bot = bot->next) {
      newbot = (conf_bot *) my_calloc(1, sizeof(conf_bot));
      conf_bot_dup(newbot, bot);
      list_append((struct list_type **) &(ret), (struct list_type *) newbot);
    }
  }
  return ret;
}

void deluser_removed_bots(conf_bot *oldlist, conf_bot *newlist)
{
  if (oldlist) {
    conf_bot *botold = NULL, *botnew = NULL;
    bool found = 0;
    struct userrec *u = NULL;

    for (botold = oldlist; botold && botold->nick; botold = botold->next) {
      found = 0;
      for (botnew = newlist; botnew && botnew->nick; botnew = botnew->next) {
        if (!egg_strcasecmp(botold->nick, botnew->nick)) {
          found = 1;
          break;
        }
      }
      if (!found && egg_strcasecmp(botold->nick, origbotnick)) {	/* Never kill ME.. will handle it elsewhere */
	/* No need to kill -- they are signalled and they will die on their own now */
        //botold->pid = checkpid(botold->nick, botold);
        //conf_killbot(conf.bots, NULL, botold, SIGKILL);
        if ((u = get_user_by_handle(userlist, botold->nick))) {
          putlog(LOG_MISC, "*", STR("Removing '%s' as it has been removed from the binary config."), botold->nick);
          if (server_online)
            check_this_user(botold->nick, 1, NULL);
          if (deluser(botold->nick)) {
            /* there is likely NO conf[] 
            if (conf.bot->hub)
              write_userfile(-1);
            */
          }
        }
      }
    }
  }
}

void
fill_conf_bot(bool fatal)
{
  if (!conf.bots || !conf.bots->nick)
    return;

  char *mynick = NULL;
  conf_bot *me = NULL;

  /* This first clause should actually be obsolete */
  if (!used_B && conf.bots && conf.bots->nick) {
    mynick = strdup(conf.bots->nick);
    strlcpy(origbotnick, conf.bots->nick, HANDLEN + 1);
  } else
    mynick = strldup(origbotnick, HANDLEN);

  sdprintf(STR("mynick: %s"), mynick);

  for (me = conf.bots; me && me->nick; me = me->next)
    if (!egg_strcasecmp(me->nick, mynick))
      break;

  if (fatal && (!me || (me->nick && egg_strcasecmp(me->nick, mynick))))
    werr(ERR_BADBOT);

  free(mynick);

  if (me) {
    if (!me->hub && me->localhub)
      sdprintf(STR("I am localhub!"));

    /* for future, we may just want to make this a pointer to ->bots if we do an emech style currentbot-> */
    conf.bot = (conf_bot *) my_calloc(1, sizeof(conf_bot));
    conf_bot_dup(conf.bot, me);
  }
}

void
bin_to_conf(bool error)
{
/* printf("Converting binary data to conf struct\n"); */
  conf.features = atol(settings.features);
  conf.uid = atol(settings.uid);
  if (settings.username[0])
    str_redup(&conf.username, settings.username);
  str_redup(&conf.uname, settings.uname); 
  str_redup(&conf.datadir, settings.datadir);
  if (settings.homedir[0])
    str_redup(&conf.homedir, settings.homedir);
  str_redup(&conf.binpath, settings.binpath);
  str_redup(&conf.binname, settings.binname);
  conf.portmin = atol(settings.portmin);
  conf.portmax = atol(settings.portmax);
  conf.autouname = atoi(settings.autouname);
  conf.autocron = atoi(settings.autocron);
  conf.watcher = atoi(settings.watcher);
  conf.pscloak = atoi(settings.pscloak);


  prep_homedir(error);
  expand_tilde(&conf.datadir);
  expand_tilde(&conf.binpath);

  /* PARSE/ADD BOTS */
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

  char datadir[PATH_MAX] = "";
  realpath(conf.datadir, datadir);
  str_redup(&conf.datadir, datadir);
  if (!mkdir_p(conf.datadir) && error)
    werr(ERR_DATADIR);

  str_redup(&conf.datadir, replace(datadir, conf.binpath, "."));

  Tempfile::FindDir();

  if (clear_tmpdir)
    clear_tmp();	/* clear out the tmp dir, no matter if we are localhub or not */
  conf_checkpids(conf.bots);

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
    if (!bot->hub && tands > 0 && !bot->disabled) {
      u = get_user_by_handle(userlist, bot->nick);
      if (!u) {
        putlog(LOG_MISC, "*", STR("Adding bot '%s' as it has been added to the binary config."), bot->nick);
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
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);

        simple_snprintf(uhost, sizeof(uhost), "*!~%s@%s", conf.username, bot->net.ip);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.host) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.host);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);

        simple_snprintf(uhost, sizeof(uhost), "*!~%s@%s", conf.username, bot->net.host);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.host6) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.host6);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);

        simple_snprintf(uhost, sizeof(uhost), "*!~%s@%s", conf.username, bot->net.host6);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);
      }
      if (bot->net.ip6) {
        simple_snprintf(uhost, sizeof(uhost), "*!%s@%s", conf.username, bot->net.ip6);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);

        simple_snprintf(uhost, sizeof(uhost), "*!~%s@%s", conf.username, bot->net.ip6);
        if (!user_has_host(NULL, u, uhost) && !host_conflicts(uhost))
          addhost_by_handle(bot->nick, uhost);
      }
    }
  }

}

conf_bot *conf_getlocalhub(conf_bot *bots) {
  if (!bots)
    return NULL;

  conf_bot *localhub = bots;
  if (localhub->disabled)
    while (localhub && localhub->disabled)
      localhub = localhub->next;

  if (!localhub) return NULL;
  return !localhub->disabled ? localhub : NULL;
}


void conf_setmypid(pid_t pid) {
  conf.bot->pid = pid;
  conf_bot *bot = conf.bots;
  if (conf.bots) {
    for (; bot && egg_strcasecmp(bot->nick, conf.bot->nick); bot = bot->next)
     ;
    if (bot)
      bot->pid = pid;
  }
}

