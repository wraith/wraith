/*
 * flags.c -- handles:
 *   all the flag matching/conversion functions in one neat package :)
 *
 */


#include "common.h"
#include "crypt.h"
#include "src/mod/share.mod/share.h"
#include "rfc1459.h"
#include "userrec.h"
#include "misc.h"
#include "dccutil.h"
#include "userent.h"
#include "users.h"
#include "chanprog.h"

flag_t FLAG[128];

void
init_flags()
{
  unsigned char i;

  for (i = 0; i < 'A'; i++)
    FLAG[(int) i] = 0;
  for (; i <= 'Z'; i++)
    FLAG[(int) i] = (flag_t) 1 << (26 + (i - 'A'));
  for (; i < 'a'; i++)
    FLAG[(int) i] = 0;
  for (; i <= 'z'; i++)
    FLAG[(int) i] = (flag_t) 1 << (i - 'a');
  for (; i < 128; i++)
    FLAG[(int) i] = 0;
}

/* Some flags are mutually exclusive -- this roots them out
 */
flag_t
sanity_check(flag_t atr, int bot)
{
  if (bot && (atr & (USER_PARTY | USER_MASTER | USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA)))
    atr &= ~(USER_PARTY | USER_MASTER | USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA);
/* only bots should be there: */
  if (!bot && (atr & (BOT_DOLIMIT | BOT_DOVOICE | BOT_UPDATEHUB | BOT_CHANHUB)))
    atr &= ~(BOT_DOLIMIT | BOT_DOVOICE | BOT_UPDATEHUB | BOT_CHANHUB);
  if (atr & USER_AUTOOP)
    atr |= USER_OP;
  if ((atr & USER_OP) && (atr & USER_DEOP))
    atr &= ~(USER_OP | USER_DEOP);
  if ((atr & USER_AUTOOP) && (atr & USER_DEOP))
    atr &= ~(USER_AUTOOP | USER_DEOP);
  if ((atr & USER_VOICE) && (atr & USER_QUIET))
    atr &= ~(USER_VOICE | USER_QUIET);
  /* Can't be admin without also being owner and having hub access */
  if (atr & USER_ADMIN)
    atr |= USER_OWNER | USER_HUBA | USER_PARTY;
  /* Hub access gets chanhub access */
  if (atr & USER_HUBA)
    atr |= USER_CHUBA;
  if (atr & USER_OWNER) {
    atr |= USER_MASTER;
  }
  /* Master implies botmaster, op */
  if (atr & USER_MASTER)
    atr |= USER_OP;
  /* Can't be botnet master without party-line access */
/*  if (atr & USER_BOTMAST)
    atr |= USER_PARTY;
*/
  return atr;
}

/* Sanity check on channel attributes
 */
flag_t
chan_sanity_check(flag_t chatr, int bot)
{
  /* these should only be global */
  if (chatr & (USER_PARTY | USER_ADMIN | USER_HUBA | USER_CHUBA | BOT_UPDATEHUB))
    chatr &= ~(USER_PARTY | USER_ADMIN | USER_HUBA | USER_CHUBA | BOT_UPDATEHUB);
  /* these should only be set on bots */
  if (!bot && (chatr & (BOT_DOLIMIT | BOT_DOVOICE | BOT_CHANHUB)))
    chatr &= ~(BOT_DOLIMIT | BOT_DOVOICE | BOT_CHANHUB);

  if ((chatr & USER_OP) && (chatr & USER_DEOP))
    chatr &= ~(USER_OP | USER_DEOP);
  if ((chatr & USER_AUTOOP) && (chatr & USER_DEOP))
    chatr &= ~(USER_AUTOOP | USER_DEOP);
  if ((chatr & USER_VOICE) && (chatr & USER_QUIET))
    chatr &= ~(USER_VOICE | USER_QUIET);
  /* Can't be channel owner without also being channel master */
  if (chatr & USER_OWNER)
    chatr |= USER_MASTER;
  /* Master implies op */
  if (chatr & USER_MASTER)
    chatr |= USER_OP;
  return chatr;
}

/* Get icon symbol for a user (depending on access level)
 *
 * (*) owner on any channel
 * (+) master on any channel
 * (%) botnet master
 * (@) op on any channel
 * (-) other
 */
char
geticon(int idx)
{
  if (!dcc[idx].user)
    return '-';

  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  if (glob_admin(fr))
    return '^';
  if (chan_owner(fr))
    return '*';
  if (chan_master(fr))
    return '+';
  if (chan_op(fr))
    return '@';
  return '-';
}

