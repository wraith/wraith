#ifndef _BOTCMD_H
#define _BOTCMD_H

void bounce_simul(int, char *);
void send_remote_simul(int, char *, char *, char *);
void bot_share(int, char *);
void bot_shareupdate(int, char *);
int base64_to_int(char *);
void init_botcmd(void);

#endif /* !_BOTCMD_H */
