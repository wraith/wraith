#ifndef _AUTH_H
#  define _AUTH_H

#  include "crypt.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>

#define AUTHED	    1
#define AUTHING     2
/* These are what we are expecting back from the user */
#define AUTH_PASS   3
#define AUTH_HASH   4
#define AUTH_BDHASH 5

class Auth {
  public:
  Auth(const char *, const char *, struct userrec * = NULL);
  ~Auth();

  int Status(int newstat = -1) { if (newstat >= 0) { status = newstat; } return status; }
  void MakeHash();
  bool Authed() { return (status == AUTHED); }
  bool GetIdx(const char *);
  void Done();
  void NewNick(const char *nick);

  static Auth *Find(const char * host);
  static void NullUsers(const char *nick = NULL);
  static void FillUsers(const char *nick = NULL);
  static void ExpireAuths();
  static void InitTimer();
  static void DeleteAll();
  static void TellAuthed(int);

  struct userrec *user;
  time_t authtime;              /* what time they authed at */
  time_t atime;                 /* when they last were active */
  int idx;			/* do they have an associated idx? */
  char hash[MD5_HASH_LENGTH + 1];       /* used for dcc authing */
  char rand[51];
  char nick[NICKLEN];
  char host[UHOSTLEN];

  static bd::HashTable<bd::String, Auth*> ht_host;
  static bd::HashTable<bd::String, Auth*> ht_nick;

  private:
  int status;
};

void makehash(struct userrec *u, const char *randstring, char *out, size_t out_size);

int check_auth_dcc(Auth *, const char *, const char *);

#endif /* !_AUTH_H */
