/*
 * tclchan.c -- part of channels.mod
 *
 */


static int tcl_channel_info(Tcl_Interp * irp, struct chanset_t *chan)
{
  char a[121], b[121], s[121];
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char *args[2];
#else
  char *args[2];
#endif
  struct udef_struct *ul;

  get_mode_protect(chan, s);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->idle_kick);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->limitraise);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->stopnethack_mode);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->revenge_mode);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_pub_thr, chan->flood_pub_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_ctcp_thr, chan->flood_ctcp_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_join_thr, chan->flood_join_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_kick_thr, chan->flood_kick_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_deop_thr, chan->flood_deop_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d:%d", chan->flood_nick_thr, chan->flood_nick_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->ban_time);
  Tcl_AppendElement(irp, s);
/* Chanint template 
 *simple_sprintf(s, "%s", chan->temp);
 *Tcl_AppendElement(irp, s);
 */
#ifdef S_IRCNET
  simple_sprintf(s, "%d", chan->exempt_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->invite_time);
  Tcl_AppendElement(irp, s);
#endif
  if (chan->status & CHAN_ENFORCEBANS)
    Tcl_AppendElement(irp, "+enforcebans");
  else
    Tcl_AppendElement(irp, "-enforcebans");
  if (chan->status & CHAN_DYNAMICBANS)
    Tcl_AppendElement(irp, "+dynamicbans");
  else
    Tcl_AppendElement(irp, "-dynamicbans");
  if (chan->status & CHAN_NOUSERBANS)
    Tcl_AppendElement(irp, "-userbans");
  else
    Tcl_AppendElement(irp, "+userbans");
  if (chan->status & CHAN_BITCH)
    Tcl_AppendElement(irp, "+bitch");
  else
    Tcl_AppendElement(irp, "-bitch");
  if (chan->status & CHAN_PROTECTOPS)
    Tcl_AppendElement(irp, "+protectops");
  else
    Tcl_AppendElement(irp, "-protectops");
  if (chan->status & CHAN_DONTKICKOPS)
    Tcl_AppendElement(irp, "+dontkickops");
  else
    Tcl_AppendElement(irp, "-dontkickops");
  if (chan->status & CHAN_INACTIVE)
    Tcl_AppendElement(irp, "+inactive");
  else
    Tcl_AppendElement(irp, "-inactive");
  if (chan->status & CHAN_REVENGE)
    Tcl_AppendElement(irp, "+revenge");
  else
    Tcl_AppendElement(irp, "-revenge");
  if (chan->status & CHAN_REVENGEBOT)
    Tcl_AppendElement(irp, "+revengebot");
  else
    Tcl_AppendElement(irp, "-revengebot");
  if (chan->status & CHAN_SECRET)
    Tcl_AppendElement(irp, "+secret");
  else
    Tcl_AppendElement(irp, "-secret");
  if (chan->status & CHAN_CYCLE)
    Tcl_AppendElement(irp, "+cycle");
  else
    Tcl_AppendElement(irp, "-cycle");
#ifdef S_IRCNET
  if (chan->ircnet_status& CHAN_DYNAMICEXEMPTS)
    Tcl_AppendElement(irp, "+dynamicexempts");
  else
    Tcl_AppendElement(irp, "-dynamicexempts");
  if (chan->ircnet_status& CHAN_NOUSEREXEMPTS)
    Tcl_AppendElement(irp, "-userexempts");
  else
    Tcl_AppendElement(irp, "+userexempts");
  if (chan->ircnet_status& CHAN_DYNAMICINVITES)
    Tcl_AppendElement(irp, "+dynamicinvites");
  else
    Tcl_AppendElement(irp, "-dynamicinvites");
  if (chan->ircnet_status& CHAN_NOUSERINVITES)
    Tcl_AppendElement(irp, "-userinvites");
  else
    Tcl_AppendElement(irp, "+userinvites");
#endif
  if (chan->status & CHAN_NODESYNCH)
    Tcl_AppendElement(irp, "+nodesynch");
  else
    Tcl_AppendElement(irp, "-nodesynch");
  if (chan->status & CHAN_CLOSED)
    Tcl_AppendElement(irp, "+closed");
  else
    Tcl_AppendElement(irp, "-closed");
  if (chan->status & CHAN_TAKE)
    Tcl_AppendElement(irp, "+take");
  else
    Tcl_AppendElement(irp, "-take");
  if (chan->status & CHAN_NOMOP)
    Tcl_AppendElement(irp, "+nomop");
  else
    Tcl_AppendElement(irp, "-nomop");
  if (chan->status & CHAN_MANOP)
    Tcl_AppendElement(irp, "+manop");
  else
    Tcl_AppendElement(irp, "-manop");
  if (chan->status & CHAN_VOICE)
    Tcl_AppendElement(irp, "+voice");
  else
    Tcl_AppendElement(irp, "-voice");