void
break_down_flags(const char *string, struct flag_record *plus, struct flag_record *minus)
{
  struct flag_record *which = plus;
  int chan = 0;                 /* 0 = glob, 1 = chan */
  int flags = plus->match;

  if (!(flags & FR_GLOBAL)) {
    if (flags & FR_CHAN)
      chan = 1;
    else
      return;                   /* We dont actually want any..huh? */
  }
  egg_bzero(plus, sizeof(struct flag_record));

  if (minus)
    egg_bzero(minus, sizeof(struct flag_record));

  plus->match = FR_OR;          /* Default binding type OR */
  while (*string) {
    switch (*string) {
      case '+':
        which = plus;
        break;
      case '-':
        which = minus ? minus : plus;
        break;
      case '|':
      case '&':
        if (!chan) {
          if (*string == '|')
            plus->match = FR_OR;
          else
            plus->match = FR_AND;
        }
        which = plus;
        chan++;
        if (chan == 2)
          string = "";
        else if (chan == 3)
          chan = 1;
        break;                  /* switch() */
      default:
      {
        flag_t flagbit = FLAG[(int) *string];

        if (flagbit) {
          switch (chan) {
            case 0:
              /* which->global |= (flag_t) 1 << (*string - 'a'); */
              which->global |=flagbit;

              break;
            case 1:
              /* which->chan |= (flag_t) 1 << (*string - 'a'); */
              which->chan |= flagbit;
              break;
          }
        }
      }
    }
    string++;
  }
  for (which = plus; which; which = (which == plus ? minus : 0)) {
    if (flags & FR_BOT) {
      which->global &= BOT_VALID;
      which->chan &= BOT_CHAN_VALID;
    } else {    
      which->global &= USER_VALID;
      which->chan &= USER_CHAN_VALID;
    }
  }

  plus->match |= flags;
  if (minus) {
    minus->match |= flags;
    if (!(plus->match & (FR_AND | FR_OR)))
      plus->match |= FR_OR;
  }
}

static int
flag2str(char *string, flag_t flag)
{
  unsigned char c;
  char *old = string;

  for (c = 0; c < 128; c++)
    if (flag & FLAG[(int) c])
      *string++ = c;

  if (string == old)
    *string++ = '-';

  return string - old;
}

int
build_flags(char *string, struct flag_record *plus, struct flag_record *minus)
{
  char *old = string;

  if (plus->match & FR_GLOBAL) {
    if (minus && plus->global)
      *string++ = '+';
    string += flag2str(string, plus->global);

    if (minus && minus->global) {
      *string++ = '-';
      string += flag2str(string, minus->global);
    }
  }
  if (plus->match & FR_CHAN) {
    if (plus->match & FR_GLOBAL)
      *string++ = (plus->match & FR_AND) ? '&' : '|';

    if (minus && plus->chan)
      *string++ = '+';
    string += flag2str(string, plus->chan);
    if (minus && minus->chan) {
      *string++ = '-';
      string += flag2str(string, minus->global);
    }
  }
  if (string == old) {
    *string++ = '-';
    *string = 0;
    return 0;
  }
  *string = 0;
  return string - old;
}

/* Returns 1 if flags match, 0 if they don't. */
int
flagrec_ok(struct flag_record *req, struct flag_record *have)
{
  if (req->match & FR_AND) {
    return flagrec_eq(req, have);
  } else if (req->match & FR_OR) {
    int hav = have->global;

    /* no flags, is a match */
    if (!req->chan && !req->global)
      return 1;
    if (hav & req->global)
      return 1;
    if (have->chan & req->chan)
      return 1;
    return 0;
  }
  return 0;
}

/* Returns 1 if flags match, 0 if they don't. */
int
flagrec_eq(struct flag_record *req, struct flag_record *have)
{
  if (req->match & FR_AND) {
    if (req->match & FR_GLOBAL) {
      if ((req->global &have->global) !=req->global)
        return 0;
    }
    if (req->match & FR_CHAN) {
      if ((req->chan & have->chan) != req->chan)
        return 0;
    }
    return 1;
  } else if (req->match & FR_OR) {
    if (!req->chan && !req->global)
      return 1;
    if (req->match & FR_GLOBAL) {
      if (have->global &req->global)
        return 1;
    }
    if (req->match & FR_CHAN) {
      if (have->chan & req->chan)
        return 1;
    }
    return 0;
  }
  return 0;                     /* fr0k3 binding, dont pass it */
}

