#ifndef _USERREC_H
#define _USERREC_H

namespace bd {
  class Stream;
}

struct userrec *adduser(struct userrec *, const char *, char *, char *, flag_t, int);
void addhost_by_handle(char *, char *);
void clear_masks(struct maskrec *);
void clear_cached_users();
void cache_users();
void clear_userlist(struct userrec *);
int u_pass_match(struct userrec *, const char *);
int delhost_by_handle(char *, char *);
int count_users(struct userrec *);
int deluser(char *);
int change_handle(struct userrec *, char *);
void correct_handle(char *);
void stream_writeuserfile(bd::Stream&, const struct userrec *, bool = 0);
int real_write_userfile(int);
int write_userfile(int);
void touch_laston(struct userrec *, char *, time_t);
void user_del_chan(char *);
struct userrec *host_conflicts(char *);
struct userrec *get_user_by_handle(struct userrec *, const char *);
struct userrec *get_user_by_host(char *);

extern struct userrec  		*userlist, *lastuser;
extern int			cache_hit, cache_miss, userfile_perm;
extern int			noshare;
#endif /* !_USERREC_H */