/* Chanflag template
 *if (chan->status & CHAN_TEMP)
 *  Tcl_AppendElement(irp, "+temp");
 *else
 *  Tcl_AppendElement(irp, "-temp");
 */
  if (chan->status & CHAN_FASTOP)
    Tcl_AppendElement(irp, "+fastop");
  else
    Tcl_AppendElement(irp, "-fastop");
  if (chan->status & CHAN_PRIVATE)
    Tcl_AppendElement(irp, "+private");
  else
    Tcl_AppendElement(irp, "-private");

  for (ul = udef; ul; ul = ul->next) {
      /* If it's undefined, skip it. */
      if (!ul->defined || !ul->name) continue;

      if (ul->type == UDEF_FLAG) {
        simple_sprintf(s, "%c%s", getudef(ul->values, chan->dname) ? '+' : '-',
		       ul->name);
        Tcl_AppendElement(irp, s);
      } else if (ul->type == UDEF_INT) {
        char *x;
        egg_snprintf(a, sizeof a, "%s", ul->name);
        egg_snprintf(b, sizeof b, "%d", getudef(ul->values, chan->dname));
        args[0] = a;
        args[1] = b;
        x = Tcl_Merge(2, args);
        egg_snprintf(s, sizeof s, "%s", x);
        Tcl_Free((char *) x);
        Tcl_AppendElement(irp, s);
      } else
        debug1("UDEF-ERROR: unknown type %d", ul->type);
    }
  return TCL_OK;
}

static int tcl_channel_get(Tcl_Interp * irp, struct chanset_t *chan, char *setting)
{
  char s[121];
  struct udef_struct *ul;

#define CHECK(x) !strcmp(setting, x)

#define CHKFLAG_POS(x,y,z) (!strcmp(setting, y)) { \
                            if(z & x) simple_sprintf(s, "%d", 1); \
                            else simple_sprintf(s, "%d", 0); }

#define CHKFLAG_NEG(x,y,z) (!strcmp(setting, y)) { \
                            if (z & x) simple_sprintf(s, "%d", 0); \
                            else simple_sprintf(s, "%d", 1); }

  if      (CHECK("chanmode"))      get_mode_protect(chan, s);

  else if (CHECK("idle-kick"))     simple_sprintf(s, "%d", chan->idle_kick);
  else if (CHECK("limit"))         simple_sprintf(s, "%d", chan->limitraise);
  else if (CHECK("stop-net-hack")) simple_sprintf(s, "%d", chan->stopnethack_mode);
  else if (CHECK("revenge-mode"))  simple_sprintf(s, "%d", chan->revenge_mode);
  else if (CHECK("flood-chan"))    simple_sprintf(s, "%d %d", chan->flood_pub_thr, chan->flood_pub_time);
  else if (CHECK("flood-ctcp"))    simple_sprintf(s, "%d %d", chan->flood_ctcp_thr, chan->flood_ctcp_time);
  else if (CHECK("flood-join"))    simple_sprintf(s, "%d %d", chan->flood_join_thr, chan->flood_join_time);
  else if (CHECK("flood-kick"))    simple_sprintf(s, "%d %d", chan->flood_kick_thr, chan->flood_kick_time);
  else if (CHECK("flood-deop"))    simple_sprintf(s, "%d %d", chan->flood_deop_thr, chan->flood_deop_time);
  else if (CHECK("flood-nick"))    simple_sprintf(s, "%d %d", chan->flood_nick_thr, chan->flood_nick_time);
/* Chanint template
 *else if (CHECK("temp"))	   simple_sprintf(s, "%s", chan->temp);
 */
  else if (CHECK("ban-time"))  	   simple_sprintf(s, "%d", chan->ban_time);
#ifdef S_IRCNET
  else if (CHECK("exempt-time"))   simple_sprintf(s, "%d", chan->exempt_time);
  else if (CHECK("invite-time"))   simple_sprintf(s, "%d", chan->invite_time);
#endif
  else if CHKFLAG_POS(CHAN_ENFORCEBANS,    "enforcebans",    chan->status)
  else if CHKFLAG_POS(CHAN_DYNAMICBANS,    "dynamicbans",    chan->status)
  else if CHKFLAG_NEG(CHAN_NOUSERBANS,     "userbans",       chan->status)
  else if CHKFLAG_POS(CHAN_BITCH,          "bitch",          chan->status)
  else if CHKFLAG_POS(CHAN_PROTECTOPS,     "protectops",     chan->status)
  else if CHKFLAG_POS(CHAN_DONTKICKOPS,    "dontkickops",    chan->status)
  else if CHKFLAG_POS(CHAN_INACTIVE,       "inactive",       chan->status)
  else if CHKFLAG_POS(CHAN_REVENGE,        "revenge",        chan->status)
  else if CHKFLAG_POS(CHAN_REVENGEBOT,     "revengebot",     chan->status)
  else if CHKFLAG_POS(CHAN_SECRET,         "secret",         chan->status)
  else if CHKFLAG_POS(CHAN_CYCLE,          "cycle",          chan->status)
  else if CHKFLAG_POS(CHAN_NODESYNCH,      "nodesynch",      chan->status)