void
set_user_flagrec(struct userrec *u, struct flag_record *fr, const char *chname)
{
  if (!u)
    return;

  struct chanuserrec *cr = NULL;
  flag_t oldflags = fr->match;
  char buffer[100] = "";
  struct chanset_t *ch;


  if (oldflags & FR_GLOBAL) {
    u->flags = fr->global;

    if (!noshare) {
      fr->match = FR_GLOBAL;
      build_flags(buffer, fr, NULL);
      shareout("a %s %s\n", u->handle, buffer);
    }
  }

  if ((oldflags & FR_CHAN) && chname) {
    for (cr = u->chanrec; cr; cr = cr->next)
      if (!rfc_casecmp(chname, cr->channel))
        break;
    ch = findchan_by_dname(chname);
    if (!cr && ch) {
      cr = (struct chanuserrec *) my_calloc(1, sizeof(struct chanuserrec));

      cr->next = u->chanrec;
      u->chanrec = cr;
      strlcpy(cr->channel, chname, sizeof cr->channel);
    }
    if (cr && ch) {
      cr->flags = fr->chan;
      if (!noshare) {
        fr->match = FR_CHAN;
        build_flags(buffer, fr, NULL);
        shareout("a %s %s %s\n", u->handle, buffer, chname);
      }
    }
  }
  fr->match = oldflags;
}

/* Always pass the dname (display name) to this function for chname <cybah>
 */
void
get_user_flagrec(struct userrec *u, struct flag_record *fr, const char *chname)
{
  fr->bot = 0;
  if (!u) {
    fr->global = fr->chan = 0;

    return;
  }

  if (u->bot)
    fr->bot = 1;
  if (fr->match & FR_GLOBAL) {
    fr->global = u->flags;
  } else {
    fr->global = 0;
  }

  if (fr->match & FR_CHAN) {
    struct chanuserrec *cr = NULL;

    if ((fr->match & FR_ANYWH) || (fr->match & FR_ANYCH)) {
      if (fr->match & FR_ANYWH)
        fr->chan = u->flags;
      for (cr = u->chanrec; cr; cr = cr->next) {
        if (findchan_by_dname(cr->channel)) {
          fr->chan |= cr->flags;
        }
      }
    } else {
      if (chname) {
        for (cr = u->chanrec; cr; cr = cr->next) {
          if (!rfc_casecmp(chname, cr->channel))
            break;
        }
      }

      if (cr) {
        fr->chan = cr->flags;
      } else {
        fr->chan = 0;
      }
    }
  }
}

/* private returns 0 if user has access, and 1 if they dont because of +private
 * This function does not check if the user has "op" access, it only checks if the user is
 * restricted by +private for the channel
 */
int
privchan(struct flag_record fr, struct chanset_t *chan, int type)
{
  if (!chan || !channel_privchan(chan) || glob_bot(fr) || glob_owner(fr))
    return 0;                   /* user is implicitly not restricted by +private, they may however be lacking other flags */

  if (type == PRIV_OP) {
    /* |o implies all flags above. n| has access to all +private. Bots are exempt. */
    if (chan_op(fr))
      return 0;
  } else if (type == PRIV_VOICE) {
    if (chan_voice(fr) || chan_op(fr))
      return 0;
  }
  return 1;                     /* user is restricted by +private */
}

int
chk_op(struct flag_record fr, struct chanset_t *chan)
{
  return real_chk_op(fr, chan, 1);
}

int
real_chk_op(struct flag_record fr, struct chanset_t *chan, bool botbitch)
{
  if (!chan && glob_op(fr))
    return 1;
  else if (!chan)
    return 0;

  if (!privchan(fr, chan, PRIV_OP) && !real_chk_deop(fr, chan, botbitch)) {
    if (chan_op(fr) || (glob_op(fr) && !chan_deop(fr)))
      return 1;
  }
  return 0;
}

int
chk_autoop(struct flag_record fr, struct chanset_t *chan)
{
  if (glob_bot(fr))
    return 0;
  if (!chan || (!channel_take(chan) && !privchan(fr, chan, PRIV_OP) && chk_op(fr, chan) && !chk_deop(fr, chan))) {
    if (channel_autoop(chan) || chan_autoop(fr) || glob_autoop(fr))
      return 1;
  }
  return 0;
}

int chk_deop(struct flag_record fr, struct chanset_t *chan)
{
  return real_chk_deop(fr, chan, 1);
}

int
real_chk_deop(struct flag_record fr, struct chanset_t *chan, bool botbitch)
{
  if (chan && botbitch && channel_botbitch(chan) && !glob_bot(fr))
    return 1;

  if (chan_deop(fr) || (glob_deop(fr) && !chan_op(fr)))
    return 1;
  else
    return 0;
}

int
chk_voice(struct flag_record fr, struct chanset_t *chan)
{
  if (!chan || (!privchan(fr, chan, PRIV_VOICE) && !chk_devoice(fr))) {
    if (chan_voice(fr) || (glob_voice(fr) && !chan_quiet(fr)))
      return 1;
  }
  return 0;
}

