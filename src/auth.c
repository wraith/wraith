/* I hereby release this into the Public Domain - Bryan Drewery */
/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */


#include "common.h"
#include "auth.h"
#include "misc.h"
#include "main.h"
#include "settings.h"
#include "types.h"
#include "userrec.h"
#include "set.h"
#include "core_binds.h"
#include "egg_timer.h"
#include "users.h"
#include "crypt.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "src/mod/server.mod/server.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <pwd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>


#include "stat.h"

bd::HashTable<bd::String, Auth*> Auth::ht_handle(10);
bd::HashTable<bd::String, Auth*> Auth::ht_host(10);

Auth::Auth(const char *_nick, const char *_host, struct userrec *u)
{
  Status(AUTHING);
  strlcpy(nick, _nick, nick_len + 1);
  strlcpy(host, _host, UHOSTLEN);
  if (u) {
    user = u;
    strlcpy(handle, u->handle, sizeof(handle));
  } else {
    user = NULL;
    handle[0] = '*';
    handle[1] = 0;
  }


  ht_host[host] = this;
  if (user)
    ht_handle[handle] = this;

  sdprintf(STR("New auth created! (%s!%s) [%s]"), nick, host, handle);
  authtime = atime = now;
  idx = -1;
}

Auth::~Auth()
{
  sdprintf(STR("Removing auth: (%s!%s) [%s]"), nick, host, handle);
  if (user)
    ht_handle.remove(handle);
  ht_host.remove(host);
}

void Auth::MakeHash()
{
 make_rand_str(rand, 50);
 makehash(user, rand, hash, 50);
}

void Auth::Done()
{
  hash[0] = 0;
  rand[0] = 0;
  Status(AUTHED);
}

void Auth::NewNick(const char *newnick) {
  strlcpy(nick, newnick, nick_len + 1);
}

Auth *Auth::Find(const char *_host)
{

  if (ht_host.contains(_host)) {
    Auth *auth = ht_host[_host];
    sdprintf(STR("Found auth: (%s!%s) [%s]"), auth->nick, auth->host, auth->handle);
    return auth;
  }
  return NULL;
}

Auth *Auth::Find(const char *handle, bool _hand)
{
  if (ht_handle.contains(handle)) {
    Auth *auth = ht_handle[handle];
    sdprintf(STR("Found auth (by handle): %s (%s!%s)"), handle, auth->nick, auth->host);
    return auth;
  }
  return NULL;
}

static void auth_clear_users_block(const bd::String key, Auth* auth, void *param)
{
  if (auth->user) {
    sdprintf(STR("Clearing USER for auth: (%s!%s) [%s]"), auth->nick, auth->host, auth->handle);
    auth->user = NULL;
  }
}

void Auth::NullUsers()
{
  ht_host.each(auth_clear_users_block);
}

static void auth_fill_users_block(const bd::String key, Auth* auth, void* param)
{
  if (strcmp(auth->handle, "*")) {
    sdprintf(STR("Filling USER for auth: (%s!%s) [%s]"), auth->nick, auth->host, auth->handle);
    auth->user = get_user_by_handle(userlist, auth->handle);
  }
}

void Auth::FillUsers()
{
  ht_host.each(auth_fill_users_block);
}


static void auth_expire_block(const bd::String key, Auth* auth, void* param)
{
  if (auth->Authed() && ((now - auth->atime) >= (60 * 60))) {
    Auth::ht_host.remove(key);
    Auth::ht_handle.remove(key);
    delete auth;
  }
}

void Auth::ExpireAuths()
{
  if (!ischanhub())
    return;

  ht_host.each(auth_expire_block);
}

static void auth_delete_all_block(const bd::String, Auth* auth, void* param)
{
  putlog(LOG_DEBUG, "*", STR("Removing (%s!%s) [%s], from auth list."), auth->nick, auth->host, auth->handle);
  delete auth;
}

void Auth::DeleteAll()
{
  if (ischanhub()) {
    putlog(LOG_DEBUG, "*", STR("Removing auth entries."));
    ht_host.each(auth_delete_all_block);
    ht_host.clear();
    ht_handle.clear();
  }
}

void Auth::InitTimer()
{
  timer_create_secs(60, STR("Auth::ExpireAuths"), (Function) Auth::ExpireAuths);
}

bool Auth::GetIdx(const char *chname)
{
sdprintf(STR("GETIDX: auth: %s, idx: %d"), nick, idx);
  if (idx != -1) {
    if (!valid_idx(idx))
      idx = -1;
    else {
      sdprintf(STR("FIRST FOUND: %d"), idx);
      strlcpy(dcc[idx].simulbot, chname ? chname : nick, sizeof(dcc[idx].simulbot));
      strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);
      return 1;
    }
  }

  int i = 0;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].irc &&
       (((chname && chname[0]) && !strcmp(dcc[i].simulbot, chname) && !strcmp(dcc[i].nick, handle)) ||
       (!(chname && chname[0]) && !strcmp(dcc[i].simulbot, nick)))) {
      putlog(LOG_DEBUG, "*", STR("Simul found old idx for %s/%s: (%s!%s)"), nick, chname, nick, host);
      dcc[i].simultime = now;
      idx = i;
      strlcpy(dcc[idx].simulbot, chname ? chname : nick, sizeof(dcc[idx].simulbot));
      strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);

      return 1;
    }
  }

  idx = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

  if (idx != -1) {
    dcc[idx].sock = -1;
    dcc[idx].timeval = now;
    dcc[idx].irc = 1;
    dcc[idx].simultime = now;
    dcc[idx].simul = 0;		/* not -1, so it's cleaned up later */
    dcc[idx].status = STAT_COLOR;
    dcc[idx].u.chat->con_flags = 0;
    strlcpy(dcc[idx].simulbot, chname ? chname : nick, sizeof(dcc[idx].simulbot));
    strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);
    dcc[idx].u.chat->strip_flags = STRIP_ALL;
    strlcpy(dcc[idx].nick, handle, sizeof(dcc[idx].nick));
    strlcpy(dcc[idx].host, host, sizeof(dcc[idx].host));
    dcc[idx].addr = 0L;
    dcc[idx].user = user ? user : get_user_by_handle(userlist, handle);
    return 1;
  }

  return 0;
}

static void auth_tell_block(const bd::String key, Auth* auth, void* param)
{
  long lparam = (long) param;
  int idx = (int) lparam;

  dprintf(idx, "(%s!%s) [%s] authtime: %li, atime: %li, Status: %d\n", auth->nick, 
        auth->host, auth->handle, (long)auth->authtime, (long)auth->atime, auth->Status());
}

void Auth::TellAuthed(int idx)
{
  ht_host.each(auth_tell_block, (void *) (long) idx);
}

void makehash(struct userrec *u, const char *randstring, char *out, size_t out_size)
{
  char hash[256] = "", *secpass = NULL;

  if (u && get_user(&USERENTRY_SECPASS, u)) {
    secpass = strdup((char *) get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  simple_snprintf(hash, sizeof hash, "%s%s%s", randstring, (secpass && secpass[0]) ? secpass : "", auth_key);
  if (secpass)
    free(secpass);

  strlcpy(out, MD5(hash), out_size);
  OPENSSL_cleanse(hash, sizeof(hash));
}

void check_auth_dcc(Auth *auth, const char *cmd, const char *par)
{
  real_check_bind_dcc(cmd, auth->idx, par, auth);
}