#ifdef S_IRCNET
  else if CHKFLAG_POS(CHAN_DYNAMICEXEMPTS, "dynamicexempts", chan->ircnet_status)
  else if CHKFLAG_NEG(CHAN_NOUSEREXEMPTS,  "userexempts",    chan->ircnet_status)
  else if CHKFLAG_POS(CHAN_DYNAMICINVITES, "dynamicinvites", chan->ircnet_status)
  else if CHKFLAG_NEG(CHAN_NOUSERINVITES,  "userinvites",    chan->ircnet_status)
#endif
  else if CHKFLAG_POS(CHAN_CLOSED,	   "closed",         chan->status)
  else if CHKFLAG_POS(CHAN_TAKE,	   "take",           chan->status)
  else if CHKFLAG_POS(CHAN_NOMOP,	   "nomop",          chan->status)
  else if CHKFLAG_POS(CHAN_MANOP,          "manop",          chan->status)
  else if CHKFLAG_POS(CHAN_VOICE,          "voice",          chan->status)
  else if CHKFLAG_POS(CHAN_FASTOP,         "fastop",         chan->status)
  else if CHKFLAG_POS(CHAN_PRIVATE,        "private",        chan->status)
/* Chanflag template
 *else if CHKFLAG_POS(CHAN_TEMP,	   "temp",		chan->status)
 */

  else {
    /* Hopefully it's a user-defined flag. */
    for (ul = udef; ul && ul->name; ul = ul->next) {
      if (!strcmp(setting, ul->name)) break;
    }
    if (!ul || !ul->name) {
      /* Error if it wasn't found. */
      Tcl_AppendResult(irp, "Unknown channel setting.", NULL);
      return(TCL_ERROR);
    }

    /* Flag or int, all the same. */
    simple_sprintf(s, "%d", getudef(ul->values, chan->dname));
    Tcl_AppendResult(irp, s, NULL);
    return(TCL_OK);
  }

  /* Ok, if we make it this far, the result is "s". */
  Tcl_AppendResult(irp, s, NULL);
  return(TCL_OK);
}

static int tcl_channel STDVAR
{
  struct chanset_t *chan;
  char buf2[1024];
  BADARGS(2, 999, " command ?options?");
  if (!strcmp(argv[1], "add")) {
    BADARGS(3, 4, " add channel-name ?options-list?");
    if (argc == 3) {
      
      snprintf(buf2, sizeof buf2, "cjoin %s", argv[2]);
      if (!loading)
        putallbots(buf2);
      return tcl_channel_add(irp, argv[2], "");
    }
    snprintf(buf2, sizeof buf2, "cjoin %s %s", argv[2], argv[3]);
    if (!loading)
      putallbots(buf2);
    return tcl_channel_add(irp, argv[2], argv[3]);
  }
  if (!strcmp(argv[1], "set")) {
    BADARGS(3, 999, " set channel-name ?options?");
    chan = findchan_by_dname(argv[2]);
    if (chan == NULL) {
      if (loading == 1)
	return TCL_OK;		/* Ignore channel settings for a static
				 * channel which has been removed from
				 * the config */
      Tcl_AppendResult(irp, "no such channel record", NULL);
      return TCL_ERROR;
    }
Context;
    do_chanset(chan, argv[3], 0);
Context;
    return tcl_channel_modify(irp, chan, argc - 3, &argv[3]);
  }
  if (!strcmp(argv[1], "get")) {
    BADARGS(4, 4, " get channel-name setting-name");
    chan = findchan_by_dname(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, "no such channel record", NULL);
      return TCL_ERROR;
    }
    return(tcl_channel_get(irp, chan, argv[3]));
  }
  if (!strcmp(argv[1], "info")) {
    BADARGS(3, 3, " info channel-name");
    chan = findchan_by_dname(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, "no such channel record", NULL);
      return TCL_ERROR;
    }
    return tcl_channel_info(irp, chan);
  }
  if (!strcmp(argv[1], "remove")) {
    BADARGS(3, 3, " remove channel-name");
    chan = findchan_by_dname(argv[2]);

    if (chan == NULL) {
      Tcl_AppendResult(irp, "no such channel record", NULL);
      return TCL_ERROR;
    }
    snprintf(buf2, sizeof buf2, "cpart %s", argv[2]);
    putallbots(buf2);
    remove_channel(chan);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, "unknown channel command: should be one of: ",
		   "add, set, get, info, remove", NULL);
  return TCL_ERROR;
}

/* Parse options for a channel.
 */
