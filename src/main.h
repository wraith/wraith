#ifndef _MAIN_H
#define _MAIN_H

#include <sys/types.h>

extern int		localhub, role, loading, default_flags, default_uflags,
			backgrd, term_z, updating, use_stderr, do_restart;
extern char		tempdir[], *binname, owner[], version[], ver[], quit_msg[];
extern time_t		online_since, now;
extern uid_t		myuid;
extern const time_t	buildts;
extern const char	egg_version[];

void fatal(const char *, int);

#endif /* !_MAIN_H */
