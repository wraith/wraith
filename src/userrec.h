#ifndef _USERREC_H
#define _USERREC_H

void deflag_user(struct userrec *, int, char *, struct chanset_t *);
struct userrec *adduser(struct userrec *, char *, char *, char *, int);
void addhost_by_handle(char *, char *);
void clear_masks(struct maskrec *);
void clear_userlist(struct userrec *);
int u_pass_match(struct userrec *, char *);
int delhost_by_handle(char *, char *);
int ishost_for_handle(char *, char *);
int count_users(struct userrec *);
int deluser(char *);
void freeuser(struct userrec *);
int change_handle(struct userrec *, char *);
void correct_handle(char *);
#ifdef HUB
int write_user(struct userrec *u, FILE * f, int shr);
int write_userfile(int);
#endif /* HUB */
struct userrec *check_dcclist_hand(char *);
void touch_laston(struct userrec *, char *, time_t);
void user_del_chan(char *);
char *fixfrom(char *);

extern struct userrec  		*userlist, *lastuser;
extern int			noshare, cache_hit, cache_miss, strict_host,
				userfile_perm;
#endif /* !_USERREC_H */