static int tcl_channel_modify(Tcl_Interp * irp, struct chanset_t *chan,
			      int items, char **item)
{
  int i, x = 0, found;
#ifdef LEAF
  int old_status = chan->status,
      old_mode_mns_prot = chan->mode_mns_prot,
      old_mode_pls_prot = chan->mode_pls_prot;
  module_entry *me;
#endif /* LEAF */
  struct udef_struct *ul = udef;
  char s[121];
Context;
  for (i = 0; i < items; i++) {
    if (!strcmp(item[i], "chanmode")) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, "channel chanmode needs argument", NULL);
	return TCL_ERROR;
      }
      strncpy(s, item[i], 120);
      s[120] = 0;
      set_mode_protect(chan, s);
    } else if (!strcmp(item[i], "idle-kick")) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, "channel idle-kick needs argument", NULL);
	return TCL_ERROR;
      }
      chan->idle_kick = atoi(item[i]);
    } else if (!strcmp(item[i], "limit")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel limit needs argument", NULL);
        return TCL_ERROR;
      }
      chan->limitraise = atoi(item[i]);
      chan->limit_prot = 0;
    } else if (!strcmp(item[i], "dont-idle-kick"))
      chan->idle_kick = 0;
    else if (!strcmp(item[i], "stopnethack-mode")) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, "channel stopnethack-mode needs argument", NULL);
	return TCL_ERROR;
      }
      chan->stopnethack_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "revenge-mode")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel revenge-mode needs argument", NULL);
        return TCL_ERROR;
      }
      chan->revenge_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "ban-time")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel ban-time needs argument", NULL);
        return TCL_ERROR;
      }
      chan->ban_time = atoi(item[i]);
#ifdef S_IRCNET
    } else if (!strcmp(item[i], "exempt-time")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel exempt-time needs argument", NULL);
        return TCL_ERROR;
      }
      chan->exempt_time = atoi(item[i]);
    } else if (!strcmp(item[i], "invite-time")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel invite-time needs argument", NULL);
        return TCL_ERROR;
      }
      chan->invite_time = atoi(item[i]);
#endif
/* Chanint template
 *  } else if (!strcmp(item[i], "temp")) {
 *    i++;
 *    if (i >= items) {
 *      if (irp)
 *        Tcl_AppendResult(irp, "channel temp needs argument", NULL);
 *      return TCL_ERROR;
 *    }
 *    chan->temp = atoi(item[i]);
 */
/* Chanchar template
    } else if (!strcmp(item[i], "temp")) {
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel temp needs argument", NULL);
        return TCL_ERROR;
      }
      strncpyz(chan->temp, item[i], sizeof(chan->temp));
      //Entry just changed so update/recheck it's purpose?
      check_temp(chan);
 */
    } else if (!strcmp(item[i], "topic")) { //this is here for compatability
      i++;
      if (i >= items) {
        if (irp)
          Tcl_AppendResult(irp, "channel topic needs argument", NULL);
        return TCL_ERROR;
      }
    }
    else if (!strcmp(item[i], "+enforcebans"))
      chan->status |= CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], "-enforcebans"))
      chan->status &= ~CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], "+dynamicbans"))
      chan->status |= CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], "-dynamicbans"))
      chan->status &= ~CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], "-userbans"))
      chan->status |= CHAN_NOUSERBANS;
    else if (!strcmp(item[i], "+userbans"))
      chan->status &= ~CHAN_NOUSERBANS;
    else if (!strcmp(item[i], "+bitch"))
      chan->status |= CHAN_BITCH;
    else if (!strcmp(item[i], "-bitch"))
      chan->status &= ~CHAN_BITCH;
    else if (!strcmp(item[i], "+nodesynch"))
      chan->status |= CHAN_NODESYNCH;
    else if (!strcmp(item[i], "-nodesynch"))
      chan->status &= ~CHAN_NODESYNCH;
    else if (!strcmp(item[i], "+protectops"))
      chan->status |= CHAN_PROTECTOPS;
    else if (!strcmp(item[i], "-protectops"))
      chan->status &= ~CHAN_PROTECTOPS;
    else if (!strcmp(item[i], "+dontkickops"))
      chan->status |= CHAN_DONTKICKOPS;
    else if (!strcmp(item[i], "-dontkickops"))
      chan->status &= ~CHAN_DONTKICKOPS;
    else if (!strcmp(item[i], "+inactive"))
      chan->status |= CHAN_INACTIVE;
    else if (!strcmp(item[i], "-inactive"))
      chan->status&= ~CHAN_INACTIVE;
    else if (!strcmp(item[i], "+revenge"))
      chan->status |= CHAN_REVENGE;
    else if (!strcmp(item[i], "-revenge"))
      chan->status &= ~CHAN_REVENGE;
    else if (!strcmp(item[i], "+revengebot"))
      chan->status |= CHAN_REVENGEBOT;
    else if (!strcmp(item[i], "-revengebot"))
      chan->status &= ~CHAN_REVENGEBOT;
    else if (!strcmp(item[i], "+secret"))
      chan->status |= CHAN_SECRET;
    else if (!strcmp(item[i], "-secret"))
      chan->status &= ~CHAN_SECRET;
    else if (!strcmp(item[i], "+cycle"))
      chan->status |= CHAN_CYCLE;
    else if (!strcmp(item[i], "-cycle"))
      chan->status &= ~CHAN_CYCLE;
#ifdef S_IRCNET
    else if (!strcmp(item[i], "+dynamicexempts"))
      chan->ircnet_status|= CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-dynamicexempts"))
      chan->ircnet_status&= ~CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-userexempts"))
      chan->ircnet_status|= CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+userexempts"))
      chan->ircnet_status&= ~CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+dynamicinvites"))
      chan->ircnet_status|= CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-dynamicinvites"))
      chan->ircnet_status&= ~CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-userinvites"))
      chan->ircnet_status|= CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], "+userinvites"))
      chan->ircnet_status&= ~CHAN_NOUSERINVITES;
