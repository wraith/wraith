#ifndef _MATCH_H
#define _MATCH_H
/*
int wild_match(register unsigned char *, register unsigned char *);
int wild_match_per(register unsigned char *, register unsigned char *);
*/

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))
#define wild_match_per(a,b) _wild_match_per((unsigned char *)(a),(unsigned char *)(b))

int _wild_match(register unsigned char *, register unsigned char *);
int _wild_match_per(register unsigned char *, register unsigned char *);

//int wild_match(register char *, register char *);
//int wild_match_per(register char *, register char *);

#endif /* !_MATCH_H */
