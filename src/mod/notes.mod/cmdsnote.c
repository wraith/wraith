static void
cmd_pls_noteign (struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  char *handle, *mask, *buf, *p;
  if (!par[0])
    {
      dprintf (idx, "%s: +noteign [handle] <ignoremask>\n", NOTES_USAGE);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# +noteign %s", dcc[idx].nick, par);
  p = buf = nmalloc (strlen (par) + 1);
  strcpy (p, par);
  handle = newsplit (&p);
  mask = newsplit (&p);
  if (mask[0])
    {
      u2 = get_user_by_handle (userlist, handle);
      if (u != u2)
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
	  get_user_flagrec (u, &fr, dcc[idx].u.chat->con_chan);
	  if (!(glob_master (fr) || glob_owner (fr)))
	    {
	      dprintf (idx, NOTES_IGN_OTHERS, handle);
	      nfree (buf);
	      return;
	    }
	}
      if (!u2)
	{
	  dprintf (idx, NOTES_UNKNOWN_USER, handle);
	  nfree (buf);
	  return;
	}
    }
  else
    {
      u2 = u;
      mask = handle;
    }
  if (add_note_ignore (u2, mask))
    dprintf (idx, NOTES_IGN_NEW, mask);
  else
    dprintf (idx, NOTES_IGN_ALREADY, mask);
  nfree (buf);
  return;
}
static void
cmd_mns_noteign (struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  char *handle, *mask, *buf, *p;
  if (!par[0])
    {
      dprintf (idx, "%s: -noteign [handle] <ignoremask>\n", NOTES_USAGE);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# -noteign %s", dcc[idx].nick, par);
  p = buf = nmalloc (strlen (par) + 1);
  strcpy (p, par);
  handle = newsplit (&p);
  mask = newsplit (&p);
  if (mask[0])
    {
      u2 = get_user_by_handle (userlist, handle);
      if (u != u2)
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
	  get_user_flagrec (u, &fr, dcc[idx].u.chat->con_chan);
	  if (!(glob_master (fr) || glob_owner (fr)))
	    {
	      dprintf (idx, NOTES_IGN_OTHERS, handle);
	      nfree (buf);
	      return;
	    }
	}
      if (!u2)
	{
	  dprintf (idx, NOTES_UNKNOWN_USER, handle);
	  nfree (buf);
	  return;
	}
    }
  else
    {
      u2 = u;
      mask = handle;
    }
  if (del_note_ignore (u2, mask))
    dprintf (idx, NOTES_IGN_REM, mask);
  else
    dprintf (idx, NOTES_IGN_NOTFOUND, mask);
  nfree (buf);
  return;
}
static void
cmd_noteigns (struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  char **ignores;
  int ignoresn, i;
  if (par[0])
    {
      u2 = get_user_by_handle (userlist, par);
      if (u != u2)
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
	  get_user_flagrec (u, &fr, dcc[idx].u.chat->con_chan);
	  if (!(glob_master (fr) || glob_owner (fr)))
	    {
	      dprintf (idx, NOTES_IGN_OTHERS, par);
	      return;
	    }
	}
      if (!u2)
	{
	  dprintf (idx, NOTES_UNKNOWN_USER, par);
	  return;
	}
    }
  else
    u2 = u;
  ignoresn = get_note_ignores (u2, &ignores);
  if (!ignoresn)
    {
      dprintf (idx, "%s", NOTES_IGN_NONE);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# noteigns %s", dcc[idx].nick, par);
  dprintf (idx, NOTES_IGN_FOR, u2->handle);
  for (i = 0; i < ignoresn; i++)
    dprintf (idx, " %s", ignores[i]);
  dprintf (idx, "\n");
  nfree (ignores[0]);
  nfree (ignores);
}
static void
cmd_fwd (struct userrec *u, int idx, char *par)
{
  char *handle;
  struct userrec *u1;
  if (!par[0])
    {
      dprintf (idx, "%s: fwd <handle> [user@bot]\n", NOTES_USAGE);
      return;
    }
  handle = newsplit (&par);
  u1 = get_user_by_handle (userlist, handle);
  if (!u1)
    {
      dprintf (idx, "%s\n", NOTES_NO_SUCH_USER);
      return;
    }
  if ((u1->flags & USER_OWNER) && egg_strcasecmp (handle, dcc[idx].nick))
    {
      dprintf (idx, "%s\n", NOTES_FWD_OWNER);
      return;
    }
  if (!par[0])
    {
      putlog (LOG_CMDS, "*", "#%s# fwd %s", dcc[idx].nick, handle);
      dprintf (idx, NOTES_FWD_FOR, handle);
      set_user (&USERENTRY_FWD, u1, NULL);
      return;
    }
  if (strchr (par, '@') == NULL)
    {
      dprintf (idx, "%s\n", NOTES_FWD_BOTNAME);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# fwd %s %s", dcc[idx].nick, handle, par);
  dprintf (idx, NOTES_FWD_CHANGED, handle, par);
  set_user (&USERENTRY_FWD, u1, par);
}
static void
cmd_notes (struct userrec *u, int idx, char *par)
{
  char *fcn;
  if (!par[0])
    {
      dprintf (idx, "%s: notes index\n", NOTES_USAGE);
      dprintf (idx, "       notes read <# or ALL>\n");
      dprintf (idx, "       notes erase <# or ALL>\n");
      dprintf (idx, "       %s\n", NOTES_MAYBE);
      dprintf (idx, "       ex: notes erase 2-4;8;16-\n");
      return;
    }
  fcn = newsplit (&par);
  if (!egg_strcasecmp (fcn, "index"))
    notes_read (dcc[idx].nick, "", "+", idx);
  else if (!egg_strcasecmp (fcn, "read"))
    {
      if (!egg_strcasecmp (par, "all"))
	notes_read (dcc[idx].nick, "", "-", idx);
      else
	notes_read (dcc[idx].nick, "", par, idx);
    }
  else if (!egg_strcasecmp (fcn, "erase"))
    {
      if (!egg_strcasecmp (par, "all"))
	notes_del (dcc[idx].nick, "", "-", idx);
      else
	notes_del (dcc[idx].nick, "", par, idx);
    }
  else
    {
      dprintf (idx, "%s\n", NOTES_MUSTBE);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# notes %s %s", dcc[idx].nick, fcn, par);
}
static void
cmd_note (struct userrec *u, int idx, char *par)
{
  char handle[512], *p;
  int echo;
  p = newsplit (&par);
  if (!par[0])
    {
      dprintf (idx, "%s: note <to-whom> <message>\n", NOTES_USAGE);
      return;
    }
  while ((*par == ' ') || (*par == '<') || (*par == '>'))
    par++;
  echo = (dcc[idx].status & STAT_ECHO);
  splitc (handle, p, ',');
  while (handle[0])
    {
      rmspace (handle);
      add_note (handle, dcc[idx].nick, par, idx, echo);
      splitc (handle, p, ',');
    }
  rmspace (p);
  add_note (p, dcc[idx].nick, par, idx, echo);
}
static dcc_cmd_t notes_cmds[] =
  { {"fwd", "m", (Function) cmd_fwd, NULL, NULL}, {"notes", "",
						   (Function) cmd_notes, NULL,
						   NULL}, {"+noteign", "",
							   (Function)
							   cmd_pls_noteign,
							   NULL, NULL},
  {"-noteign", "", (Function) cmd_mns_noteign, NULL, NULL}, {"noteigns", "",
							     (Function)
							     cmd_noteigns,
							     NULL, NULL},
  {"note", "", (Function) cmd_note, NULL, NULL}, {NULL, NULL, NULL, NULL,
						  NULL} };
