/* enclink.h
 *
 */

#ifndef _ENCLINK_H
#define _ENCLINK_H

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
  void (*func) (int, direction_t);
};


extern struct enc_link enclink[];


extern void enclink_call(int, int, direction_t);

#endif /* !_ENCLINK_H */
