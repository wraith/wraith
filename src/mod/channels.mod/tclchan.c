/*
 * tclchan.c -- part of channels.mod
 *
 */


/* Parse options for a channel.
 */
static int tcl_channel_modify(char *result, struct chanset_t *chan, int items, char **item)
{
  int i, x = 0, found;
#ifdef LEAF
  int old_status = chan->status,
      old_mode_mns_prot = chan->mode_mns_prot,
      old_mode_pls_prot = chan->mode_pls_prot;
  module_entry *me = NULL;
#endif /* LEAF */
  struct udef_struct *ul = udef;
  char s[121] = "";

  for (i = 0; i < items; i++) {
    if (!strcmp(item[i], "chanmode")) {
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "channel chanmode needs argument");
	return TCL_ERROR;
      }
      strncpy(s, item[i], 120);
      s[120] = 0;
      set_mode_protect(chan, s);
    } else if (!strcmp(item[i], "addedby")) {
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "addedby chanmode needs argument");
	return TCL_ERROR;
      }
      strncpyz(chan->added_by, item[i], NICKLEN);
    } else if (!strcmp(item[i], "addedts")) {
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "addedts chanmode needs argument");
	return TCL_ERROR;
      }
      chan->added_ts = atoi(item[i]);
    } else if (!strcmp(item[i], "idle-kick")) {
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "channel idle-kick needs argument");
	return TCL_ERROR;
      }
      chan->idle_kick = atoi(item[i]);
    } else if (!strcmp(item[i], "limit")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel limit needs argument");
        return TCL_ERROR;
      }
      chan->limitraise = atoi(item[i]);
      chan->limit_prot = 0;
    } else if (!strcmp(item[i], "dont-idle-kick"))
      chan->idle_kick = 0;
    else if (!strcmp(item[i], "stopnethack-mode")) {
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "channel stopnethack-mode needs argument");
	return TCL_ERROR;
      }
      chan->stopnethack_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "revenge-mode")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel revenge-mode needs argument");
        return TCL_ERROR;
      }
      chan->revenge_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "ban-time")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel ban-time needs argument");
        return TCL_ERROR;
      }
      chan->ban_time = atoi(item[i]);
    } else if (!strcmp(item[i], "exempt-time")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel exempt-time needs argument");
        return TCL_ERROR;
      }
      chan->exempt_time = atoi(item[i]);
    } else if (!strcmp(item[i], "invite-time")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel invite-time needs argument");
        return TCL_ERROR;
      }
      chan->invite_time = atoi(item[i]);
    } else if (!strcmp(item[i], "closed-ban")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel closed-ban needs argument");
        return TCL_ERROR;
      }
      chan->closed_ban = atoi(item[i]);
/* Chanint template
 *  } else if (!strcmp(item[i], "temp")) {
 *    i++;
 *    if (i >= items) {
 *      if (result)
 *        sprintf(result, "channel temp needs argument");
 *      return TCL_ERROR;
 *    }
 *    chan->temp = atoi(item[i]);
 */
