#ifndef _EGG_CHAN_H
#define _EGG_CHAN_H
typedef struct memstruct
{
  char nick[NICKLEN];
  char userhost[UHOSTLEN];
  char server[SERVLEN];
  time_t joined;
  unsigned short flags;
  time_t split;
  time_t last;
  time_t delay;
  struct userrec *user;
  int tried_getuser;
  struct memstruct *next;
} memberlist;
#define CHANMETA "#&!+"
#define NICKVALID "[{}]^`|\\_-"
#define CHANOP 0x00001
#define CHANVOICE 0x00002
#define FAKEOP 0x00004
#define SENTOP 0x00008
#define SENTDEOP 0x00010
#define SENTKICK 0x00020
#define SENTVOICE 0x00040
#define SENTDEVOICE 0x00080
#define WASOP 0x00100
#define STOPWHO 0x00200
#define FULL_DELAY 0x00400
#define STOPCHECK 0x00800
#define EVOICE 0x01000
#define OPER 0x02000
#define chan_hasvoice(x) (x->flags & CHANVOICE)
#define chan_hasop(x) (x->flags & CHANOP)
#define chan_fakeop(x) (x->flags & FAKEOP)
#define chan_sentop(x) (x->flags & SENTOP)
#define chan_sentdeop(x) (x->flags & SENTDEOP)
#define chan_sentkick(x) (x->flags & SENTKICK)
#define chan_sentvoice(x) (x->flags & SENTVOICE)
#define chan_sentdevoice(x) (x->flags & SENTDEVOICE)
#define chan_issplit(x) (x->split > 0)
#define chan_wasop(x) (x->flags & WASOP)
#define chan_stopcheck(x) (x->flags & STOPCHECK)
typedef struct maskstruct
{
  char *mask;
  char *who;
  time_t timer;
  struct maskstruct *next;
} masklist;
typedef struct maskrec
{
  struct maskrec *next;
  char *mask, *desc, *user;
  time_t expire, added, lastactive;
  int flags;
} maskrec;
#ifdef S_IRCNET
extern maskrec *global_bans, *global_exempts, *global_invites;
#else
extern maskrec *global_bans;
#endif
#define MASKREC_STICKY 1
#define MASKREC_PERM 2
struct chan_t
{
  memberlist *member;
  masklist *ban;
#ifdef S_IRCNET
  masklist *exempt;
  masklist *invite;
#endif
  char *topic;
  char *key;
  unsigned short int mode;
  int maxmembers;
  int members;
  int do_opreq;
};
#define CHANINV 0x0001
#define CHANPRIV 0x0002
#define CHANSEC 0x0004
#define CHANMODER 0x0008
#define CHANTOPIC 0x0010
#define CHANNOMSG 0x0020
#define CHANLIMIT 0x0040
#define CHANKEY 0x0080
#define CHANANON 0x0100
#define CHANQUIET 0x0200
#define CHANNOCLR 0x0400
#define CHANREGON 0x0800
#define CHANMODR 0x1000
#define CHANNOCTCP 0x2000
#define CHANLONLY 0x4000
#define MODES_PER_LINE_MAX 6
struct chanset_t
{
  struct chanset_t *next;
  struct chan_t channel;
  char dname[81];
  char name[81];
  char need_op[121];
  char need_key[121];
  char need_limit[121];
  char need_unban[121];
  char need_invite[121];
  int flood_pub_thr;
  int flood_pub_time;
  int flood_join_thr;
  int flood_join_time;
  int flood_deop_thr;
  int flood_deop_time;
  int flood_kick_thr;
  int flood_kick_time;
  int flood_ctcp_thr;
  int flood_ctcp_time;
  int flood_nick_thr;
  int flood_nick_time;
  int status;
  int ircnet_status;
  int limitraise;
  int jointime;
  int idle_kick;
  int stopnethack_mode;
  int revenge_mode;
  int ban_time;
#ifdef S_IRCNET
  int invite_time;
  int exempt_time;
  maskrec *bans, *exempts, *invites;
#else
  maskrec *bans;
#endif
  int mode_pls_prot;
  int mode_mns_prot;
  int limit_prot;
  char key_prot[121];
  char topic_prot[501];
  char pls[21];
  char mns[21];
  char *key;
  char *rmkey;
  int limit;
  int bytes;
  int compat;
  struct
  {
    char *target;
  } opqueue[24];
  struct
  {
    char *target;
  } deopqueue[24];
  struct
  {
    char *op;
    int type;
  } cmode[MODES_PER_LINE_MAX];
  char floodwho[FLOOD_CHAN_MAX][81];
  time_t floodtime[FLOOD_CHAN_MAX];
  int floodnum[FLOOD_CHAN_MAX];
  char deopd[NICKLEN];
  int opreqtime[5];
#ifdef G_AUTOLOCK
  int fighting;
#endif
#ifdef G_BACKUP
  int backup_time;
#endif
#ifdef HUB
  char topic[91];
#endif
};
#define CHAN_ENFORCEBANS 0x0001
#define CHAN_DYNAMICBANS 0x0002
#define CHAN_NOUSERBANS 0x0004
#define CHAN_CLOSED 0x0008
#define CHAN_BITCH 0x0010
#define CHAN_TAKE 0x0020
#define CHAN_PROTECTOPS 0x0040
#define CHAN_NOMOP 0x0080
#define CHAN_REVENGE 0x0100
#define CHAN_SECRET 0x0200
#define CHAN_NOMDOP 0x0400
#define CHAN_CYCLE 0x0800
#define CHAN_DONTKICKOPS 0x1000
#define CHAN_INACTIVE 0x2000
#define CHAN_PROTECTFRIENDS 0x4000
#define CHAN_VOICE 0x8000
#define CHAN_SEEN 0x10000
#define CHAN_REVENGEBOT 0x20000
#define CHAN_NODESYNCH 0x40000
#define CHAN_FASTOP 0x80000
#define CHAN_PUNISH 0x100000
#define CHAN_ACTIVE 0x1000000
#define CHAN_PEND 0x2000000
#define CHAN_FLAGGED 0x4000000
#define CHAN_STATIC 0x8000000
#define CHAN_ASKEDBANS 0x10000000
#define CHAN_ASKEDMODES 0x20000000
#define CHAN_JUPED 0x40000000
#define CHAN_STOP_CYCLE 0x80000000
#ifdef S_IRCNET
#define CHAN_ASKED_EXEMPTS 0x0001
#define CHAN_ASKED_INVITED 0x0002
#define CHAN_DYNAMICEXEMPTS 0x0004
#define CHAN_NOUSEREXEMPTS 0x0008
#define CHAN_DYNAMICINVITES 0x0010
#define CHAN_NOUSERINVITES 0x0020
#endif
memberlist *ismember (struct chanset_t *, char *);
struct chanset_t *findchan (const char *name);
struct chanset_t *findchan_by_dname (const char *name);
#define channel_hidden(chan) (chan->channel.mode & (CHANPRIV | CHANSEC))
#define channel_optopic(chan) (chan->channel.mode & CHANTOPIC)
#define channel_active(chan) (chan->status & CHAN_ACTIVE)
#define channel_pending(chan) (chan->status & CHAN_PEND)
#define channel_bitch(chan) (chan->status & CHAN_BITCH)
#define channel_nodesynch(chan) (chan->status & CHAN_NODESYNCH)
#define channel_enforcebans(chan) (chan->status & CHAN_ENFORCEBANS)
#define channel_revenge(chan) (chan->status & CHAN_REVENGE)
#define channel_dynamicbans(chan) (chan->status & CHAN_DYNAMICBANS)
#define channel_nouserbans(chan) (chan->status & CHAN_NOUSERBANS)
#define channel_protectops(chan) (chan->status & CHAN_PROTECTOPS)
#define channel_protectfriends(chan) (chan->status & CHAN_PROTECTFRIENDS)
#define channel_autovoice(chan) (0)
#define channel_dontkickops(chan) (chan->status & CHAN_DONTKICKOPS)
#define channel_secret(chan) (chan->status & CHAN_SECRET)
#define channel_shared(chan) (1)
#define channel_static(chan) (0)
#define channel_cycle(chan) (chan->status & CHAN_CYCLE)
#define channel_seen(chan) (1)
#define channel_inactive(chan) (chan->status & CHAN_INACTIVE)
#define channel_revengebot(chan) (chan->status & CHAN_REVENGEBOT)
#ifdef S_IRCNET
#define channel_dynamicexempts(chan) (chan->ircnet_status & CHAN_DYNAMICEXEMPTS)
#define channel_nouserexempts(chan) (chan->ircnet_status & CHAN_NOUSEREXEMPTS)
#define channel_dynamicinvites(chan) (chan->ircnet_status & CHAN_DYNAMICINVITES)
#define channel_nouserinvites(chan) (chan->ircnet_status & CHAN_NOUSERINVITES)
#endif
#define channel_juped(chan) (chan->status & CHAN_JUPED)
#define channel_stop_cycle(chan) (chan->status & CHAN_STOP_CYCLE)
#define channel_closed(chan) (chan->status & CHAN_CLOSED)
#define channel_take(chan) (chan->status & CHAN_TAKE)
#define channel_nomop(chan) (chan->status & CHAN_NOMOP)
#define channel_nomdop(chan) (chan->status & CHAN_NOMDOP)
#define channel_voice(chan) (chan->status & CHAN_VOICE)
#define channel_fastop(chan) (chan->status & CHAN_FASTOP)
#define channel_punish(chan) (chan->status & CHAN_PUNISH)
struct msgq_head
{
  struct msgq *head;
  struct msgq *last;
  int tot;
  int warned;
};
struct msgq
{
  struct msgq *next;
  int len;
  char *msg;
};
#endif
