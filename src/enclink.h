/* enclink.h
 *
 */

#ifndef _ENCLINK_H
#define _ENCLINK_H

#include <sys/types.h>

enum {
        LINK_GHOST = 0,
        LINK_GHOSTSHA1,
        LINK_GHOSTMD5,
        LINK_CLEARTEXT
};
enum direction_t {
        FROM,
        TO
};

struct enc_link {
  const char *name;
  int type;
  void (*link) (int, direction_t);
  char *(*write) (int, char *, size_t *);
  int (*read) (int, char *, size_t *);
};


extern struct enc_link enclink[];


extern int link_find_by_type(int);

extern void link_link(int, int, direction_t);
extern char *link_write(int, char *, size_t *);
extern int link_read(int, char *, size_t *);

#endif /* !_ENCLINK_H */