#endif
    else if (!strcmp(item[i], "+closed"))
      chan->status |= CHAN_CLOSED;
    else if (!strcmp(item[i], "-closed"))
      chan->status &= ~CHAN_CLOSED;
    else if (!strcmp(item[i], "+take"))
      chan->status |= CHAN_TAKE;
    else if (!strcmp(item[i], "-take"))
      chan->status &= ~CHAN_TAKE;
    else if (!strcmp(item[i], "+nomop"))
      chan->status |= CHAN_NOMOP;
    else if (!strcmp(item[i], "-nomop"))
      chan->status &= ~CHAN_NOMOP;
    else if (!strcmp(item[i], "+manop"))
      chan->status |= CHAN_MANOP;
    else if (!strcmp(item[i], "-manop"))
      chan->status &= ~CHAN_MANOP;
    else if (!strcmp(item[i], "+voice"))
      chan->status |= CHAN_VOICE;
    else if (!strcmp(item[i], "-voice"))
      chan->status &= ~CHAN_VOICE;
/* Chanflag template
 *  else if (!strcmp(item[i], "+temp"))
 *    chan->status |= CHAN_TEMP;
 *  else if (!strcmp(item[i], "-temp"))
 *    chan->status &= ~CHAN_TEMP;
 */
    else if (!strcmp(item[i], "+fastop")) {
      chan->status |= CHAN_FASTOP;
    }
    else if (!strcmp(item[i], "-fastop"))
      chan->status &= ~(CHAN_FASTOP | CHAN_PROTECTOPS | CHAN_DONTKICKOPS);
    else if (!strcmp(item[i], "+private")) {
      chan->status |= CHAN_PRIVATE;
    }
    else if (!strcmp(item[i], "-private"))
      chan->status &= ~CHAN_PRIVATE;

    /* ignore wasoptest, stopnethack and clearbans in chanfile, remove
       this later */
    else if (!strcmp(item[i], "+nomdop"))  ;
    else if (!strcmp(item[i], "-nomdop"))  ;
    else if (!strcmp(item[i], "+nomop"))  ;
    else if (!strcmp(item[i], "-nomop"))  ;
    else if (!strcmp(item[i], "+protectfriends"))  ;
    else if (!strcmp(item[i], "-protectfriends"))  ;
    else if (!strcmp(item[i], "+punish"))  ;
    else if (!strcmp(item[i], "-punish"))  ;
    else if (!strcmp(item[i], "+seen"))  ;
    else if (!strcmp(item[i], "-seen"))  ;
    else if (!strcmp(item[i], "+secret"))  ;
    else if (!strcmp(item[i], "-secret"))  ;
      else if (!strcmp(item[i], "-stopnethack"))  ;
    else if (!strcmp(item[i], "+stopnethack"))  ;
    else if (!strcmp(item[i], "-wasoptest"))  ;
    else if (!strcmp(item[i], "+wasoptest"))  ;  /* Eule 01.2000 */
    else if (!strcmp(item[i], "+clearbans"))  ;
    else if (!strcmp(item[i], "-clearbans"))  ;
    else if (!strncmp(item[i], "need-", 5))   ;
    else if (!strncmp(item[i], "flood-", 6)) {
      int *pthr = 0, *ptime;
      char *p;

      if (!strcmp(item[i] + 6, "chan")) {
	pthr = &chan->flood_pub_thr;
	ptime = &chan->flood_pub_time;
      } else if (!strcmp(item[i] + 6, "join")) {
	pthr = &chan->flood_join_thr;
	ptime = &chan->flood_join_time;
      } else if (!strcmp(item[i] + 6, "ctcp")) {
	pthr = &chan->flood_ctcp_thr;
	ptime = &chan->flood_ctcp_time;
      } else if (!strcmp(item[i] + 6, "kick")) {
	pthr = &chan->flood_kick_thr;
	ptime = &chan->flood_kick_time;
      } else if (!strcmp(item[i] + 6, "deop")) {
	pthr = &chan->flood_deop_thr;
	ptime = &chan->flood_deop_time;
      } else if (!strcmp(item[i] + 6, "nick")) {
	pthr = &chan->flood_nick_thr;
	ptime = &chan->flood_nick_time;
      } else {
	if (irp)
	  Tcl_AppendResult(irp, "illegal channel flood type: ", item[i], NULL);
	return TCL_ERROR;
      }
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, item[i - 1], " needs argument", NULL);
	return TCL_ERROR;
      }
      p = strchr(item[i], ':');
      if (p) {
	*p++ = 0;
	*pthr = atoi(item[i]);
	*ptime = atoi(p);
	*--p = ':';
      } else {
	*pthr = atoi(item[i]);
	*ptime = 1;
      }
    } else {
      if (!strncmp(item[i] + 1, "udef-flag-", 10))
        initudef(UDEF_FLAG, item[i] + 11, 0);
      else if (!strncmp(item[i], "udef-int-", 9))
        initudef(UDEF_INT, item[i] + 9, 0);
      found = 0;
      for (ul = udef; ul; ul = ul->next) {
        if (ul->type == UDEF_FLAG &&
	     /* Direct match when set during .chanset ... */
	    (!egg_strcasecmp(item[i] + 1, ul->name) ||
	     /* ... or with prefix when set during chanfile load. */
	     (!strncmp(item[i] + 1, "udef-flag-", 10) &&
	      !egg_strcasecmp(item[i] + 11, ul->name)))) {
          if (item[i][0] == '+')
            setudef(ul, chan->dname, 1);
          else
            setudef(ul, chan->dname, 0);
          found = 1;
	  break;
        } else if (ul->type == UDEF_INT &&
		    /* Direct match when set during .chanset ... */
		   (!egg_strcasecmp(item[i], ul->name) ||
		    /* ... or with prefix when set during chanfile load. */
		    (!strncmp(item[i], "udef-int-", 9) &&
		     !egg_strcasecmp(item[i] + 9, ul->name)))) {
          i++;
          if (i >= items) {
            if (irp)
              Tcl_AppendResult(irp, "this setting needs an argument", NULL);
            return TCL_ERROR;
          }
          setudef(ul, chan->dname, atoi(item[i]));
          found = 1;
	  break;
        }
      }
      if (!found) {
        if (irp && item[i][0]) /* ignore "" */
      	  Tcl_AppendResult(irp, "illegal channel option: ", item[i], NULL);
      	x++;
      }
    }
  }
  /* If protect_readonly == 0 and loading == 0 then
   * bot is now processing the configfile, so dont do anything,
   * we've to wait the channelfile that maybe override these settings
   * (note: it may cause problems if there is no chanfile!)
   * <drummer/1999/10/21>
   */
