#ifndef _MAIN_H
#define _MAIN_H

#ifndef MAKING_MODS
int crontab_exists();
void crontab_create(int);
void fatal(const char *, int);
void eggContext(const char *, int, const char *);
void eggContextNote(const char *, int, const char *, const char *);
void eggAssert(const char *, int, const char *);
#endif /* !MAKING_MODS */

#endif /* !_MAIN_H */
