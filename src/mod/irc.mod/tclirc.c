#ifdef LEAF
/*
 * tclirc.c -- part of irc.mod
 *
 */


/* flushmode <chan> */
static int tcl_flushmode STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "invalid channel: ", argv[1], NULL);
    return TCL_ERROR;
  }
  flush_mode(chan, NORMAL);
  return TCL_OK;
}

static int tcl_resetbans STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "invalid channel ", argv[1], NULL);
    return TCL_ERROR;
  }
  resetbans(chan);
  return TCL_OK;
}

static int tcl_resetexempts STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "invalid channel ", argv[1], NULL);
    return TCL_ERROR;
  }
  resetexempts(chan);
  return TCL_OK;
}

static int tcl_resetinvites STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "invalid channel ", argv[1], NULL);
    return TCL_ERROR;
  }
  resetinvites(chan);
  return TCL_OK;
}

static int tcl_resetchan STDVAR
{
  struct chanset_t *chan;

  BADARGS(2, 2, " channel");
  chan = findchan_by_dname(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, "invalid channel ", argv[1], NULL);
    return TCL_ERROR;
  }
  reset_chan_info(chan);
  return TCL_OK;
}

static tcl_cmds tclchan_cmds[] =
{
  {"flushmode",		tcl_flushmode},
  {"resetbans",		tcl_resetbans},
  {"resetexempts",	tcl_resetexempts},
  {"resetinvites",	tcl_resetinvites},
  {"resetchan",		tcl_resetchan},
  {NULL,		NULL}
};

#endif /* LEAF */