#ifdef LEAF
  if (protect_readonly || loading) {
    if (((old_status ^ chan->status) & CHAN_INACTIVE) &&
	module_find("irc", 0, 0)) {
      if (!shouldjoin(chan) &&
	  (chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, "PART %s\n", chan->name);
      if (shouldjoin(chan) &&
	  !(chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, "JOIN %s %s\n", (chan->name[0]) ?
					   chan->name : chan->dname,
					   chan->channel.key[0] ?
					   chan->channel.key : chan->key_prot);
    }
Context;
    if ((old_status ^ chan->status) & (CHAN_ENFORCEBANS |
	CHAN_BITCH)) {
      if ((me = module_find("irc", 0, 0)))
        (me->funcs[IRC_RECHECK_CHANNEL])(chan, 1);
    } else if (old_mode_pls_prot != chan->mode_pls_prot ||
	       old_mode_mns_prot != chan->mode_mns_prot)
    if ((me = module_find("irc", 0, 0)))
      (me->funcs[IRC_RECHECK_CHANNEL_MODES])(chan);
  }
Context;
#endif /* LEAF */
  if (x > 0)
    return TCL_ERROR;
  return TCL_OK;
}

static int tcl_do_masklist(maskrec *m, Tcl_Interp *irp)
{
  char ts[21], ts1[21], ts2[21], *p;
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char *list[6];
#else
  char *list[6];
#endif

  for (; m; m = m->next) {
    list[0] = m->mask;
    list[1] = m->desc;
    sprintf(ts, "%lu", m->expire);
    list[2] = ts;
    sprintf(ts1, "%lu", m->added);
    list[3] = ts1;
    sprintf(ts2, "%lu", m->lastactive);
    list[4] = ts2;
    list[5] = m->user;
    p = Tcl_Merge(6, list);
    Tcl_AppendElement(irp, p);
    Tcl_Free((char *) p);
  }
  return TCL_OK;
}

static int tcl_banlist STDVAR
{
  struct chanset_t *chan;

  BADARGS(1, 2, " ?channel?");
  if (argc == 2) {
    chan = findchan_by_dname(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->bans, irp);
  }

  return tcl_do_masklist(global_bans, irp);
}

static int tcl_exemptlist STDVAR
{
  struct chanset_t *chan;

  BADARGS(1, 2, " ?channel?");
  if (argc == 2) {
    chan = findchan_by_dname(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->exempts, irp);
  }

  return tcl_do_masklist(global_exempts, irp);
}

static int tcl_invitelist STDVAR
{
  struct chanset_t *chan;

  BADARGS(1, 2, " ?channel?");
  if (argc == 2) {
    chan = findchan_by_dname(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->invites, irp);
  }
  return tcl_do_masklist(global_invites, irp);
}

static int tcl_channels STDVAR
{
  struct chanset_t *chan;

  BADARGS(1, 1, "");
  for (chan = chanset; chan; chan = chan->next) 
    Tcl_AppendElement(irp, chan->dname);
  return TCL_OK;
}

static int tcl_validchan STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL)
    Tcl_AppendResult(irp, "0", NULL);
  else
    Tcl_AppendResult(irp, "1", NULL);
  return TCL_OK;
}