int
chk_devoice(struct flag_record fr)
{
  if (chan_quiet(fr) || (glob_quiet(fr) && !chan_voice(fr)))
    return 1;
  else
    return 0;
}

int
chk_noflood(struct flag_record fr)
{
  return (chan_noflood(fr) || glob_noflood(fr));
}

int
isupdatehub()
{
  if (conf.bot->hub && conf.bot->u && (conf.bot->u->flags & BOT_UPDATEHUB))
    return 1;
  else
    return 0;
}

int
ischanhub()
{
  if (!conf.bot->hub && conf.bot->u && (conf.bot->u->flags & BOT_CHANHUB))
    return 1;
  else
    return 0;
}

int
doresolv(struct chanset_t *chan)
{
  if (!chan)
    return 0;

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(conf.bot->u, &fr, chan->dname);
  if (glob_doresolv(fr) || chan_doresolv(fr))
    return 1;
  return 0;
}

int
dovoice(struct chanset_t *chan)
{
  if (!chan)
    return 0;

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(conf.bot->u, &fr, chan->dname);
  if (glob_dovoice(fr) || chan_dovoice(fr))
    return 1;
  return 0;
}

int
dolimit(struct chanset_t *chan)
{
  if (!chan)
    return 0;

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(conf.bot->u, &fr, chan->dname);
  if (glob_dolimit(fr) || chan_dolimit(fr))
    return 1;
  return 0;
}

int
whois_access(struct userrec *user, struct userrec *whois_user)
{
  if (user == whois_user)
    return 1;

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 }, whois = {
  FR_GLOBAL | FR_CHAN, 0, 0, 0};

  get_user_flagrec(user, &fr, NULL);
  get_user_flagrec(whois_user, &whois, NULL);

  /* Don't show hub bots from leaf bots. */
  if (!conf.bot->hub && whois_user->bot && bot_hublevel(whois_user) < 999)
    return 0;

  if (
      (isowner(whois_user->handle) && !isowner(user->handle)) ||
      (glob_admin(whois) && !glob_admin(fr)) || 
      (glob_owner(whois) && !glob_owner(fr)) ||
      (glob_master(whois) && !glob_master(fr)) ||
      (glob_bot(whois) && !glob_master(fr))
     )
    return 0;
  return 1;
}

int deflag_translate(const char *buf)
{
  if (str_isdigit(buf))
    return (atoi(buf));

  if (!egg_strcasecmp(buf, "ignore"))
    return P_IGNORE;
  else if (!egg_strcasecmp(buf, "deop"))
    return P_DEOP;
  else if (!egg_strcasecmp(buf, "kick"))
    return P_KICK;
  else if (!egg_strcasecmp(buf, "delete") || !egg_strcasecmp(buf, "remove"))
    return P_DELETE;
  return P_IGNORE;
}
void deflag_user(struct userrec *u, int why, char *msg, struct chanset_t *chan)
{
  if (!u)
    return;

  char tmp[256] = "", tmp2[1024] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int which = 0;

  switch (why) {
  case DEFLAG_BADCOOKIE:
    strcpy(tmp, "Bad op cookie");
    which = chan->bad_cookie;
    break;
  case DEFLAG_MANUALOP:
    strcpy(tmp, STR("Manual op in -manop channel"));
    which = chan->manop;
    break;
#ifdef G_MEAN
  case DEFLAG_MEAN_DEOP:
    strcpy(tmp, STR("Deopped bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_KICK:
    strcpy(tmp, STR("Kicked bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_BAN:
    strcpy(tmp, STR("Banned bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
#endif /* G_MEAN */
  case DEFLAG_MDOP:
    strcpy(tmp, "Mass deop");
    which = chan->mdop;
    break;
  case DEFLAG_MOP:
    strcpy(tmp, "Mass op");
    which = chan->mop;
    break;
  default:
    simple_sprintf(tmp, "Reason #%i", why);
  }
  if (which == P_DEOP) {
    putlog(LOG_WARN, "*",  "Setting %s +d (%s): %s", u->handle, tmp, msg);
    simple_sprintf(tmp2, "+d: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP;
    fr.chan = USER_DEOP;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (which == P_KICK) {
    putlog(LOG_WARN, "*",  "Setting %s +dk (%s): %s", u->handle, tmp, msg);
    simple_sprintf(tmp2, "+dk: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP | USER_KICK;
    fr.chan = USER_DEOP | USER_KICK;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (which == P_DELETE) {
    putlog(LOG_WARN, "*",  "Deleting %s (%s): %s", u->handle, tmp, msg);
    deluser(u->handle);
  } else {
    putlog(LOG_WARN, "*",  "No user flag effects for %s (%s): %s", u->handle, tmp, msg);
    simple_sprintf(tmp2, "Warning: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
  }
}
