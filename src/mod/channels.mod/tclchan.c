static int tcl_killban STDVAR
{
  struct chanset_t *chan;
    BADARGS (2, 2, " ban");
  if (u_delban (NULL, argv[1], 1) > 0)
    {
      for (chan = chanset; chan; chan = chan->next)
	add_mode (chan, '-', 'b', argv[1]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
      Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_killchanban STDVAR
{
  struct chanset_t *chan;
    BADARGS (3, 3, " channel ban");
    chan = findchan_by_dname (argv[1]);
  if (!chan)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (u_delban (chan, argv[2], 1) > 0)
    {
      add_mode (chan, '-', 'b', argv[2]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_killexempt STDVAR
{
  struct chanset_t *chan;
    BADARGS (2, 2, " exempt");
  if (u_delexempt (NULL, argv[1], 1) > 0)
    {
      for (chan = chanset; chan; chan = chan->next)
	add_mode (chan, '-', 'e', argv[1]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
      Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_killchanexempt STDVAR
{
  struct chanset_t *chan;
    BADARGS (3, 3, " channel exempt");
    chan = findchan_by_dname (argv[1]);
  if (!chan)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (u_delexempt (chan, argv[2], 1) > 0)
    {
      add_mode (chan, '-', 'e', argv[2]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_killinvite STDVAR
{
  struct chanset_t *chan;
    BADARGS (2, 2, " invite");
  if (u_delinvite (NULL, argv[1], 1) > 0)
    {
      for (chan = chanset; chan; chan = chan->next)
	add_mode (chan, '-', 'I', argv[1]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
      Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_killchaninvite STDVAR
{
  struct chanset_t *chan;
    BADARGS (3, 3, " channel invite");
    chan = findchan_by_dname (argv[1]);
  if (!chan)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (u_delinvite (chan, argv[2], 1) > 0)
    {
      add_mode (chan, '-', 'I', argv[2]);
      Tcl_AppendResult (irp, "1", NULL);
    }
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_stick STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_setsticky_ban
	  (chan, argv[1], !strncmp (argv[0], "un", 2) ? 0 : 1))
	  ok = 1;
    }
  if (!ok
      && u_setsticky_ban (NULL, argv[1], !strncmp (argv[0], "un", 2) ? 0 : 1))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_stickinvite STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_setsticky_invite
	  (chan, argv[1], !strncmp (argv[0], "un", 2) ? 0 : 1))
	  ok = 1;
    }
  if (!ok
      && u_setsticky_invite (NULL, argv[1],
			     !strncmp (argv[0], "un", 2) ? 0 : 1))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_stickexempt STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_setsticky_exempt
	  (chan, argv[1], !strncmp (argv[0], "un", 2) ? 0 : 1))
	  ok = 1;
    }
  if (!ok
      && u_setsticky_exempt (NULL, argv[1],
			     !strncmp (argv[0], "un", 2) ? 0 : 1))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isban STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->bans, argv[1]))
	  ok = 1;
    }
  if (u_equals_mask (global_bans, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isexempt STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " exempt ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->exempts, argv[1]))
	  ok = 1;
    }
  if (u_equals_mask (global_exempts, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isinvite STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " invite ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->invites, argv[1]))
	  ok = 1;
    }
  if (u_equals_mask (global_invites, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isbansticky STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_sticky_mask (chan->bans, argv[1]))
	  ok = 1;
    }
  if (u_sticky_mask (global_bans, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isexemptsticky STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " exempt ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_sticky_mask (chan->exempts, argv[1]))
	  ok = 1;
    }
  if (u_sticky_mask (global_exempts, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isinvitesticky STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " invite ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (!chan)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_sticky_mask (chan->invites, argv[1]))
	  ok = 1;
    }
  if (u_sticky_mask (global_invites, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_ispermban STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " ban ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->bans, argv[1]) == 2)
	ok = 1;
    }
  if (u_equals_mask (global_bans, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_ispermexempt STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " exempt ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->exempts, argv[1]) == 2)
	ok = 1;
    }
  if (u_equals_mask (global_exempts, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_isperminvite STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " invite ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_equals_mask (chan->invites, argv[1]) == 2)
	ok = 1;
    }
  if (u_equals_mask (global_invites, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_matchban STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " user!nick@host ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_match_mask (chan->bans, argv[1]))
	  ok = 1;
    }
  if (u_match_mask (global_bans, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_matchexempt STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " user!nick@host ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_match_mask (chan->exempts, argv[1]))
	  ok = 1;
    }
  if (u_match_mask (global_exempts, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_matchinvite STDVAR
{
  struct chanset_t *chan;
  int ok = 0;
    BADARGS (2, 3, " user!nick@host ?channel?");
  if (argc == 3)
    {
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[2], NULL);
	  return TCL_ERROR;
	}
      if (u_match_mask (chan->invites, argv[1]))
	  ok = 1;
    }
  if (u_match_mask (global_invites, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_newchanban STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char ban[161], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
  module_entry *me;
    BADARGS (5, 7, " channel ban creator comment ?lifetime? ?options?");
    chan = findchan_by_dname (argv[1]);
  if (chan == NULL)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (argc == 7)
    {
      if (!egg_strcasecmp (argv[6], "none"));
      else if (!egg_strcasecmp (argv[6], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[6],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (ban, argv[2], sizeof ban);
  strncpyz (from, argv[3], sizeof from);
  strncpyz (cmt, argv[4], sizeof cmt);
  if (argc == 5)
    {
      if (chan->ban_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * chan->ban_time);
    }
  else
    {
      if (atoi (argv[5]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[5]) * 60);
    }
  if (u_addban (chan, ban, from, cmt, expire_time, sticky))
    if ((me = module_find ("irc", 0, 0)))
      (me->funcs[IRC_CHECK_THIS_BAN]) (chan, ban, sticky);
  return TCL_OK;
}
static int tcl_newban STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char ban[UHOSTLEN], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
  module_entry *me;
    BADARGS (4, 6, " ban creator comment ?lifetime? ?options?");
  if (argc == 6)
    {
      if (!egg_strcasecmp (argv[5], "none"));
      else if (!egg_strcasecmp (argv[5], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[5],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (ban, argv[1], sizeof ban);
  strncpyz (from, argv[2], sizeof from);
  strncpyz (cmt, argv[3], sizeof cmt);
  if (argc == 4)
    {
      if (global_ban_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * global_ban_time);
    }
  else
    {
      if (atoi (argv[4]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[4]) * 60);
    }
  if (u_addban (NULL, ban, from, cmt, expire_time, sticky))
    if ((me = module_find ("irc", 0, 0)))
      for (chan = chanset; chan != NULL; chan = chan->next)
	(me->funcs[IRC_CHECK_THIS_BAN]) (chan, ban, sticky);
  return TCL_OK;
}
static int tcl_newchanexempt STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char exempt[161], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
    BADARGS (5, 7, " channel exempt creator comment ?lifetime? ?options?");
    chan = findchan_by_dname (argv[1]);
  if (chan == NULL)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (argc == 7)
    {
      if (!egg_strcasecmp (argv[6], "none"));
      else if (!egg_strcasecmp (argv[6], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[6],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (exempt, argv[2], sizeof exempt);
  strncpyz (from, argv[3], sizeof from);
  strncpyz (cmt, argv[4], sizeof cmt);
  if (argc == 5)
    {
      if (chan->exempt_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * chan->exempt_time);
    }
  else
    {
      if (atoi (argv[5]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[5]) * 60);
    }
  if (u_addexempt (chan, exempt, from, cmt, expire_time, sticky))
    add_mode (chan, '+', 'e', exempt);
  return TCL_OK;
}
static int tcl_newexempt STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char exempt[UHOSTLEN], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
    BADARGS (4, 6, " exempt creator comment ?lifetime? ?options?");
  if (argc == 6)
    {
      if (!egg_strcasecmp (argv[5], "none"));
      else if (!egg_strcasecmp (argv[5], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[5],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (exempt, argv[1], sizeof exempt);
  strncpyz (from, argv[2], sizeof from);
  strncpyz (cmt, argv[3], sizeof cmt);
  if (argc == 4)
    {
      if (global_exempt_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * global_exempt_time);
    }
  else
    {
      if (atoi (argv[4]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[4]) * 60);
    }
  u_addexempt (NULL, exempt, from, cmt, expire_time, sticky);
  for (chan = chanset; chan; chan = chan->next)
    add_mode (chan, '+', 'e', exempt);
  return TCL_OK;
}
static int tcl_newchaninvite STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char invite[161], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
    BADARGS (5, 7, " channel invite creator comment ?lifetime? ?options?");
    chan = findchan_by_dname (argv[1]);
  if (chan == NULL)
    {
      Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (argc == 7)
    {
      if (!egg_strcasecmp (argv[6], "none"));
      else if (!egg_strcasecmp (argv[6], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[6],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (invite, argv[2], sizeof invite);
  strncpyz (from, argv[3], sizeof from);
  strncpyz (cmt, argv[4], sizeof cmt);
  if (argc == 5)
    {
      if (chan->invite_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * chan->invite_time);
    }
  else
    {
      if (atoi (argv[5]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[5]) * 60);
    }
  if (u_addinvite (chan, invite, from, cmt, expire_time, sticky))
    add_mode (chan, '+', 'I', invite);
  return TCL_OK;
}
static int tcl_newinvite STDVAR
{
  time_t expire_time;
  struct chanset_t *chan;
  char invite[UHOSTLEN], cmt[MASKREASON_LEN], from[HANDLEN + 1];
  int sticky = 0;
    BADARGS (4, 6, " invite creator comment ?lifetime? ?options?");
  if (argc == 6)
    {
      if (!egg_strcasecmp (argv[5], "none"));
      else if (!egg_strcasecmp (argv[5], "sticky"))
	sticky = 1;
      else
	{
	  Tcl_AppendResult (irp, "invalid option ", argv[5],
			    " (must be one of: ", "sticky, none)", NULL);
	  return TCL_ERROR;
	}
    }
  strncpyz (invite, argv[1], sizeof invite);
  strncpyz (from, argv[2], sizeof from);
  strncpyz (cmt, argv[3], sizeof cmt);
  if (argc == 4)
    {
      if (global_invite_time == 0)
	expire_time = 0L;
      else
	expire_time = now + (60 * global_invite_time);
    }
  else
    {
      if (atoi (argv[4]) == 0)
	expire_time = 0L;
      else
	expire_time = now + (atoi (argv[4]) * 60);
    }
  u_addinvite (NULL, invite, from, cmt, expire_time, sticky);
  for (chan = chanset; chan; chan = chan->next)
    add_mode (chan, '+', 'I', invite);
  return TCL_OK;
}
static int
tcl_channel_info (Tcl_Interp * irp, struct chanset_t *chan)
{
  char a[121], b[121], s[121];
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char *args[2];
#else
  char *args[2];
#endif
  struct udef_struct *ul;
  get_mode_protect (chan, s);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->idle_kick);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->limitraise);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->stopnethack_mode);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->revenge_mode);
  Tcl_AppendElement (irp, s);
  Tcl_AppendElement (irp, chan->need_op);
  Tcl_AppendElement (irp, chan->need_invite);
  Tcl_AppendElement (irp, chan->need_key);
  Tcl_AppendElement (irp, chan->need_unban);
  Tcl_AppendElement (irp, chan->need_limit);
  simple_sprintf (s, "%d:%d", chan->flood_pub_thr, chan->flood_pub_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d:%d", chan->flood_ctcp_thr, chan->flood_ctcp_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d:%d", chan->flood_join_thr, chan->flood_join_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d:%d", chan->flood_kick_thr, chan->flood_kick_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d:%d", chan->flood_deop_thr, chan->flood_deop_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d:%d", chan->flood_nick_thr, chan->flood_nick_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->ban_time);
  Tcl_AppendElement (irp, s);
#ifdef S_IRCNET
  simple_sprintf (s, "%d", chan->exempt_time);
  Tcl_AppendElement (irp, s);
  simple_sprintf (s, "%d", chan->invite_time);
  Tcl_AppendElement (irp, s);
#endif
  if (chan->status & CHAN_ENFORCEBANS)
    Tcl_AppendElement (irp, "+enforcebans");
  else
    Tcl_AppendElement (irp, "-enforcebans");
  if (chan->status & CHAN_DYNAMICBANS)
    Tcl_AppendElement (irp, "+dynamicbans");
  else
    Tcl_AppendElement (irp, "-dynamicbans");
  if (chan->status & CHAN_NOUSERBANS)
    Tcl_AppendElement (irp, "-userbans");
  else
    Tcl_AppendElement (irp, "+userbans");
  if (chan->status & CHAN_BITCH)
    Tcl_AppendElement (irp, "+bitch");
  else
    Tcl_AppendElement (irp, "-bitch");
  if (chan->status & CHAN_PROTECTOPS)
    Tcl_AppendElement (irp, "+protectops");
  else
    Tcl_AppendElement (irp, "-protectops");
  if (chan->status & CHAN_PROTECTFRIENDS)
    Tcl_AppendElement (irp, "+protectfriends");
  else
    Tcl_AppendElement (irp, "-protectfriends");
  if (chan->status & CHAN_DONTKICKOPS)
    Tcl_AppendElement (irp, "+dontkickops");
  else
    Tcl_AppendElement (irp, "-dontkickops");
  if (chan->status & CHAN_INACTIVE)
    Tcl_AppendElement (irp, "+inactive");
  else
    Tcl_AppendElement (irp, "-inactive");
  if (chan->status & CHAN_REVENGE)
    Tcl_AppendElement (irp, "+revenge");
  else
    Tcl_AppendElement (irp, "-revenge");
  if (chan->status & CHAN_REVENGEBOT)
    Tcl_AppendElement (irp, "+revengebot");
  else
    Tcl_AppendElement (irp, "-revengebot");
  if (chan->status & CHAN_SECRET)
    Tcl_AppendElement (irp, "+secret");
  else
    Tcl_AppendElement (irp, "-secret");
  if (chan->status & CHAN_CYCLE)
    Tcl_AppendElement (irp, "+cycle");
  else
    Tcl_AppendElement (irp, "-cycle");
#ifdef S_IRCNET
  if (chan->ircnet_status & CHAN_DYNAMICEXEMPTS)
    Tcl_AppendElement (irp, "+dynamicexempts");
  else
    Tcl_AppendElement (irp, "-dynamicexempts");
  if (chan->ircnet_status & CHAN_NOUSEREXEMPTS)
    Tcl_AppendElement (irp, "-userexempts");
  else
    Tcl_AppendElement (irp, "+userexempts");
  if (chan->ircnet_status & CHAN_DYNAMICINVITES)
    Tcl_AppendElement (irp, "+dynamicinvites");
  else
    Tcl_AppendElement (irp, "-dynamicinvites");
  if (chan->ircnet_status & CHAN_NOUSERINVITES)
    Tcl_AppendElement (irp, "-userinvites");
  else
    Tcl_AppendElement (irp, "+userinvites");
#endif
  if (chan->status & CHAN_NODESYNCH)
    Tcl_AppendElement (irp, "+nodesynch");
  else
    Tcl_AppendElement (irp, "-nodesynch");
  if (chan->status & CHAN_CLOSED)
    Tcl_AppendElement (irp, "+closed");
  else
    Tcl_AppendElement (irp, "-closed");
  if (chan->status & CHAN_TAKE)
    Tcl_AppendElement (irp, "+take");
  else
    Tcl_AppendElement (irp, "-take");
  if (chan->status & CHAN_NOMOP)
    Tcl_AppendElement (irp, "+nomop");
  else
    Tcl_AppendElement (irp, "-nomop");
  if (chan->status & CHAN_NOMDOP)
    Tcl_AppendElement (irp, "+nomdop");
  else
    Tcl_AppendElement (irp, "-nomdop");
  if (chan->status & CHAN_VOICE)
    Tcl_AppendElement (irp, "+voice");
  else
    Tcl_AppendElement (irp, "-voice");
  if (chan->status & CHAN_FASTOP)
    Tcl_AppendElement (irp, "+fastop");
  else
    Tcl_AppendElement (irp, "-fastop");
  if (chan->status & CHAN_PUNISH)
    Tcl_AppendElement (irp, "+punish");
  else
    Tcl_AppendElement (irp, "-punish");
  for (ul = udef; ul; ul = ul->next)
    {
      if (!ul->defined || !ul->name)
	continue;
      if (ul->type == UDEF_FLAG)
	{
	  simple_sprintf (s, "%c%s",
			  getudef (ul->values, chan->dname) ? '+' : '-',
			  ul->name);
	  Tcl_AppendElement (irp, s);
	}
      else if (ul->type == UDEF_INT)
	{
	  char *x;
	  egg_snprintf (a, sizeof a, "%s", ul->name);
	  egg_snprintf (b, sizeof b, "%d", getudef (ul->values, chan->dname));
	  args[0] = a;
	  args[1] = b;
	  x = Tcl_Merge (2, args);
	  egg_snprintf (s, sizeof s, "%s", x);
	  Tcl_Free ((char *) x);
	  Tcl_AppendElement (irp, s);
	}
      else
	debug1 ("UDEF-ERROR: unknown type %d", ul->type);
    }
  return TCL_OK;
}
static int
tcl_channel_get (Tcl_Interp * irp, struct chanset_t *chan, char *setting)
{
  char s[121];
  struct udef_struct *ul;
#define CHECK(x) !strcmp(setting, x)
#define CHKFLAG_POS(x,y,z) (!strcmp(setting, y)) { if(z & x) simple_sprintf(s, "%d", 1); else simple_sprintf(s, "%d", 0); }
#define CHKFLAG_NEG(x,y,z) (!strcmp(setting, y)) { if (z & x) simple_sprintf(s, "%d", 0); else simple_sprintf(s, "%d", 1); }
  if (CHECK ("chanmode"))
    get_mode_protect (chan, s);
  else if (CHECK ("need-op"))
    {
      strncpy (s, chan->need_op, 120);
      s[120] = 0;
    }
  else if (CHECK ("need-invite"))
    {
      strncpy (s, chan->need_invite, 120);
      s[120] = 0;
    }
  else if (CHECK ("need-key"))
    {
      strncpy (s, chan->need_key, 120);
      s[120] = 0;
    }
  else if (CHECK ("need-unban"))
    {
      strncpy (s, chan->need_unban, 120);
      s[120] = 0;
    }
  else if (CHECK ("need-limit"))
    {
      strncpy (s, chan->need_limit, 120);
      s[120] = 0;
    }
  else if (CHECK ("idle-kick"))
    simple_sprintf (s, "%d", chan->idle_kick);
  else if (CHECK ("limit"))
    simple_sprintf (s, "%d", chan->limitraise);
  else if (CHECK ("stop-net-hack"))
    simple_sprintf (s, "%d", chan->stopnethack_mode);
  else if (CHECK ("revenge-mode"))
    simple_sprintf (s, "%d", chan->revenge_mode);
  else if (CHECK ("flood-chan"))
    simple_sprintf (s, "%d %d", chan->flood_pub_thr, chan->flood_pub_time);
  else if (CHECK ("flood-ctcp"))
    simple_sprintf (s, "%d %d", chan->flood_ctcp_thr, chan->flood_ctcp_time);
  else if (CHECK ("flood-join"))
    simple_sprintf (s, "%d %d", chan->flood_join_thr, chan->flood_join_time);
  else if (CHECK ("flood-kick"))
    simple_sprintf (s, "%d %d", chan->flood_kick_thr, chan->flood_kick_time);
  else if (CHECK ("flood-deop"))
    simple_sprintf (s, "%d %d", chan->flood_deop_thr, chan->flood_deop_time);
  else if (CHECK ("flood-nick"))
    simple_sprintf (s, "%d %d", chan->flood_nick_thr, chan->flood_nick_time);
  else if (CHECK ("ban-time"))
    simple_sprintf (s, "%d", chan->ban_time);
#ifdef S_IRCNET
  else if (CHECK ("exempt-time"))
    simple_sprintf (s, "%d", chan->exempt_time);
  else if (CHECK ("invite-time"))
    simple_sprintf (s, "%d", chan->invite_time);
#endif
  else if CHKFLAG_POS
    (CHAN_ENFORCEBANS, "enforcebans", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_DYNAMICBANS, "dynamicbans", chan->status)
    else
  if CHKFLAG_NEG
    (CHAN_NOUSERBANS, "userbans", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_BITCH, "bitch", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_PROTECTOPS, "protectops", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_PROTECTFRIENDS, "protectfriends", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_DONTKICKOPS, "dontkickops", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_INACTIVE, "inactive", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_REVENGE, "revenge", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_REVENGEBOT, "revengebot", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_SECRET, "secret", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_CYCLE, "cycle", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_SEEN, "seen", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_NODESYNCH, "nodesynch", chan->status)
#ifdef S_IRCNET
      else
  if CHKFLAG_POS
    (CHAN_DYNAMICEXEMPTS, "dynamicexempts", chan->ircnet_status)
    else
  if CHKFLAG_NEG
    (CHAN_NOUSEREXEMPTS, "userexempts", chan->ircnet_status)
    else
  if CHKFLAG_POS
    (CHAN_DYNAMICINVITES, "dynamicinvites", chan->ircnet_status)
    else
  if CHKFLAG_NEG
    (CHAN_NOUSERINVITES, "userinvites", chan->ircnet_status)
#endif
      else
  if CHKFLAG_POS
    (CHAN_CLOSED, "closed", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_TAKE, "take", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_NOMOP, "nomdop", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_NOMDOP, "nomdop", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_VOICE, "voice", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_FASTOP, "fastop", chan->status)
    else
  if CHKFLAG_POS
    (CHAN_PUNISH, "punish", chan->status)
    else
    {
      for (ul = udef; ul && ul->name; ul = ul->next)
	{
	  if (!strcmp (setting, ul->name))
	    break;
	}
      if (!ul || !ul->name)
	{
	  Tcl_AppendResult (irp, "Unknown channel setting.", NULL);
	  return (TCL_ERROR);
	}
      simple_sprintf (s, "%d", getudef (ul->values, chan->dname));
      Tcl_AppendResult (irp, s, NULL);
      return (TCL_OK);
    }
  Tcl_AppendResult (irp, s, NULL);
  return (TCL_OK);
}
static int tcl_channel STDVAR
{
  struct chanset_t *chan;
  char buf2[1024];
    BADARGS (2, 999, " command ?options?");
  if (!strcmp (argv[1], "add"))
    {
      BADARGS (3, 4, " add channel-name ?options-list?");
      if (argc == 3)
	{
	  sprintf (buf2, "cjoin %s", argv[2]);
	  if (!loading)
	    botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
	  return tcl_channel_add (irp, argv[2], "");
	}
      sprintf (buf2, "cjoin %s %s", argv[2], argv[3]);
      if (!loading)
	botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
      return tcl_channel_add (irp, argv[2], argv[3]);
    }
  if (!strcmp (argv[1], "set"))
    {
      BADARGS (3, 999, " set channel-name ?options?");
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  if (chan_hack == 1)
	    return TCL_OK;
	  Tcl_AppendResult (irp, "no such channel record", NULL);
	  return TCL_ERROR;
	}
      sprintf (buf2, "cset %s %s", chan->dname, argv[3]);
      botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
      return tcl_channel_modify (irp, chan, argc - 3, &argv[3]);
    }
  if (!strcmp (argv[1], "get"))
    {
      BADARGS (4, 4, " get channel-name setting-name");
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "no such channel record", NULL);
	  return TCL_ERROR;
	}
      return (tcl_channel_get (irp, chan, argv[3]));
    }
  if (!strcmp (argv[1], "info"))
    {
      BADARGS (3, 3, " info channel-name");
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "no such channel record", NULL);
	  return TCL_ERROR;
	}
      return tcl_channel_info (irp, chan);
    }
  if (!strcmp (argv[1], "remove"))
    {
      BADARGS (3, 3, " remove channel-name");
      chan = findchan_by_dname (argv[2]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "no such channel record", NULL);
	  return TCL_ERROR;
	}
      sprintf (buf2, "cpart %s", argv[2]);
      botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
      remove_channel (chan);
      return TCL_OK;
    }
  Tcl_AppendResult (irp, "unknown channel command: should be one of: ",
		    "add, set, get, info, remove", NULL);
  return TCL_ERROR;
}
static int
tcl_channel_modify (Tcl_Interp * irp, struct chanset_t *chan, int items,
		    char **item)
{
  int i, x = 0, found;
#ifdef LEAF
  int old_status = chan->status, old_mode_mns_prot =
    chan->mode_mns_prot, old_mode_pls_prot = chan->mode_pls_prot;
#endif
  struct udef_struct *ul = udef;
  char s[121];
#ifdef LEAF
  module_entry *me;
#endif
  for (i = 0; i < items; i++)
    {
      if (!strcmp (item[i], "need-op"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel need-op needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (chan->need_op, item[i], 120);
	  chan->need_op[120] = 0;
	}
      else if (!strcmp (item[i], "need-invite"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel need-invite needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (chan->need_invite, item[i], 120);
	  chan->need_invite[120] = 0;
	}
      else if (!strcmp (item[i], "need-key"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel need-key needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (chan->need_key, item[i], 120);
	  chan->need_key[120] = 0;
	}
      else if (!strcmp (item[i], "need-limit"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel need-limit needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (chan->need_limit, item[i], 120);
	  chan->need_limit[120] = 0;
	}
      else if (!strcmp (item[i], "need-unban"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel need-unban needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (chan->need_unban, item[i], 120);
	  chan->need_unban[120] = 0;
	}
      else if (!strcmp (item[i], "chanmode"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel chanmode needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  strncpy (s, item[i], 120);
	  s[120] = 0;
	  set_mode_protect (chan, s);
	}
      else if (!strcmp (item[i], "idle-kick"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel idle-kick needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->idle_kick = atoi (item[i]);
	}
      else if (!strcmp (item[i], "limit"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel limit needs argument", NULL);
	      return TCL_ERROR;
	    }
	  chan->limitraise = atoi (item[i]);
	  chan->limit_prot = 0;
	}
      else if (!strcmp (item[i], "dont-idle-kick"))
	chan->idle_kick = 0;
      else if (!strcmp (item[i], "stopnethack-mode"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp,
				  "channel stopnethack-mode needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->stopnethack_mode = atoi (item[i]);
	}
      else if (!strcmp (item[i], "revenge-mode"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel revenge-mode needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->revenge_mode = atoi (item[i]);
	}
      else if (!strcmp (item[i], "ban-time"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel ban-time needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->ban_time = atoi (item[i]);
#ifdef S_IRCNET
	}
      else if (!strcmp (item[i], "exempt-time"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel exempt-time needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->exempt_time = atoi (item[i]);
	}
      else if (!strcmp (item[i], "invite-time"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel invite-time needs argument",
				  NULL);
	      return TCL_ERROR;
	    }
	  chan->invite_time = atoi (item[i]);
#endif
	}
      else if (!strcmp (item[i], "topic"))
	{
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, "channel topic needs argument", NULL);
	      return TCL_ERROR;
	    }
	  strncpyz (chan->topic_prot, item[i], sizeof (chan->topic_prot));
	  check_topic (chan);
	}
      else if (!strcmp (item[i], "+enforcebans"))
	chan->status |= CHAN_ENFORCEBANS;
      else if (!strcmp (item[i], "-enforcebans"))
	chan->status &= ~CHAN_ENFORCEBANS;
      else if (!strcmp (item[i], "+dynamicbans"))
	chan->status |= CHAN_DYNAMICBANS;
      else if (!strcmp (item[i], "-dynamicbans"))
	chan->status &= ~CHAN_DYNAMICBANS;
      else if (!strcmp (item[i], "-userbans"))
	chan->status |= CHAN_NOUSERBANS;
      else if (!strcmp (item[i], "+userbans"))
	chan->status &= ~CHAN_NOUSERBANS;
      else if (!strcmp (item[i], "+bitch"))
	chan->status |= CHAN_BITCH;
      else if (!strcmp (item[i], "-bitch"))
	chan->status &= ~CHAN_BITCH;
      else if (!strcmp (item[i], "+nodesynch"))
	chan->status |= CHAN_NODESYNCH;
      else if (!strcmp (item[i], "-nodesynch"))
	chan->status &= ~CHAN_NODESYNCH;
      else if (!strcmp (item[i], "+protectops"))
	chan->status |= CHAN_PROTECTOPS;
      else if (!strcmp (item[i], "-protectops"))
	chan->status &= ~CHAN_PROTECTOPS;
      else if (!strcmp (item[i], "+protectfriends"))
	chan->status |= CHAN_PROTECTFRIENDS;
      else if (!strcmp (item[i], "-protectfriends"))
	chan->status &= ~CHAN_PROTECTFRIENDS;
      else if (!strcmp (item[i], "+dontkickops"))
	chan->status |= CHAN_DONTKICKOPS;
      else if (!strcmp (item[i], "-dontkickops"))
	chan->status &= ~CHAN_DONTKICKOPS;
      else if (!strcmp (item[i], "+inactive"))
	chan->status |= CHAN_INACTIVE;
      else if (!strcmp (item[i], "-inactive"))
	chan->status &= ~CHAN_INACTIVE;
      else if (!strcmp (item[i], "+revenge"))
	chan->status |= CHAN_REVENGE;
      else if (!strcmp (item[i], "-revenge"))
	chan->status &= ~CHAN_REVENGE;
      else if (!strcmp (item[i], "+revengebot"))
	chan->status |= CHAN_REVENGEBOT;
      else if (!strcmp (item[i], "-revengebot"))
	chan->status &= ~CHAN_REVENGEBOT;
      else if (!strcmp (item[i], "+secret"))
	chan->status |= CHAN_SECRET;
      else if (!strcmp (item[i], "-secret"))
	chan->status &= ~CHAN_SECRET;
      else if (!strcmp (item[i], "+cycle"))
	chan->status |= CHAN_CYCLE;
      else if (!strcmp (item[i], "-cycle"))
	chan->status &= ~CHAN_CYCLE;
#ifdef S_IRCNET
      else if (!strcmp (item[i], "+dynamicexempts"))
	chan->ircnet_status |= CHAN_DYNAMICEXEMPTS;
      else if (!strcmp (item[i], "-dynamicexempts"))
	chan->ircnet_status &= ~CHAN_DYNAMICEXEMPTS;
      else if (!strcmp (item[i], "-userexempts"))
	chan->ircnet_status |= CHAN_NOUSEREXEMPTS;
      else if (!strcmp (item[i], "+userexempts"))
	chan->ircnet_status &= ~CHAN_NOUSEREXEMPTS;
      else if (!strcmp (item[i], "+dynamicinvites"))
	chan->ircnet_status |= CHAN_DYNAMICINVITES;
      else if (!strcmp (item[i], "-dynamicinvites"))
	chan->ircnet_status &= ~CHAN_DYNAMICINVITES;
      else if (!strcmp (item[i], "-userinvites"))
	chan->ircnet_status |= CHAN_NOUSERINVITES;
      else if (!strcmp (item[i], "+userinvites"))
	chan->ircnet_status &= ~CHAN_NOUSERINVITES;
#endif
      else if (!strcmp (item[i], "+closed"))
	chan->status |= CHAN_CLOSED;
      else if (!strcmp (item[i], "-closed"))
	chan->status &= ~CHAN_CLOSED;
      else if (!strcmp (item[i], "+take"))
	chan->status |= CHAN_TAKE;
      else if (!strcmp (item[i], "-take"))
	chan->status &= ~CHAN_TAKE;
      else if (!strcmp (item[i], "+nomop"))
	chan->status |= CHAN_NOMOP;
      else if (!strcmp (item[i], "-nomop"))
	chan->status &= ~CHAN_NOMOP;
      else if (!strcmp (item[i], "+nomdop"))
	chan->status |= CHAN_NOMDOP;
      else if (!strcmp (item[i], "-nomdop"))
	chan->status &= ~CHAN_NOMDOP;
      else if (!strcmp (item[i], "+voice"))
	chan->status |= CHAN_VOICE;
      else if (!strcmp (item[i], "-voice"))
	chan->status &= ~CHAN_VOICE;
      else if (!strcmp (item[i], "+fastop"))
	{
	  chan->status |= CHAN_FASTOP;
	  chan->status &= ~CHAN_PUNISH;
	}
      else if (!strcmp (item[i], "-fastop"))
	chan->status &=
	  ~(CHAN_FASTOP | CHAN_PROTECTFRIENDS | CHAN_PROTECTOPS |
	    CHAN_DONTKICKOPS);
      else if (!strcmp (item[i], "+punish"))
	{
	  chan->status |= CHAN_PUNISH;
	  chan->status &= ~CHAN_FASTOP;
	}
      else if (!strcmp (item[i], "-punish"))
	chan->status &= ~CHAN_PUNISH;
      else if (!strcmp (item[i], "-stopnethack"));
      else if (!strcmp (item[i], "+stopnethack"));
      else if (!strcmp (item[i], "-wasoptest"));
      else if (!strcmp (item[i], "+wasoptest"));
      else if (!strcmp (item[i], "+clearbans"));
      else if (!strcmp (item[i], "-clearbans"));
      else if (!strncmp (item[i], "flood-", 6))
	{
	  int *pthr = 0, *ptime;
	  char *p;
	  if (!strcmp (item[i] + 6, "chan"))
	    {
	      pthr = &chan->flood_pub_thr;
	      ptime = &chan->flood_pub_time;
	    }
	  else if (!strcmp (item[i] + 6, "join"))
	    {
	      pthr = &chan->flood_join_thr;
	      ptime = &chan->flood_join_time;
	    }
	  else if (!strcmp (item[i] + 6, "ctcp"))
	    {
	      pthr = &chan->flood_ctcp_thr;
	      ptime = &chan->flood_ctcp_time;
	    }
	  else if (!strcmp (item[i] + 6, "kick"))
	    {
	      pthr = &chan->flood_kick_thr;
	      ptime = &chan->flood_kick_time;
	    }
	  else if (!strcmp (item[i] + 6, "deop"))
	    {
	      pthr = &chan->flood_deop_thr;
	      ptime = &chan->flood_deop_time;
	    }
	  else if (!strcmp (item[i] + 6, "nick"))
	    {
	      pthr = &chan->flood_nick_thr;
	      ptime = &chan->flood_nick_time;
	    }
	  else
	    {
	      if (irp)
		Tcl_AppendResult (irp, "illegal channel flood type: ",
				  item[i], NULL);
	      return TCL_ERROR;
	    }
	  i++;
	  if (i >= items)
	    {
	      if (irp)
		Tcl_AppendResult (irp, item[i - 1], " needs argument", NULL);
	      return TCL_ERROR;
	    }
	  p = strchr (item[i], ':');
	  if (p)
	    {
	      *p++ = 0;
	      *pthr = atoi (item[i]);
	      *ptime = atoi (p);
	      *--p = ':';
	    }
	  else
	    {
	      *pthr = atoi (item[i]);
	      *ptime = 1;
	    }
	}
      else
	{
	  if (!strncmp (item[i] + 1, "udef-flag-", 10))
	    initudef (UDEF_FLAG, item[i] + 11, 0);
	  else if (!strncmp (item[i], "udef-int-", 9))
	    initudef (UDEF_INT, item[i] + 9, 0);
	  found = 0;
	  for (ul = udef; ul; ul = ul->next)
	    {
	      if (ul->type == UDEF_FLAG
		  && (!egg_strcasecmp (item[i] + 1, ul->name)
		      || (!strncmp (item[i] + 1, "udef-flag-", 10)
			  && !egg_strcasecmp (item[i] + 11, ul->name))))
		{
		  if (item[i][0] == '+')
		    setudef (ul, chan->dname, 1);
		  else
		    setudef (ul, chan->dname, 0);
		  found = 1;
		  break;
		}
	      else if (ul->type == UDEF_INT
		       && (!egg_strcasecmp (item[i], ul->name)
			   || (!strncmp (item[i], "udef-int-", 9)
			       && !egg_strcasecmp (item[i] + 9, ul->name))))
		{
		  i++;
		  if (i >= items)
		    {
		      if (irp)
			Tcl_AppendResult (irp,
					  "this setting needs an argument",
					  NULL);
		      return TCL_ERROR;
		    }
		  setudef (ul, chan->dname, atoi (item[i]));
		  found = 1;
		  break;
		}
	    }
	  if (!found)
	    {
	      if (irp && item[i][0])
		Tcl_AppendResult (irp, "illegal channel option: ", item[i],
				  NULL);
	      x++;
	    }
	}
    }
#ifdef LEAF
  if (protect_readonly || chan_hack)
    {
      if (((old_status ^ chan->status) & CHAN_INACTIVE)
	  && module_find ("irc", 0, 0))
	{
	  if (channel_inactive (chan)
	      && (chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	    dprintf (DP_SERVER, "PART %s\n", chan->name);
	  if (!channel_inactive (chan)
	      && !(chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	    dprintf (DP_SERVER, "JOIN %s %s\n",
		     (chan->name[0]) ? chan->name : chan->dname,
		     chan->channel.key[0] ? chan->channel.key : chan->
		     key_prot);
	}
      Context;
      if ((old_status ^ chan->status) & (CHAN_ENFORCEBANS | CHAN_BITCH))
	{
	  if ((me = module_find ("irc", 0, 0)))
	    (me->funcs[IRC_RECHECK_CHANNEL]) (chan, 1);
	}
      else if (old_mode_pls_prot != chan->mode_pls_prot
	       || old_mode_mns_prot != chan->mode_mns_prot)
	if ((me = module_find ("irc", 0, 0)))
	  (me->funcs[IRC_RECHECK_CHANNEL_MODES]) (chan);
    }
  Context;
#endif
  if (x > 0)
    return TCL_ERROR;
  return TCL_OK;
}
static int
tcl_do_masklist (maskrec * m, Tcl_Interp * irp)
{
  char ts[21], ts1[21], ts2[21], *p;
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char *list[6];
#else
  char *list[6];
#endif
  for (; m; m = m->next)
    {
      list[0] = m->mask;
      list[1] = m->desc;
      sprintf (ts, "%lu", m->expire);
      list[2] = ts;
      sprintf (ts1, "%lu", m->added);
      list[3] = ts1;
      sprintf (ts2, "%lu", m->lastactive);
      list[4] = ts2;
      list[5] = m->user;
      p = Tcl_Merge (6, list);
      Tcl_AppendElement (irp, p);
      Tcl_Free ((char *) p);
    } return TCL_OK;
}
static int tcl_banlist STDVAR
{
  struct chanset_t *chan;
    BADARGS (1, 2, " ?channel?");
  if (argc == 2)
    {
      chan = findchan_by_dname (argv[1]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
	  return TCL_ERROR;
	}
      return tcl_do_masklist (chan->bans, irp);
    }
  return tcl_do_masklist (global_bans, irp);
}
static int tcl_exemptlist STDVAR
{
  struct chanset_t *chan;
    BADARGS (1, 2, " ?channel?");
  if (argc == 2)
    {
      chan = findchan_by_dname (argv[1]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
	  return TCL_ERROR;
	}
      return tcl_do_masklist (chan->exempts, irp);
    }
  return tcl_do_masklist (global_exempts, irp);
}
static int tcl_invitelist STDVAR
{
  struct chanset_t *chan;
    BADARGS (1, 2, " ?channel?");
  if (argc == 2)
    {
      chan = findchan_by_dname (argv[1]);
      if (chan == NULL)
	{
	  Tcl_AppendResult (irp, "invalid channel: ", argv[1], NULL);
	  return TCL_ERROR;
	}
      return tcl_do_masklist (chan->invites, irp);
    }
  return tcl_do_masklist (global_invites, irp);
}
static int tcl_channels STDVAR
{
  struct chanset_t *chan;
    BADARGS (1, 1, "");
  for (chan = chanset; chan; chan = chan->next)
    Tcl_AppendElement (irp, chan->dname);
    return TCL_OK;
}
#ifdef HUB
#endif
static int tcl_validchan STDVAR
{
  struct chanset_t *chan;
    BADARGS (2, 2, " channel");
    chan = findchan_by_dname (argv[1]);
  if (chan == NULL)
    Tcl_AppendResult (irp, "0", NULL);
  else
      Tcl_AppendResult (irp, "1", NULL);
    return TCL_OK;
}
static int tcl_isdynamic STDVAR
{
  struct chanset_t *chan;
    BADARGS (2, 2, " channel");
    chan = findchan_by_dname (argv[1]);
  if (chan != NULL)
    if (!channel_static (chan))
      {
	Tcl_AppendResult (irp, "1", NULL);
	return TCL_OK;
      }
  Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_getchaninfo STDVAR
{
  char s[161];
  struct userrec *u;
    BADARGS (3, 3, " handle channel");
    u = get_user_by_handle (userlist, argv[1]);
  if (!u || (u->flags & USER_BOT))
      return TCL_OK;
    get_handle_chaninfo (argv[1], argv[2], s);
    Tcl_AppendResult (irp, s, NULL);
    return TCL_OK;
}
static int tcl_setchaninfo STDVAR
{
  struct chanset_t *chan;
    BADARGS (4, 4, " handle channel info");
    chan = findchan_by_dname (argv[2]);
  if (chan == NULL)
    {
      Tcl_AppendResult (irp, "illegal channel: ", argv[2], NULL);
      return TCL_ERROR;
    }
  if (!egg_strcasecmp (argv[3], "none"))
    {
      set_handle_chaninfo (userlist, argv[1], argv[2], NULL);
      return TCL_OK;
    }
  set_handle_chaninfo (userlist, argv[1], argv[2], argv[3]);
  return TCL_OK;
}
static int tcl_setlaston STDVAR
{
  time_t t = now;
  struct userrec *u;
    BADARGS (2, 4, " handle ?channel? ?timestamp?");
    u = get_user_by_handle (userlist, argv[1]);
  if (!u)
    {
      Tcl_AppendResult (irp, "No such user: ", argv[1], NULL);
      return TCL_ERROR;
    }
  if (argc == 4)
    t = (time_t) atoi (argv[3]);
  if (argc == 3 && ((argv[2][0] != '#') && (argv[2][0] != '&')))
    t = (time_t) atoi (argv[2]);
  if (argc == 2
      || (argc == 3 && ((argv[2][0] != '#') && (argv[2][0] != '&'))))
    set_handle_laston ("*", u, t);
  else
    set_handle_laston (argv[2], u, t);
  return TCL_OK;
}
static int tcl_addchanrec STDVAR
{
  struct userrec *u;
    BADARGS (3, 3, " handle channel");
    u = get_user_by_handle (userlist, argv[1]);
  if (!u)
    {
      Tcl_AppendResult (irp, "0", NULL);
      return TCL_OK;
    }
  if (!findchan_by_dname (argv[2]))
    {
      Tcl_AppendResult (irp, "0", NULL);
      return TCL_OK;
    }
  if (get_chanrec (u, argv[2]) != NULL)
    {
      Tcl_AppendResult (irp, "0", NULL);
      return TCL_OK;
    }
  add_chanrec (u, argv[2]);
  Tcl_AppendResult (irp, "1", NULL);
  return TCL_OK;
}
static int tcl_delchanrec STDVAR
{
  struct userrec *u;
    BADARGS (3, 3, " handle channel");
    u = get_user_by_handle (userlist, argv[1]);
  if (!u)
    {
      Tcl_AppendResult (irp, "0", NULL);
      return TCL_OK;
    }
  if (get_chanrec (u, argv[2]) == NULL)
    {
      Tcl_AppendResult (irp, "0", NULL);
      return TCL_OK;
    }
  del_chanrec (u, argv[2]);
  Tcl_AppendResult (irp, "1", NULL);
  return TCL_OK;
}
static int tcl_haschanrec STDVAR
{
  struct userrec *u;
  struct chanset_t *chan;
  struct chanuserrec *chanrec;
    BADARGS (3, 3, " handle channel");
    chan = findchan_by_dname (argv[2]);
  if (chan == NULL)
    {
      Tcl_AppendResult (irp, "illegal channel: ", argv[2], NULL);
      return TCL_ERROR;
    }
  u = get_user_by_handle (userlist, argv[1]);
  if (!u)
    {
      Tcl_AppendResult (irp, "No such user: ", argv[1], NULL);
      return TCL_ERROR;
    }
  chanrec = get_chanrec (u, chan->dname);
  if (chanrec)
    {
      Tcl_AppendResult (irp, "1", NULL);
      return TCL_OK;
    }
  Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static void
init_masklist (masklist * m)
{
  m->mask = (char *) nmalloc (1);
  m->mask[0] = 0;
  m->who = NULL;
  m->next = NULL;
} static void
init_channel (struct chanset_t *chan, int reset)
{
  chan->channel.maxmembers = 0;
  chan->channel.mode = 0;
  chan->channel.members = 0;
  if (!reset)
    {
      chan->channel.key = (char *) nmalloc (1);
      chan->channel.key[0] = 0;
    }
  chan->channel.ban = (masklist *) nmalloc (sizeof (masklist));
  init_masklist (chan->channel.ban);
  chan->channel.exempt = (masklist *) nmalloc (sizeof (masklist));
  init_masklist (chan->channel.exempt);
  chan->channel.invite = (masklist *) nmalloc (sizeof (masklist));
  init_masklist (chan->channel.invite);
  chan->channel.member = (memberlist *) nmalloc (sizeof (memberlist));
  chan->channel.member->nick[0] = 0;
  chan->channel.member->next = NULL;
  chan->channel.topic = NULL;
}
static void
clear_masklist (masklist * m)
{
  masklist *temp;
  for (; m; m = temp)
    {
      temp = m->next;
      if (m->mask)
	nfree (m->mask);
      if (m->who)
	nfree (m->who);
      nfree (m);
    }
}
static void
clear_channel (struct chanset_t *chan, int reset)
{
  memberlist *m, *m1;
  if (chan->channel.topic)
    nfree (chan->channel.topic);
  for (m = chan->channel.member; m; m = m1)
    {
      m1 = m->next;
      nfree (m);
    }
  clear_masklist (chan->channel.ban);
  chan->channel.ban = NULL;
  clear_masklist (chan->channel.exempt);
  chan->channel.exempt = NULL;
  clear_masklist (chan->channel.invite);
  chan->channel.invite = NULL;
  if (reset)
    init_channel (chan, 1);
}
static int
tcl_channel_add (Tcl_Interp * irp, char *newname, char *options)
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
  if (!newname || !newname[0] || !strchr (CHANMETA, newname[0]))
    {
      if (irp)
	Tcl_AppendResult (irp, "invalid channel prefix", NULL);
      return TCL_ERROR;
    }
  if (strchr (newname, ',') != NULL)
    {
      if (irp)
	Tcl_AppendResult (irp, "invalid channel name", NULL);
      return TCL_ERROR;
    }
  convert_element (glob_chanmode, buf2);
  simple_sprintf (buf, "chanmode %s ", buf2);
  strncat (buf, glob_chanset, 2047 - strlen (buf));
  strncat (buf, options, 2047 - strlen (buf));
  buf[2047] = 0;
  if (Tcl_SplitList (NULL, buf, &items, &item) != TCL_OK)
    return TCL_ERROR;
  if ((chan = findchan_by_dname (newname)))
    {
      chan->status &= ~CHAN_FLAGGED;
    }
  else
    {
      chan = (struct chanset_t *) nmalloc (sizeof (struct chanset_t));
      egg_bzero (chan, sizeof (struct chanset_t));
      chan->limit_prot = 0;
      chan->limit = 0;
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
      chan->limitraise = 0;
      chan->ban_time = global_ban_time;
      chan->exempt_time = global_exempt_time;
      chan->invite_time = global_invite_time;
      strncpy (chan->dname, newname, 81);
      chan->dname[80] = 0;
      init_channel (chan, 0);
      list_append ((struct list_type **) &chanset, (struct list_type *) chan);
      join = 1;
  } if (setstatic)
    chan->status |= CHAN_STATIC;
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  if ((tcl_channel_modify (irp, chan, items, (char **) item) != TCL_OK)
      && !chan_hack)
    {
#else
  if ((tcl_channel_modify (irp, chan, items, item) != TCL_OK) && !chan_hack)
    {
#endif
      ret = TCL_ERROR;
    }
  Tcl_Free ((char *) item);
#ifdef LEAF
  if (join && !channel_inactive (chan) && module_find ("irc", 0, 0))
    dprintf (DP_SERVER, "JOIN %s %s\n", chan->dname, chan->key_prot);
#endif
  return ret;
}
static int tcl_setudef STDVAR
{
  int type;
    BADARGS (3, 3, " type name");
  if (!egg_strcasecmp (argv[1], "flag"))
      type = UDEF_FLAG;
  else if (!egg_strcasecmp (argv[1], "int"))
      type = UDEF_INT;
  else
    {
      Tcl_AppendResult (irp, "invalid type. Must be one of: flag, int", NULL);
      return TCL_ERROR;
    }
  initudef (type, argv[2], 1);
  return TCL_OK;
}
static int tcl_renudef STDVAR
{
  struct udef_struct *ul;
  int type, found = 0;
    BADARGS (4, 4, " type oldname newname");
  if (!egg_strcasecmp (argv[1], "flag"))
      type = UDEF_FLAG;
  else if (!egg_strcasecmp (argv[1], "int"))
      type = UDEF_INT;
  else
    {
      Tcl_AppendResult (irp, "invalid type. Must be one of: flag, int", NULL);
      return TCL_ERROR;
    }
  for (ul = udef; ul; ul = ul->next)
    {
      if (ul->type == type && !egg_strcasecmp (ul->name, argv[2]))
	{
	  nfree (ul->name);
	  ul->name = nmalloc (strlen (argv[3]) + 1);
	  strcpy (ul->name, argv[3]);
	  found = 1;
	}
    }
  if (!found)
    {
      Tcl_AppendResult (irp, "not found", NULL);
      return TCL_ERROR;
    }
  else
    return TCL_OK;
}
static int tcl_deludef STDVAR
{
  struct udef_struct *ul, *ull;
  int type, found = 0;
    BADARGS (3, 3, " type name");
  if (!egg_strcasecmp (argv[1], "flag"))
      type = UDEF_FLAG;
  else if (!egg_strcasecmp (argv[1], "int"))
      type = UDEF_INT;
  else
    {
      Tcl_AppendResult (irp, "invalid type. Must be one of: flag, int", NULL);
      return TCL_ERROR;
    }
  for (ul = udef; ul; ul = ul->next)
    {
      ull = ul->next;
      if (!ull)
	break;
      if (ull->type == type && !egg_strcasecmp (ull->name, argv[2]))
	{
	  ul->next = ull->next;
	  nfree (ull->name);
	  free_udef_chans (ull->values);
	  nfree (ull);
	  found = 1;
	}
    }
  if (udef)
    {
      if (udef->type == type && !egg_strcasecmp (udef->name, argv[2]))
	{
	  ul = udef->next;
	  nfree (udef->name);
	  free_udef_chans (udef->values);
	  nfree (udef);
	  udef = ul;
	  found = 1;
	}
    }
  if (!found)
    {
      Tcl_AppendResult (irp, "not found", NULL);
      return TCL_ERROR;
    }
  else
    return TCL_OK;
}
static tcl_cmds channels_cmds[] =
  { {"killban", tcl_killban}, {"killchanban", tcl_killchanban},
  {"isbansticky", tcl_isbansticky}, {"isban", tcl_isban}, {"ispermban",
							   tcl_ispermban},
  {"matchban", tcl_matchban}, {"newchanban", tcl_newchanban}, {"newban",
							       tcl_newban},
  {"killexempt", tcl_killexempt}, {"killchanexempt", tcl_killchanexempt},
  {"isexemptsticky", tcl_isexemptsticky}, {"isexempt", tcl_isexempt},
  {"ispermexempt", tcl_ispermexempt}, {"matchexempt", tcl_matchexempt},
  {"newchanexempt", tcl_newchanexempt}, {"newexempt", tcl_newexempt},
  {"killinvite", tcl_killinvite}, {"killchaninvite", tcl_killchaninvite},
  {"isinvitesticky", tcl_isinvitesticky}, {"isinvite", tcl_isinvite},
  {"isperminvite", tcl_isperminvite}, {"matchinvite", tcl_matchinvite},
  {"newchaninvite", tcl_newchaninvite}, {"newinvite", tcl_newinvite},
  {"channel", tcl_channel}, {"channels", tcl_channels}, {"exemptlist",
							 tcl_exemptlist},
  {"invitelist", tcl_invitelist}, {"banlist", tcl_banlist},
#ifdef HUB
#endif
{"validchan", tcl_validchan}, {"isdynamic", tcl_isdynamic}, {"getchaninfo",
							     tcl_getchaninfo},
  {"setchaninfo", tcl_setchaninfo}, {"setlaston", tcl_setlaston},
  {"addchanrec", tcl_addchanrec}, {"delchanrec", tcl_delchanrec}, {"stick",
								   tcl_stick},
  {"unstick", tcl_stick}, {"stickinvite", tcl_stickinvite}, {"unstickinvite",
							     tcl_stickinvite},
  {"stickexempt", tcl_stickexempt}, {"unstickexempt", tcl_stickexempt},
  {"setudef", tcl_setudef}, {"renudef", tcl_renudef}, {"deludef",
						       tcl_deludef},
  {"haschanrec", tcl_haschanrec}, {NULL, NULL}
};
