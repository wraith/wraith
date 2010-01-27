/*
 * chan.h
 *   stuff common to chan.c and mode.c
 *   users.h needs to be loaded too
 *
 */

#ifndef _EGG_CHAN_H
#define _EGG_CHAN_H

typedef struct memstruct {
  struct memstruct *next;
  struct userrec *user;
  time_t joined;
  time_t split;			/* in case they were just netsplit	*/
  time_t last;			/* for measuring idle time		*/
  interval_t delay;                  /* for delayed autoop                   */
  int hops;
  int tried_getuser;
  unsigned short flags;
  char nick[NICKLEN];
  char userhost[UHOSTLEN];
  char userip[UHOSTLEN];
} memberlist;

#define FLOOD_EXEMPT_OP	1
#define FLOOD_EXEMPT_VOICE 2
#define FLOOD_EXEMPT_USER 3

#define CHANMETA "#&!+"
#define NICKVALID "[{}]^`|\\_-"

#define CHANOP       BIT0 /* channel +o                                   */
#define CHANVOICE    BIT1 /* channel +v                                   */
#define FAKEOP       BIT2 /* op'd by server                               */
#define SENTOP       BIT3 /* a mode +o was already sent out for this user */
#define SENTDEOP     BIT4 /* a mode -o was already sent out for this user */
#define SENTKICK     BIT5 /* a kick was already sent out for this user    */
#define SENTVOICE    BIT6 /* a mode +v was already sent out for this user */
#define SENTDEVOICE  BIT7 /* a devoice has been sent                      */
#define WASOP        BIT8 /* was an op before a split                     */
//#define STOPWHO      BIT9
#define FULL_DELAY   BIT10
#define STOPCHECK    BIT11
#define EVOICE       BIT12 /* keeps people -v */
#define OPER         BIT13

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

#define P_IGNORE	0
#define P_DEOP		1
#define P_KICK		2
#define P_DELETE	3

/* Why duplicate this struct for exempts and invites only under another
 * name? <cybah>
 */
typedef struct maskstruct {
  struct maskstruct *next;
  time_t timer;
  char *mask;
  char *who;
} masklist;

/* Used for temporary bans, exempts and invites */
typedef struct maskrec {
  struct maskrec *next;
  time_t expire,
         added,
         lastactive;
  int flags;
  char *mask,
       *desc,
       *user;
} maskrec;
extern maskrec *global_bans, *global_exempts, *global_invites;
#define MASKREC_STICKY 1
#define MASKREC_PERM   2

/* For every channel i join */
struct chan_t {
  memberlist *member;
  masklist *ban;
  masklist *exempt;
  masklist *invite;
  time_t jointime;
  time_t parttime;
  time_t no_op;
  time_t drone_jointime;
  time_t last_eI;      /* this will stop +e and +I from being checked over and over if the bot is stuck in a
                        * -o+o loop for some reason, hence possibly causing a SENDQ kill
                        */
  int fighting;
  int drone_set_mode;
  int drone_joins;
#ifdef G_BACKUP
  int backup_time;              /* If non-0, set +backup when now>backup_time */
#endif /* G_BACKUP */
  int maxmembers;
  int members;
  int do_opreq;
  char *topic;
  char *key;
  unsigned short int mode;
};

#define CHANINV    BIT0		/* +i					*/
#define CHANPRIV   BIT1		/* +p					*/
#define CHANSEC    BIT2		/* +s					*/
#define CHANMODER  BIT3		/* +m					*/
#define CHANTOPIC  BIT4		/* +t					*/
#define CHANNOMSG  BIT5		/* +n					*/
#define CHANLIMIT  BIT6		/* -l -- used only for protecting modes	*/
#define CHANKEY    BIT7		/* +k					*/
#define CHANANON   BIT8		/* +a -- ircd 2.9			*/
#define CHANQUIET  BIT9		/* +q -- ircd 2.9			*/
#define CHANNOCLR  BIT10	/* +c -- bahamut			*/
#define CHANREGON  BIT11	/* +R -- bahamut			*/
#define CHANMODR   BIT12	/* +M -- bahamut			*/
#define CHANNOCTCP BIT13        /* +C -- QuakeNet's ircu 2.10           */
#define CHANLONLY  BIT14        /* +r -- ircu 2.10.11                   */

#define MODES_PER_LINE_MAX 6

