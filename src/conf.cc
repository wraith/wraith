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
#include "EncryptedStream.h"
#include <bdlib/src/String.h>
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

conf_t conf;                    /* global conf struct */

static void
tellconf()
{
  conf_bot *bot = NULL;
  int i = 0;
  sdprintf(STR("tempdir: %s\n"), replace(tempdir, conf.homedir, "~"));
  sdprintf(STR("features: %d\n"), conf.features);
  sdprintf(STR("uid: %d\n"), conf.uid);
  sdprintf(STR("homedir: %s\n"), conf.homedir);
  sdprintf(STR("username: %s\n"), conf.username);
  sdprintf(STR("datadir: %s\n"), replace(conf.datadir, conf.homedir, "~"));
  sdprintf(STR("portmin: %d\n"), conf.portmin);
  sdprintf(STR("portmax: %d\n"), conf.portmax);
  sdprintf(STR("autocron: %d\n"), conf.autocron);
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
             (int)bot->pid);
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
             (int)bot->pid);
  }
}

void spawnbot(const char *nick)
{
  int status = 0;

  sdprintf("Spawning '%s'", nick);
  const char* argv[] = {binname, nick, 0};
  status = simple_exec(argv);
  if (status == -1 || WEXITSTATUS(status))
    sdprintf("Failed to spawn '%s': %s", nick, strerror(errno));
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
    } else if ((conf.bot && !strcasecmp(bot->nick, conf.bot->nick) &&
               (updating == UPDATE_AUTO || rehashed)) || (bot->pid && !updating)) {
      sdprintf(STR(" ... skipping. Updating: %d, pid: %d"), updating, (int)bot->pid);
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
             (conf.bot->nick && strcasecmp(conf.bot->nick, bot->nick))) ||
          (botnick && 
             ((notbotnick == 0 && !strcasecmp(botnick, bot->nick)) ||
              (notbotnick == 1 && strcasecmp(botnick, bot->nick))
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

  autowrote = writeconf(NULL, tmpconf.fd, CONF_COMMENT);
  fsync(tmpconf.fd);
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

  my_gettime(&ts1);
  switch (pid = vfork()) {
    case -1:
      fatal(STR("Cannot fork"), 0);
    case 0:
    {
      /* child */
      execlp(editor, editor, tmpconf.file, (char*)NULL);
      perror(editor);
      _exit(127);
    }
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
              WCOREDUMP(waiter)
              ? "" : "no ");
      goto fatal;
    } else {
      break;
    }
  }


  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);

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
      tmpconf.my_close();
      unlink(tmpconf.file);
      exit(0);            
    }
  }

  tmpconf.my_close();

  oldbots = conf_bots_dup(conf.bots);
  free_conf();

  readconf((const char *) tmpconf.file, 0);               /* read cleartext conf tmp into &settings */
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

