#ifndef _BOTCMD_H
#define _BOTCMD_H


#ifndef MAKING_MODS
void bounce_simul(int, char *);
void send_remote_simul(int, char *, char *, char *);
void bot_share(int, char *);
void bot_shareupdate(int, char *);
int base64_to_int(char *);
#endif /* !MAKING_MODS */

#endif /* !_BOTCMD_H */