/* For every channel i'm supposed to be active on */
struct chanset_t {
  struct chanset_t *next;
  struct chan_t channel;	/* current information			*/
  maskrec *bans,		/* temporary channel bans		*/
          *exempts,		/* temporary channel exempts		*/
          *invites;		/* temporary channel invites		*/
  time_t added_ts;		/* ..and when? */
  struct {
    char *op;
    int type;
  } cmode[MODES_PER_LINE_MAX];                 /* parameter-type mode changes -        */
  struct {
    char *op;
  } ccmode[MODES_PER_LINE_MAX];                 /* parameter-type mode changes -        */
  /* detect floods */
  time_t floodtime[FLOOD_CHAN_MAX];
  uint32_t status;
  uint32_t ircnet_status;
  int flood_pub_thr;
  interval_t flood_pub_time;
  int flood_join_thr;
  interval_t flood_join_time;
  int flood_deop_thr;
  interval_t flood_deop_time;
  int flood_kick_thr;
  interval_t flood_kick_time;
  int flood_ctcp_thr;
  interval_t flood_ctcp_time;
  int flood_nick_thr;
  interval_t flood_nick_time;
  interval_t flood_lock_time;
  interval_t flood_mjoin_time;
  int flood_mjoin_thr;
  int limitraise;
  int checklimit;
  int closed_ban;
  int closed_private;
  int closed_invite;
  int bad_cookie;
  int manop;
  int mdop;
  int mop;
  int voice_non_ident;
  int ban_type;
  interval_t auto_delay;
  int knock_flags;
/* Chanint template 
 *int temp;
 */
  int flood_exempt_mode;
#ifdef REVENGE
  int revenge_mode;
#endif
  interval_t ban_time;
  interval_t invite_time;
  interval_t exempt_time;

  /* desired channel modes: */
  int mode_pls_prot;		/* modes to enforce			*/
  int mode_mns_prot;		/* modes to reject			*/
  int limit_prot;		/* desired limit			*/
  /* queued mode changes: */
  int limit;			/* new limit to set			*/
  size_t bytes;			/* total bytes so far			*/
  size_t cbytes;
  int compat;			/* to prevent mixing old/new modes	*/
  int floodnum[FLOOD_CHAN_MAX];
  int opreqtime[5];             /* remember when ops was requested */

  char *key;			/* new key to set			*/
  char *rmkey;			/* old key to remove			*/
  char pls[21];			/* positive mode changes		*/
  char mns[21];			/* negative mode changes		*/
  char key_prot[121];		/* desired password			*/
/* Chanchar template
 *char temp[121];
 */
  char topic[121];
  char added_by[HANDLEN + 1];	/* who added the channel? */
  char floodwho[FLOOD_CHAN_MAX][128];
  char deopd[NICKLEN];		/* last person deop'd (must change	*/
  char dname[81];               /* what the users know the channel as like !eggdev */
  char name[81];                /* what the servers know the channel as, like !ABCDEeggdev */
};

/* behavior modes for the channel */

/* Chanflag template 
 * #define CHAN_TEMP           0x0000
 */

#define CHAN_ENFORCEBANS    BIT0	/* kick people who match channel bans */
#define CHAN_DYNAMICBANS    BIT1	/* only activate bans when needed     */
#define CHAN_NOUSERBANS     BIT2	/* don't let non-bots place bans      */
#define CHAN_CLOSED         BIT3	/* Only users +o can join */
#define CHAN_BITCH          BIT4	/* be a tightwad with ops             */
#define CHAN_TAKE 	    BIT5	/* When a bot gets opped, take the chan */
//#define CHAN_KNOCK 	    BIT6	/* Auto invite qualifying people when they knock */
#define CHAN_BOTBITCH       BIT7        /* only let bots be opped? */
#define CHAN_BACKUP         BIT8	/* Join the BOT_BACKUP bots when set */
#define CHAN_SECRET         BIT9	/* don't advertise channel on botnet  */
#define CHAN_RBL            BIT10	/* Lookup users in RBL (requires +r) */
#define CHAN_CYCLE          BIT11	/* cycle the channel if possible      */
#define CHAN_INACTIVE       BIT12	/* no irc support for this channel */
#define CHAN_VOICE          BIT13	/* a bot +y|y will voice *, except +q */
#define CHAN_NOMASSJOIN     BIT14       /* watch for mass join for flood nets and react */
#define CHAN_NODESYNCH      BIT15
#define CHAN_FASTOP         BIT16	/* Bots will not use +o-b to op (no cookies) */ 
#define CHAN_PRIVATE        BIT17	/* users need |o to access chan */ 
#define CHAN_ACTIVE         BIT18	/* like i'm actually on the channel and stuff */
#define CHAN_PEND           BIT19	/* just joined; waiting for end of WHO list */
#define CHAN_FLAGGED        BIT20	/* flagged during rehash for delete   */
#define CHAN_ASKEDBANS      BIT21
#define CHAN_ASKEDMODES     BIT22	/* find out key-info on IRCu          */
#define CHAN_JUPED          BIT23	/* Is channel juped                   */
#define CHAN_STOP_CYCLE     BIT24	/* Some efnetservers have defined NO_CHANOPS_WHEN_SPLIT */
#define CHAN_AUTOOP         BIT25
#define CHAN_JOINING        BIT26	/* attempting to join, dont flood with JOIN #chan */
#define CHAN_MEANKICKS      BIT27	/* use mean/offensive kicks/bans */
#define CHAN_HAVEBANS       BIT28	/* have been opped and received the ban list */

