#ifdef LEAF
/*
 * tclserv.c -- part of server.mod
 *
 */

static int tcl_putquick STDVAR
{
  char s[511], *p;

  BADARGS(2, 3, " text ?options?");
  if ((argc == 3) &&
      egg_strcasecmp(argv[2], "-next") && egg_strcasecmp(argv[2], "-normal")) {
      Tcl_AppendResult(irp, "unknown putquick option: should be one of: ",
		       "-normal -next", NULL);
    return TCL_ERROR;
  }
  strncpy(s, argv[1], 510);
  s[510] = 0;
  p = strchr(s, '\n');
  if (p != NULL)
    *p = 0;
   p = strchr(s, '\r');
  if (p != NULL)
    *p = 0;
  if (argc == 3 && !egg_strcasecmp(argv[2], "-next"))
    dprintf(DP_MODE_NEXT, "%s\n", s);
  else
    dprintf(DP_MODE, "%s\n", s);
  return TCL_OK;
}

static int tcl_putserv STDVAR
{
  char s[511], *p;

  BADARGS(2, 3, " text ?options?");
  if ((argc == 3) &&
    egg_strcasecmp(argv[2], "-next") && egg_strcasecmp(argv[2], "-normal")) {
    Tcl_AppendResult(irp, "unknown putserv option: should be one of: ",
		     "-normal -next", NULL);
    return TCL_ERROR;
  }
  strncpy(s, argv[1], 510);
  s[510] = 0;
  p = strchr(s, '\n');
  if (p != NULL)
    *p = 0;
   p = strchr(s, '\r');
  if (p != NULL)
    *p = 0;
  if (argc == 3 && !egg_strcasecmp(argv[2], "-next"))
    dprintf(DP_SERVER_NEXT, "%s\n", s);
  else
    dprintf(DP_SERVER, "%s\n", s);
  return TCL_OK;
}

static int tcl_puthelp STDVAR
{
  char s[511], *p;

  BADARGS(2, 3, " text ?options?");
  if ((argc == 3) &&
    egg_strcasecmp(argv[2], "-next") && egg_strcasecmp(argv[2], "-normal")) {
    Tcl_AppendResult(irp, "unknown puthelp option: should be one of: ",
		     "-normal -next", NULL);
    return TCL_ERROR;
  }
  strncpy(s, argv[1], 510);
  s[510] = 0;
  p = strchr(s, '\n');
  if (p != NULL)
    *p = 0;
   p = strchr(s, '\r');
  if (p != NULL)
    *p = 0;
  if (argc == 3 && !egg_strcasecmp(argv[2], "-next"))
    dprintf(DP_HELP_NEXT, "%s\n", s);
  else
    dprintf(DP_HELP, "%s\n", s);
  return TCL_OK;
}

static tcl_cmds my_tcl_cmds[] =
{
  {"puthelp",		tcl_puthelp},
  {"putserv",		tcl_putserv},
  {"putquick",		tcl_putquick},
  {NULL,		NULL},
};
#endif