static int tcl_getchaninfo STDVAR
{
  char s[161];
  struct userrec *u;

  BADARGS(3, 3, " handle channel");
  u = get_user_by_handle(userlist, argv[1]);
  if (!u || (u->flags & USER_BOT))
    return TCL_OK;
  get_handle_chaninfo(argv[1], argv[2], s);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

static int tcl_setchaninfo STDVAR
{
  struct chanset_t *chan;

  BADARGS(4, 4, " handle channel info");
  chan = findchan_by_dname(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "illegal channel: ", argv[2], NULL);
    return TCL_ERROR;
  }
  if (!egg_strcasecmp(argv[3], "none")) {
    set_handle_chaninfo(userlist, argv[1], argv[2], NULL);
    return TCL_OK;
  }
  set_handle_chaninfo(userlist, argv[1], argv[2], argv[3]);
  return TCL_OK;
}

static void init_masklist(masklist *m)
{
  m->mask = (char *)nmalloc(1);
  m->mask[0] = 0;
  m->who = NULL;
  m->next = NULL;
}

/* Initialize out the channel record.
 */
static void init_channel(struct chanset_t *chan, int reset)
{
  chan->channel.maxmembers = 0;
  chan->channel.mode = 0;
  chan->channel.members = 0;
  if (!reset) {
    chan->channel.key = (char *) nmalloc(1);
    chan->channel.key[0] = 0;
  }

  chan->channel.ban = (masklist *) nmalloc(sizeof(masklist));
  init_masklist(chan->channel.ban);

  chan->channel.exempt = (masklist *) nmalloc(sizeof(masklist));
  init_masklist(chan->channel.exempt);

  chan->channel.invite = (masklist *) nmalloc(sizeof(masklist));
  init_masklist(chan->channel.invite);

  chan->channel.member = (memberlist *) nmalloc(sizeof(memberlist));
  chan->channel.member->nick[0] = 0;
  chan->channel.member->next = NULL;
  chan->channel.topic = NULL;
}

static void clear_masklist(masklist *m)
{
  masklist *temp;

  for (; m; m = temp) {
    temp = m->next;
    if (m->mask)
      nfree(m->mask);
    if (m->who)
      nfree(m->who);
    nfree(m);
  }
}

/* Clear out channel data from memory.
 */
static void clear_channel(struct chanset_t *chan, int reset)
{
  memberlist *m, *m1;

  if (chan->channel.topic)
    nfree(chan->channel.topic);
  for (m = chan->channel.member; m; m = m1) {
    m1 = m->next;
    nfree(m);
  }

  clear_masklist(chan->channel.ban);
  chan->channel.ban = NULL;
  clear_masklist(chan->channel.exempt);
  chan->channel.exempt = NULL;
  clear_masklist(chan->channel.invite);
  chan->channel.invite = NULL;

  if (reset)
    init_channel(chan, 1);
}

/* Create new channel and parse commands.
 */
