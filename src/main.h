#ifndef _MAIN_H
#define _MAIN_H

#include <sys/types.h>
#include "egg_timer.h"

enum {
  UPDATE_AUTO = 1,
  UPDATE_EXIT
};

enum {
  CONF_AUTO = 1,
  CONF_STATIC
};

extern int		role, default_flags, default_uflags, do_confedit,
			updating, do_restart, do_write_userfile;
extern bool		use_stderr, backgrd, used_B, term_z, loading, have_linked_to_hub, restart_was_update, restarting, safe_to_log;
extern char		tempdir[], *binname, owner[121], version[151], ver[101], quit_msg[], *socksfile;
extern time_t		online_since, now, restart_time;
extern egg_timeval_t	egg_timeval_now;
extern uid_t		myuid;
extern pid_t            mypid;
extern const time_t	buildts;
extern const char	*egg_version, *commit, *branch;

void fatal(const char *, int);

#endif /* !_MAIN_H */
