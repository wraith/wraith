#ifndef _CORE_BINDS_H_
#define _CORE_BINDS_H_

#ifndef MAKING_MODS
void core_binds_init();
void check_bind_time(struct tm *tm);
void check_bind_dcc(const char *, int, const char *);
void check_bind_chjn(const char *, const char *, int, char, int, const char *);
void check_bind_chpt(const char *, const char *, int, int);
void check_bind_bot(const char *, const char *, const char *);
void check_bind_link(const char *, const char *);
void check_bind_disc(const char *);
int check_bind_note(const char *, const char *, const char *);
void check_bind_nkch(const char *, const char *);
void check_bind_away(const char *, int, const char *);
int check_bind_chat(char *, int, const char *);
void check_bind_act(const char *, int, const char *);
void check_bind_bcst(const char *, int, const char *);
void check_bind_chon(char *, int);
void check_bind_chof(char *, int);
#endif /* !MAKING_MODS */
#endif /* !_CORE_BINDS_H */