/* Chanchar template
    } else if (!strcmp(item[i], "temp")) {
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel temp needs argument");
        return TCL_ERROR;
      }
      strncpyz(chan->temp, item[i], sizeof(chan->temp));
      check_temp(chan);
 */
    } else if (!strcmp(item[i], "topic")) { //this is here for compatability
      i++;
      if (i >= items) {
        if (result)
          sprintf(result, "channel topic needs argument");
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
      chan->status &= ~(CHAN_FASTOP | CHAN_PROTECTOPS);
    else if (!strcmp(item[i], "+private")) {
      chan->status |= CHAN_PRIVATE;
    }
    else if (!strcmp(item[i], "-private"))
      chan->status &= ~CHAN_PRIVATE;

    /* ignore wasoptest, stopnethack and clearbans in chanfile, remove
       this later */
    else if (!strcmp(item[i], "+dontkickops")) ;
    else if (!strcmp(item[i], "-dontkickops")) ;
    else if (!strcmp(item[i], "+nomdop"))  ;
    else if (!strcmp(item[i], "-nomdop"))  ;
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
      int *pthr = NULL, *ptime = NULL;
      char *p = NULL;

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
	if (result)
	  sprintf(result, "illegal channel flood type: %s", item[i]);
	return TCL_ERROR;
      }
      i++;
      if (i >= items) {
	if (result)
	  sprintf(result, "%s needs argument", item[i - 1]);
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
            if (result)
              sprintf(result, "this setting needs an argument");
            return TCL_ERROR;
          }
          setudef(ul, chan->dname, atoi(item[i]));
          found = 1;
	  break;
        }
      }
      if (!found) {
        if (result && item[i][0]) /* ignore "" */
      	  sprintf(result, "illegal channel option: %s", item[i]);
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
    if ((old_status ^ chan->status) & (CHAN_ENFORCEBANS |
	CHAN_BITCH)) {
      if ((me = module_find("irc", 0, 0)))
        (me->funcs[IRC_RECHECK_CHANNEL])(chan, 1);
    } else if (old_mode_pls_prot != chan->mode_pls_prot ||
	       old_mode_mns_prot != chan->mode_mns_prot)
    if ((me = module_find("irc", 0, 0)))
      (me->funcs[IRC_RECHECK_CHANNEL_MODES])(chan);
  }
#endif /* LEAF */
  if (x > 0)
    return TCL_ERROR;
  return TCL_OK;
}

static void init_masklist(masklist *m)
{
  m->mask = (char *)calloc(1, 1);
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
    chan->channel.key = (char *) calloc(1, 1);
  }

  chan->channel.ban = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.ban);

  chan->channel.exempt = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.exempt);

  chan->channel.invite = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.invite);

  chan->channel.member = (memberlist *) calloc(1, sizeof(memberlist));
  chan->channel.member->nick[0] = 0;
  chan->channel.member->next = NULL;
  chan->channel.topic = NULL;
}

static void clear_masklist(masklist *m)
{
  masklist *temp = NULL;

  for (; m; m = temp) {
    temp = m->next;
    if (m->mask)
      free(m->mask);
    if (m->who)
      free(m->who);
    free(m);
  }
}

/* Clear out channel data from memory.
 */
static void clear_channel(struct chanset_t *chan, int reset)
{
  memberlist *m = NULL, *m1 = NULL;

  if (chan->channel.topic)
    free(chan->channel.topic);
  for (m = chan->channel.member; m; m = m1) {
    m1 = m->next;
    free(m);
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
static int tcl_channel_add(char *result, char *newname, char *options)
{
  struct chanset_t *chan = NULL;
  int items;
  int ret = TCL_OK;
  int join = 0;
  char buf[2048] = "", buf2[256] = "";
//  char **item = NULL;
  CONST char **item;


  if (!newname || !newname[0] || !strchr(CHANMETA, newname[0])) {
    if (result)
      sprintf(result, "invalid channel prefix");
    return TCL_ERROR;
  }

  if (strchr(newname, ',') != NULL) {
    if (result)
      sprintf(result, "invalid channel name");
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
    chan = (struct chanset_t *) calloc(1, sizeof(struct chanset_t));

    /* Hells bells, why set *every* variable to 0 when we have bzero? */
/* not needed..    egg_bzero(chan, sizeof(struct chanset_t)); */

    /* These are defaults, bzero already set them 0, but we set them for future reference */
    chan->limit_prot = 0;
    chan->limit = 0;
    chan->closed_ban = 0;
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
  if ((tcl_channel_modify(result, chan, items, (char **) item) != TCL_OK) && !loading) {
    ret = TCL_ERROR;
  }
  Tcl_Free((char *) item);
#ifdef LEAF
  if (join && shouldjoin(chan) && module_find("irc", 0, 0))
    dprintf(DP_SERVER, "JOIN %s %s\n", chan->dname, chan->key_prot);
#endif
  return ret;
}
