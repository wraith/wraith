/*
 * cmdsnote.c -- part of notes.mod
 *   handles all notes interaction over the party line
 *
 */


static void cmd_fwd(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "%s: fwd <handle> [user@bot]\n", NOTES_USAGE);
    return;
  }

  struct userrec *u1 = get_user_by_handle(userlist, newsplit(&par));

  if (!u1) {
    dprintf(idx, "%s\n", NOTES_NO_SUCH_USER);
    return;
  }

  if ((u1->flags & USER_OWNER) && egg_strcasecmp(u1->handle, dcc[idx].nick)) {
    dprintf(idx, "%s\n", NOTES_FWD_OWNER);
    return;
  }
  if (!par[0]) {
    putlog(LOG_CMDS, "*", "#%s# fwd %s", dcc[idx].nick, u1->handle);
    dprintf(idx, NOTES_FWD_FOR, u1->handle);
    set_user(&USERENTRY_FWD, u1, NULL);
    return;
  }
  /* Thanks to vertex & dw */
  if (strchr(par, '@') == NULL) {
    dprintf(idx, "%s\n", NOTES_FWD_BOTNAME);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# fwd %s %s", dcc[idx].nick, u1->handle, par);
  dprintf(idx, NOTES_FWD_CHANGED, u1->handle, par);
  set_user(&USERENTRY_FWD, u1, par);
}

static void cmd_notes(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "%s: notes index\n", NOTES_USAGE);
    dprintf(idx, "       notes read <# or ALL>\n");
    dprintf(idx, "       notes erase <# or ALL>\n");
    dprintf(idx, "       %s\n", NOTES_MAYBE);
    dprintf(idx, "       ex: notes erase 2-4;8;16-\n");
    return;
  }
  char *fcn = newsplit(&par);

  if (!egg_strcasecmp(fcn, "index"))
    notes_read(dcc[idx].nick, "", "+", idx);
  else if (!egg_strcasecmp(fcn, "read")) {
    if (!egg_strcasecmp(par, "all"))
      notes_read(dcc[idx].nick, "", "-", idx);
    else
      notes_read(dcc[idx].nick, "", par, idx);
  } else if (!egg_strcasecmp(fcn, "erase")) {
    if (!egg_strcasecmp(par, "all"))
      notes_del(dcc[idx].nick, "", "-", idx);
    else
      notes_del(dcc[idx].nick, "", par, idx);
  } else {
    dprintf(idx, "%s\n", NOTES_MUSTBE);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# notes %s %s", dcc[idx].nick, fcn, par);
}

static void cmd_note(int idx, char *par)
{
  char handle[512] = "", *p = newsplit(&par);
  int echo;

  if (!par[0]) {
    dprintf(idx, "%s: note <to-whom> <message>\n", NOTES_USAGE);
    return;
  }
  while ((*par == ' ') || (*par == '<') || (*par == '>'))
    par++;			/* These are now illegal *starting* notes
				 * characters */
  echo = (dcc[idx].status & STAT_ECHO);
  splitc(handle, p, ',');
  while (handle[0]) {
    rmspace(handle);
    add_note(handle, dcc[idx].nick, par, idx, echo);
    splitc(handle, p, ',');
  }
  rmspace(p);
  add_note(p, dcc[idx].nick, par, idx, echo);
}

static cmd_t notes_cmds[] =
{
  {"fwd",	"m",	(Function) cmd_fwd,		NULL, 0},
  {"notes",	"",	(Function) cmd_notes,		NULL, 0},
  {"note",	"",	(Function) cmd_note,		NULL, 0},
  {NULL,	NULL,	NULL,				NULL, 0}
};