void
init_conf()
{
//  conf.bots = (conf_bot *) calloc(1, sizeof(conf_bot));
//  conf.bots->nick = NULL;
//  conf.bots->next = NULL;
  conf.bots = NULL;
  conf.bot = NULL;
  // If conf_hubs is blank, revert to pack hubs
  if (strlen(settings.conf_hubs)) {
    conf.hubs = bd::String(settings.conf_hubs).split(',');
  } else {
    conf.hubs = bd::String(settings.hubs).split(',');
  }

  conf.localhub = NULL;
  conf.autocron = 1;

  conf.features = 0;
  conf.portmin = 0;
  conf.portmax = 0;
  conf.uid = -1;
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

static char* datafile(const char* prefix, const char* nick) {
  static char buf[DIRMAX] = "";
  char *tmpnick = NULL, *tmp_ptr = NULL;

  tmpnick = tmp_ptr = strdup(nick);
  strtolower(tmpnick);
  simple_snprintf(buf, sizeof buf, STR("%s/.%s.%s"), conf.datadir, prefix, tmpnick);
  free(tmp_ptr);
  return buf;
}

/*
 * Return the PID of a bot if it is running, otherwise return 0
 */

pid_t
checkpid(const char *nick, conf_bot *bot)
{
  FILE *f = NULL;
  pid_t pid = 0;


  char buf[DIRMAX] = "";
  char *bufp = datafile("pid", nick);

  if (bot && !(bot->pid_file))
    bot->pid_file = strdup(bufp);
  else if (bot && strcasecmp(bot->pid_file, bufp))
    str_redup(&bot->pid_file, bufp);

  if ((f = fopen(bufp, "r"))) {
    char *pids = NULL;

    if (!fgets(buf, sizeof(buf), f)) {
      fclose(f);
      return 0;
    }
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


    if (bufp[0] && pid && can_stat(bufp) && (mypid == pid) &&
        !strncasecmp(nick, origbotnick, HANDLEN)) {
      socksfile = strdup(bufp);
      return 0;
    }
  }

  return pid;
}

void
conf_addbot(const char *nick, const char *ip, const char *host, const char *ip6)
{
  conf_bot *bot = (conf_bot *) calloc(1, sizeof(conf_bot));

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
    bot->u = get_user_by_handle(userlist, (char*)nick);
  else
    bot->u = NULL;

//  bot->pid = checkpid(nick, bot);

  if (is_hub(bot->nick))
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

    conf.localhub_socket = strdup(datafile("sock", conf.localhub));
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
    if (!strcasecmp(bot->nick, botn)) {     /* found it! */
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
  if (conf.username)
    free(conf.username);
  if (conf.datadir)
    free(conf.datadir);
  if (conf.homedir)
    free(conf.homedir);
  conf.hubs.clear();
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
  if (error && conf.uid == -1 && !conf.homedir)
    werr(ERR_NOTINIT);

  if (!conf.username)
    str_redup(&conf.username, my_username());

  if (error && (!conf.username || !conf.username[0]))
    werr(ERR_NOUSERNAME);

  if (error && conf.uid != (signed) myuid) {
    sdprintf(STR("wrong uid, conf: %d :: %d"), conf.uid, myuid);
    werr(ERR_WRONGUID);
  } else if (!conf.uid)
    conf.uid = myuid;

  return 0;
}

int
readconf(const char *fname, int bits)
{
  int enc = (bits & CONF_ENC) ? 1 : 0;
  bd::Stream* stream;
  size_t bots = 0;

  if (enc) {
    const char salt1[] = SALT1;
    stream = new EncryptedStream(salt1);
  } else
    stream = new bd::Stream;

  sdprintf(STR("readconf(%s, %d)"), fname, enc);

  if (stream->loadFile(fname)) {
    delete stream;
    fatal(STR("Cannot read config"), 0);
  }

  conf.hubs.clear();
  free_conf_bots(conf.bots);

  bd::String line, option;
  unsigned short hublevel = 0;

  while (stream->tell() < stream->length()) {
    line = stream->getline().chomp().trim();

    // Skip blank lines
    if (!line)
      continue;

    sdprintf(STR("CONF LINE: %s"), line.c_str());
// !strchr("_`|}][{*/#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ", line[0])) {
    /* - uid */
    if (line[0] == '!') {
      ++line;
      line.trim();

      option.clear();
      if (line.length())
        option = newsplit(line);

      if (!option || !line)
        continue;

//      option.toLower();

      if (option == STR("autocron")) {      /* automatically check/create crontab? */
        if (egg_isdigit(line[0]))
          conf.autocron = atoi(line.c_str());

      } else if (option == STR("username")) {       /* shell username */
        str_redup(&conf.username, line.c_str());

      } else if (option == STR("homedir")) {        /* homedir */
        str_redup(&conf.homedir, line.c_str());

      } else if (option == STR("datadir")) {        /* datadir */
        str_redup(&conf.datadir, line.c_str());

      } else if (option == STR("portmin")) {
        if (egg_isdigit(line[0]))
          conf.portmin = atoi(line.c_str());

      } else if (option == STR("portmax")) {
        if (egg_isdigit(line[0]))
          conf.portmax = atoi(line.c_str());

      } else if (option == STR("uid")) {    /* new method uid */
        if (str_isdigit(line.c_str()))
          conf.uid = atoi(line.c_str());

      } else if (option == STR("hub")) {
        if (line.split(' ').length() == 3) {
          conf.hubs << bd::String::printf("%s %d", line.c_str(), ++hublevel);
        }

      } else {
        putlog(LOG_MISC, "*", STR("Unrecognized config option '%s'"), option.c_str());

      }
      /* read in portmin */
    } else if (line[0] == '>') {
      conf.portmin = atoi(newsplit(line).c_str());

    } else if (line[0] == '<') {
      conf.portmax = atoi(newsplit(line).c_str());

      /* now to parse nick/hosts */
    } else if (line[0] != '#') {
      bd::String nick, host, ip, ipsix;

      nick = newsplit(line);
      if (!nick)
        werr(ERR_BADCONF);

      ip = newsplit(line);
      host = newsplit(line);
      ipsix = newsplit(line);

      conf_addbot(nick.c_str(), ip.c_str(), host.c_str(), ipsix.c_str());
      ++bots;
    }
  }                             /* while(fgets()) */

  if (bots > 5)
    werr(ERR_TOOMANYBOTS);

  delete stream;
  return 0;
}

char s1_9[3] = "",s1_5[3] = "",s1_1[3] = "";

bool hubSort (bd::String hub1, bd::String hub2) {
  bd::Array<bd::String> hub1params(static_cast<bd::String>(hub1).split(' '));
  bd::Array<bd::String> hub2params(static_cast<bd::String>(hub2).split(' '));

  unsigned short hub1level = 99, hub2level = 99;
  if (hub1params.length() == 4) {
    hub1level = atoi(static_cast<bd::String>(hub1params[3]).c_str());
  }
  if (hub2params.length() == 4) {
    hub2level = atoi(static_cast<bd::String>(hub2params[3]).c_str());
  }
  return hub1level < hub2level;
}

int
writeconf(char *filename, int fd, int bits)
{
  conf_bot *bot = NULL;
  int autowrote = 0;

  bd::Stream* stream;

  if (bits & CONF_ENC) {
    const char salt1[] = SALT1;
    stream = new EncryptedStream(salt1);
  } else
    stream = new bd::Stream;

#define comment(text)	do {		\
	if (bits & CONF_COMMENT)	\
	  *stream << bd::String::printf(STR("%s\n"), text);	\
} while(0)

  char *p = NULL;

  comment("");

#define conf_com() do {							\
	if (do_confedit == CONF_AUTO) {					\
	  comment("# Automatically updated with -C");			\
	  autowrote = 1;						\
        } else								\
	  comment("#The correct line follows. (Not auto due to -c)");	\
} while(0)

  if ((bits & CONF_COMMENT) && conf.uid != (signed) myuid) {
    conf_com();
    *stream << bd::String::printf(STR("%s! uid %d\n"), do_confedit == CONF_AUTO ? "" : "#", myuid);
    *stream << bd::String::printf(STR("%s! uid %d\n"), do_confedit == CONF_STATIC ? "" : "#", conf.uid);
  } else
    *stream << bd::String::printf(STR("! uid %d\n"), conf.uid);

  comment("");

  if (conf.username && my_username() && strcmp(conf.username, my_username())) {
    conf_com();
    *stream << bd::String::printf(STR("%s! username %s\n"), do_confedit == CONF_AUTO ? "" : "#", my_username());
    *stream << bd::String::printf(STR("%s! username %s\n"), do_confedit == CONF_STATIC ? "" : "#", conf.username);
  } else
    *stream << bd::String::printf(STR("! username %s\n"), conf.username ? conf.username : my_username() ? my_username() : "");

  if (conf.homedir && homedir(0) && strcmp(conf.homedir, homedir(0))) {
    conf_com();
    *stream << bd::String::printf(STR("%s! homedir %s\n"), do_confedit == CONF_AUTO ? "" : "#", homedir(0));
    *stream << bd::String::printf(STR("%s! homedir %s\n"), do_confedit == CONF_STATIC ? "" : "#", conf.homedir);
  } else 
    *stream << bd::String::printf(STR("! homedir %s\n"), conf.homedir ? conf.homedir : homedir(0) ? homedir(0) : "");

  comment("");

  if (conf.datadir && strcmp(conf.datadir, "./...")) {
    comment("# datadir should be set to a static directory that is writable");

    if (homedir() && strstr(conf.datadir, homedir())) {
      p = replace(conf.datadir, homedir(), "~");
      *stream << bd::String::printf(STR("! datadir %s\n"), p);
    } else if (conf.homedir && strstr(conf.datadir, conf.homedir)) { /* Could be an older homedir */
      p = replace(conf.datadir, conf.homedir, "~");
      *stream << bd::String::printf(STR("! datadir %s\n"), p);
    } else
      *stream << bd::String::printf(STR("! datadir %s\n"), conf.datadir);

    comment("");
  }

  if (conf.portmin || conf.portmax) {
    comment("# portmin/max are for incoming connections (DCC) [0 for any] (These only make sense for HUBS)");
    *stream << bd::String::printf(STR("! portmin %d\n"), conf.portmin);
    *stream << bd::String::printf(STR("! portmax %d\n"), conf.portmax);

    comment("");
  }


  if (conf.autocron == 0) {
    comment("# Automatically add the bot to crontab?");
    *stream << bd::String::printf(STR("! autocron %d\n"), conf.autocron);

    comment("");
  }

  if (conf.hubs.length()) {
    comment("# Hubs this bot will connect to");
    // Sort hub list by hublevel
    bd::Array<bd::String> sortedhubs(conf.hubs);
    std::sort(sortedhubs.begin(), sortedhubs.end(), hubSort);

    for (size_t idx = 0; idx < sortedhubs.length(); ++idx) {
      bd::Array<bd::String> hubparams(static_cast<bd::String>(sortedhubs[idx]).split(' '));
      bd::String hubnick(hubparams[0]), address(hubparams[1]);
      in_port_t port = atoi(static_cast<bd::String>(hubparams[2]).c_str());
      *stream << bd::String::printf(STR("! hub %s %s %d\n"), hubnick.c_str(), address.c_str(), port);
    }
    comment("");
  }

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

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    *stream << bd::String::printf(STR("%s%s %s %s%s"),
             bot->disabled ? "/" : "", bot->nick,
             bot->net.ip ? bot->net.ip : "*", bot->net.host6 ? "+" : "",
             bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "*"));
    if (bot->net.ip6)
      *stream << bd::String::printf(STR(" %s"), bot->net.ip6 ? bot->net.ip6 : "");
    *stream << "\n";
  }

  if (fd != -1)
    stream->writeFile(fd);
  else
    stream->writeFile(filename);

  delete stream;
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
      newbot = (conf_bot *) calloc(1, sizeof(conf_bot));
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
        if (!strcasecmp(botold->nick, botnew->nick)) {
          found = 1;
          break;
        }
      }
      if (!found && strcasecmp(botold->nick, origbotnick)) {	/* Never kill ME.. will handle it elsewhere */
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
    strlcpy(origbotnick, conf.bots->nick, sizeof(origbotnick));
  } else
    mynick = strldup(origbotnick, HANDLEN);

  sdprintf(STR("mynick: %s"), mynick);

  for (me = conf.bots; me && me->nick; me = me->next)
    if (!strcasecmp(me->nick, mynick))
      break;

  if (fatal && (!me || (me->nick && strcasecmp(me->nick, mynick))))
    werr(ERR_BADBOT);

  free(mynick);

  if (me) {
    if (!me->hub && me->localhub)
      sdprintf(STR("I am localhub!"));

    /* for future, we may just want to make this a pointer to ->bots if we do an emech style currentbot-> */
    conf.bot = (conf_bot *) calloc(1, sizeof(conf_bot));
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
  str_redup(&conf.datadir, settings.datadir);
  if (settings.homedir[0])
    str_redup(&conf.homedir, settings.homedir);
  conf.portmin = atol(settings.portmin);
  conf.portmax = atol(settings.portmax);
  conf.autocron = atoi(settings.autocron);


  prep_homedir(error);
  expand_tilde(&conf.datadir);

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
  if (!realpath(conf.datadir, datadir)) {
    ;
  }

  str_redup(&conf.datadir, datadir);
  if (!mkdir_p(conf.datadir) && error)
    werr(ERR_DATADIR);

  str_redup(&conf.datadir, replace(datadir, dirname(binname), "."));

  if (Tempfile::FindDir() == ERROR)
    werr(ERR_TMPSTAT);

  if (clear_tmpdir)
    clear_tmp();	/* clear out the tmp dir, no matter if we are localhub or not */
  conf_checkpids(conf.bots);

  tellconf();
}