#define CHAN_ASKED_EXEMPTS  BIT0
#define CHAN_ASKED_INVITES  BIT1
#define CHAN_DYNAMICEXEMPTS BIT2
#define CHAN_NOUSEREXEMPTS  BIT3
#define CHAN_DYNAMICINVITES BIT4
#define CHAN_NOUSERINVITES  BIT5
/* prototypes */
memberlist *ismember(struct chanset_t *, const char *);
struct chanset_t *findchan(const char *name);
struct chanset_t *findchan_by_dname(const char *name);

/* is this channel +s/+p? */
#define channel_hidden(chan) (chan->channel.mode & (CHANPRIV | CHANSEC))
/* is this channel +t? */
#define channel_optopic(chan) (chan->channel.mode & CHANTOPIC)

#define channel_active(chan)  (chan->status & CHAN_ACTIVE)
#define channel_joining(chan) (chan->status & CHAN_JOINING)
#define channel_pending(chan)  (chan->status & CHAN_PEND)
#define channel_backup(chan) (chan->status & CHAN_BACKUP)
#define channel_bitch(chan) (chan->status & CHAN_BITCH)
#define chan_bitch(chan) (chan->status & (CHAN_BITCH|CHAN_BOTBITCH))
#define channel_botbitch(chan) (chan->status & CHAN_BOTBITCH)
#define channel_nodesynch(chan) (chan->status & CHAN_NODESYNCH)
#define channel_enforcebans(chan) (chan->status & CHAN_ENFORCEBANS)
#define channel_dynamicbans(chan) (chan->status & CHAN_DYNAMICBANS)
#define channel_nouserbans(chan) (chan->status & CHAN_NOUSERBANS)
#define channel_secret(chan) (chan->status & CHAN_SECRET)
#define channel_cycle(chan) (chan->status & CHAN_CYCLE)
#define channel_inactive(chan) (chan->status & CHAN_INACTIVE)
#define channel_dynamicexempts(chan) (chan->ircnet_status & CHAN_DYNAMICEXEMPTS)
#define channel_nouserexempts(chan) (chan->ircnet_status & CHAN_NOUSEREXEMPTS)
#define channel_dynamicinvites(chan) (chan->ircnet_status & CHAN_DYNAMICINVITES)
#define channel_nouserinvites(chan) (chan->ircnet_status & CHAN_NOUSERINVITES)
#define channel_juped(chan) (chan->status & CHAN_JUPED)
#define channel_stop_cycle(chan) (chan->status & CHAN_STOP_CYCLE)

#ifdef REVENGE
#define channel_revenge(chan) (chan->status & CHAN_REVENGE)
#define channel_revengebot(chan) (chan->status & CHAN_REVENGEBOT)
#endif 

#define channel_closed(chan) (chan->status & CHAN_CLOSED)
#define channel_take(chan) (chan->status & CHAN_TAKE)
#define channel_voice(chan) (chan->status & CHAN_VOICE)
#define channel_fastop(chan) (chan->status & CHAN_FASTOP)
#define channel_privchan(chan) (chan->status & CHAN_PRIVATE)
#define channel_autoop(chan) (chan->status & CHAN_AUTOOP)
#define channel_nomassjoin(chan) (chan->status & CHAN_NOMASSJOIN)
#define channel_meankicks(chan) (chan->status & CHAN_MEANKICKS)
#define channel_rbl(chan) (chan->status & CHAN_RBL)
/* Chanflag template
 *#define channel_temp(chan) (chan->status & CHAN_PRIVATE)
 */

struct msgq_head {
  struct msgq *head;
  struct msgq *last;
  int tot;
  int warned;
};

/* Used to queue a lot of things */
struct msgq {
  struct msgq *next;
  size_t len;
  char *msg;
};

#endif				/* _EGG_CHAN_H */
