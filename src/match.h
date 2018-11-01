#ifndef _MATCH_H
#define _MATCH_H
/*
int wild_match(unsigned char *, unsigned char *);
int wild_match_per(unsigned char *, unsigned char *);
*/

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))
#define wild_match_per(a,b) _wild_match_per((unsigned char *)(a),(unsigned char *)(b))

int _wild_match(const unsigned char *, const unsigned char *) __attribute__((pure));
int _wild_match_per(const unsigned char *, const unsigned char *) __attribute__((pure));

//int wild_match(char *, char *);
//int wild_match_per(char *, char *);

int match_cidr(const char *, const char *) __attribute__((pure));

#endif /* !_MATCH_H */