void conf_update_hubs(struct userrec* list) {
  if (!conf.bot->hub && !conf.bot->localhub) {
    return;
  }

  bd::Array<bd::String> hubUsers, oldhubs(conf.hubs);

  // Count how many hubs there are
  for (struct userrec *u = list; u; u = u->next) {
    if (bot_hublevel(u) < 999) {
      hubUsers << u->handle;
    }
  }

  conf.hubs.clear();
  conf.hubs.Reserve(hubUsers.length());
  for (size_t idx = 0; idx < hubUsers.length(); ++idx) {
    struct userrec *u = get_user_by_handle(list, const_cast<char*>(static_cast<bd::String>(hubUsers[idx]).c_str()));
    struct bot_addr *bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u);
    conf.hubs << bd::String::printf("%s %s %d %d", u->handle, bi->address, bi->telnet_port, bi->hublevel);
  }

  // Only rewrite binary / notify bots if the hubs changed
  if (conf.hubs == oldhubs) {
    return;
  }

  if (conf.bot->hub || conf.bot->localhub) {
    /* rewrite our binary */
    conf_to_bin(&conf, 0, -1);

    /* Now signal all of the old bots with SIGUSR1
     * They will auto die and determine new localhub, etc..
     */
    conf_checkpids(conf.bots);
    conf_killbot(conf.bots, conf.bot->nick, NULL, SIGUSR1, 1); /* Don't kill me. */
  }
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

        simple_snprintf(tmp, sizeof(tmp), "%li %s", (long)now, conf.bot->nick);
        set_user(&USERENTRY_ADDED, u, tmp);

        bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

        bi->address = (char *) calloc(1, 1);
        bi->uplink = (char *) calloc(1, 1);
        bi->telnet_port = bi->relay_port = 3333;
        bi->hublevel = 999;
        set_user(&USERENTRY_BOTADDR, u, bi);

      }
      if (bot->net.ip && strcmp(bot->net.ip, "0.0.0.0")) {
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
      if (bot->net.ip6 && strcmp(bot->net.ip6, "::")) {
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
    for (; bot && strcasecmp(bot->nick, conf.bot->nick); bot = bot->next)
     ;
    if (bot)
      bot->pid = pid;
  }
}

/* vim: set sts=2 sw=2 ts=8 et: */