static int tcl_channel_add(Tcl_Interp *irp, char *newname, char *options)
{
  struct chanset_t *chan;
  int items;
  int ret = TCL_OK;
  int join = 0;
  char buf[2048], buf2[256];
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char **item;
#else
  char **item;
#endif

  if (!newname || !newname[0] || !strchr(CHANMETA, newname[0])) {
    if (irp)
      Tcl_AppendResult(irp, "invalid channel prefix", NULL);
    return TCL_ERROR;
  }

  if (strchr(newname, ',') != NULL) {
    if (irp)
      Tcl_AppendResult(irp, "invalid channel name", NULL);
    return TCL_ERROR;
  }

  convert_element(glob_chanmode, buf2);
  simple_sprintf(buf, "chanmode %s ", buf2);
  strncat(buf, glob_chanset, 2047 - strlen(buf));
  strncat(buf, options, 2047 - strlen(buf));
  buf[2047] = 0;

  if (Tcl_SplitList(NULL, buf, &items, &item) != TCL_OK)
    return TCL_ERROR;
  if ((chan = findchan_by_dname(newname))) {
    /* Already existing channel, maybe a reload of the channel file */
    chan->status &= ~CHAN_FLAGGED;	/* don't delete me! :) */
  } else {
    chan = (struct chanset_t *) nmalloc(sizeof(struct chanset_t));

    /* Hells bells, why set *every* variable to 0 when we have bzero? */
    egg_bzero(chan, sizeof(struct chanset_t));

    chan->limit_prot = 0;
    chan->limit = 0;
/* Chanint template
 *  chan->temp = 0;
 */
    chan->flood_pub_thr = gfld_chan_thr;
    chan->flood_pub_time = gfld_chan_time;
    chan->flood_ctcp_thr = gfld_ctcp_thr;
    chan->flood_ctcp_time = gfld_ctcp_time;
    chan->flood_join_thr = gfld_join_thr;
    chan->flood_join_time = gfld_join_time;
    chan->flood_deop_thr = gfld_deop_thr;
    chan->flood_deop_time = gfld_deop_time;
    chan->flood_kick_thr = gfld_kick_thr;
    chan->flood_kick_time = gfld_kick_time;
    chan->flood_nick_thr = gfld_nick_thr;
    chan->flood_nick_time = gfld_nick_time;
    chan->stopnethack_mode = global_stopnethack_mode;
    chan->revenge_mode = global_revenge_mode;
    chan->idle_kick = global_idle_kick;
    chan->limitraise = 20;
    chan->ban_time = global_ban_time;
    chan->exempt_time = global_exempt_time;
    chan->invite_time = global_invite_time;
    /* let's initialize this stuff for shits & giggles */
    chan->channel.jointime = 0;
    chan->channel.parttime = 0;
#ifdef S_AUTOLOCK
    chan->channel.fighting = 0;
#endif /* S_AUTOLOCK */

    /* We _only_ put the dname (display name) in here so as not to confuse
     * any code later on. chan->name gets updated with the channel name as
     * the server knows it, when we join the channel. <cybah>
     */
    strncpy(chan->dname, newname, 81);
    chan->dname[80] = 0;

    /* Initialize chan->channel info */
    init_channel(chan, 0);
    list_append((struct list_type **) &chanset, (struct list_type *) chan);
    /* Channel name is stored in xtra field for sharebot stuff */
    join = 1;
  }
  /* If loading is set, we're loading the userfile. Ignore errors while
   * reading userfile and just return TCL_OK. This is for compatability
   * if a user goes back to an eggdrop that no-longer supports certain
   * (channel) options.
   */
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  if ((tcl_channel_modify(irp, chan, items, (char **)item) != TCL_OK) && !loading) {
#else
  if ((tcl_channel_modify(irp, chan, items, item) != TCL_OK) && !loading) {
#endif
    ret = TCL_ERROR;
  }
  Tcl_Free((char *) item);
#ifdef LEAF
  if (join && shouldjoin(chan) && module_find("irc", 0, 0))
    dprintf(DP_SERVER, "JOIN %s %s\n", chan->dname, chan->key_prot);
#endif
  return ret;
}

static int tcl_setudef STDVAR
{
  int type;

  BADARGS(3, 3, " type name");
  if (!egg_strcasecmp(argv[1], "flag"))
    type = UDEF_FLAG;
  else if (!egg_strcasecmp(argv[1], "int"))
    type = UDEF_INT;
  else {
    Tcl_AppendResult(irp, "invalid type. Must be one of: flag, int", NULL);
    return TCL_ERROR;
  }
  initudef(type, argv[2], 1);
  return TCL_OK;
}

static int tcl_renudef STDVAR
{
  struct udef_struct *ul;
  int type, found = 0;

  BADARGS(4, 4, " type oldname newname");
  if (!egg_strcasecmp(argv[1], "flag"))
    type = UDEF_FLAG;
  else if (!egg_strcasecmp(argv[1], "int"))
    type = UDEF_INT;
  else {
    Tcl_AppendResult(irp, "invalid type. Must be one of: flag, int", NULL);
    return TCL_ERROR;
  }
  for (ul = udef; ul; ul = ul->next) {
    if (ul->type == type && !egg_strcasecmp(ul->name, argv[2])) {
      nfree(ul->name);
      ul->name = nmalloc(strlen(argv[3]) + 1);
      strcpy(ul->name, argv[3]);
      found = 1;
    }
  }
  if (!found) {
    Tcl_AppendResult(irp, "not found", NULL);
    return TCL_ERROR;
  } else
    return TCL_OK;
}

static int tcl_deludef STDVAR
{
  struct udef_struct *ul, *ull;
  int type, found = 0;

  BADARGS(3, 3, " type name");
  if (!egg_strcasecmp(argv[1], "flag"))
    type = UDEF_FLAG;
  else if (!egg_strcasecmp(argv[1], "int"))
    type = UDEF_INT;
  else {
    Tcl_AppendResult(irp, "invalid type. Must be one of: flag, int", NULL);
    return TCL_ERROR;
  }
  for (ul = udef; ul; ul = ul->next) {
    ull = ul->next;
    if (!ull)
      break;
    if (ull->type == type && !egg_strcasecmp(ull->name, argv[2])) {
      ul->next = ull->next;
      nfree(ull->name);
      free_udef_chans(ull->values);
      nfree(ull);
      found = 1;
    }
  }
  if (udef) {
    if (udef->type == type && !egg_strcasecmp(udef->name, argv[2])) {
      ul = udef->next;
      nfree(udef->name);
      free_udef_chans(udef->values);
      nfree(udef);
      udef = ul;
      found = 1;
    }
  }
  if (!found) {
    Tcl_AppendResult(irp, "not found", NULL);
    return TCL_ERROR;
  } else
    return TCL_OK;
}

static tcl_cmds channels_cmds[] =
{
  {"channel",		tcl_channel},
  {"channels",		tcl_channels},
  {"exemptlist",	tcl_exemptlist},
  {"invitelist",	tcl_invitelist},
  {"banlist",		tcl_banlist},
  {"validchan",		tcl_validchan},
  {"getchaninfo",	tcl_getchaninfo},
  {"setchaninfo",	tcl_setchaninfo},
  {"setudef",		tcl_setudef},
  {"renudef",		tcl_renudef},
  {"deludef",		tcl_deludef},
  {NULL,		NULL}
};
