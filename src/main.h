#ifndef _MAIN_H
#define _MAIN_H

extern int use_stderr;

#ifndef MAKING_MODS
int crontab_exists();
void crontab_create(int);
void fatal(const char *, int);
#endif /* !MAKING_MODS */

#endif /* !_MAIN_H */
