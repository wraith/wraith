/*
 * tcldcc.c -- handles:
 *   Tcl stubs for the dcc commands
 *
 */

#include "main.h"
#include "tandem.h"
#include "modules.h"

#include <sys/stat.h>

extern Tcl_Interp	*interp;
extern struct dcc_t	*dcc;

extern int		 dcc_total, backgrd, parties,
			 do_restart, remote_boots, max_dcc, hub, leaf;

extern char		 botnetnick[], *binname;
extern party_t		*party;
extern tand_t		*tandbot;
extern time_t		 now;

/* Traffic stuff. */
extern unsigned long otraffic_irc, otraffic_irc_today, itraffic_irc, itraffic_irc_today, otraffic_bn, otraffic_bn_today, itraffic_bn, itraffic_bn_today, otraffic_dcc, otraffic_dcc_today, itraffic_dcc, itraffic_dcc_today, otraffic_trans, otraffic_trans_today, itraffic_trans, itraffic_trans_today, otraffic_unknown, otraffic_unknown_today, itraffic_unknown, itraffic_unknown_today;

int			 enable_simul = 0;

int expmem_tcldcc(void)
{
  int tot = 0;
  return tot;
}

/***********************************************************************/

static int tcl_putdcc STDVAR
{
  int i, j;

  BADARGS(3, 3, " idx text");
  i = atoi(argv[1]);
  j = findidx(i);
  if (j < 0) {
    Tcl_AppendResult(irp, "invalid idx", NULL);
    return TCL_ERROR;
  }
  dumplots(-i, "", argv[2]);
  return TCL_OK;
}

static int tcl_hand2idx STDVAR
{
  int i;
  char s[11];

  BADARGS(2, 2, " nickname");
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_SIMUL) &&
        !egg_strcasecmp(argv[1], dcc[i].nick)) {
      egg_snprintf(s, sizeof s, "%ld", dcc[i].sock);
      Tcl_AppendResult(irp, s, NULL);
      return TCL_OK;
    }
  Tcl_AppendResult(irp, "-1", NULL);
  return TCL_OK;
}

static int tcl_valididx STDVAR
{
  int idx;

  BADARGS(2, 2, " idx");
  idx = findidx(atoi(argv[1]));
  if (idx < 0 || !(dcc[idx].type->flags & DCT_VALIDIDX))
     Tcl_AppendResult(irp, "0", NULL);
  else
     Tcl_AppendResult(irp, "1", NULL);
   return TCL_OK;
}

static int tcl_killdcc STDVAR
{
  int idx;

  BADARGS(2, 3, " idx ?reason?");
  idx = findidx(atoi(argv[1]));
  if (idx < 0) {
    Tcl_AppendResult(irp, "invalid idx", NULL);
    return TCL_ERROR;
  }
  /* Don't kill terminal socket */
  if ((dcc[idx].sock == STDOUT) && !backgrd)
    return TCL_OK;
  /* Make sure 'whom' info is updated for other bots */
  if (dcc[idx].type->flags & DCT_CHAT) {
    chanout_but(idx, dcc[idx].u.chat->channel, "*** %s has left the %s%s%s\n",
		dcc[idx].nick, dcc[idx].u.chat ? "channel" : "partyline",
		argc == 3 ? ": " : "", argc == 3 ? argv[2] : "");
    botnet_send_part_idx(idx, argc == 3 ? argv[2] : "");
    if ((dcc[idx].u.chat->channel >= 0) &&
	(dcc[idx].u.chat->channel < GLOBAL_CHANS))
      check_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock,
		     dcc[idx].u.chat->channel);
    check_chof(dcc[idx].nick, idx);
    /* Notice is sent to the party line, the script can add a reason. */
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
  return TCL_OK;
}

static int tcl_putbot STDVAR
{
  int i;
  char msg[SGRAB-110];

  BADARGS(3, 3, " botnick message");
  i = nextbot(argv[1]);
  if (i < 0) {
    Tcl_AppendResult(irp, "bot is not in the botnet", NULL);
    return TCL_ERROR;
  }
  strncpyz(msg, argv[2], sizeof msg);
  botnet_send_zapf(i, botnetnick, argv[1], msg);
  return TCL_OK;
}

static int tcl_putallbots STDVAR
{
  char msg[SGRAB-110];

  BADARGS(2, 2, " message");
  strncpyz(msg, argv[1], sizeof msg);
  botnet_send_zapf_broad(-1, botnetnick, NULL, msg);
  return TCL_OK;
}

static int tcl_idx2hand STDVAR
{
  int idx;

  BADARGS(2, 2, " idx");
  idx = findidx(atoi(argv[1]));
  if (idx < 0) {
    Tcl_AppendResult(irp, "invalid idx", NULL);
    return TCL_ERROR;
  }
  Tcl_AppendResult(irp, dcc[idx].nick, NULL);
  return TCL_OK;
}

static int tcl_islinked STDVAR
{
  int i;

  BADARGS(2, 2, " bot");
  i = nextbot(argv[1]);
  if (i < 0)
     Tcl_AppendResult(irp, "0", NULL);
  else
     Tcl_AppendResult(irp, "1", NULL);
   return TCL_OK;
}

static int tcl_randstring STDVAR
{
 int length = atoi(argv[1]);
 char s[length+1];

 BADARGS(2, 2, " length");
 if (length) {
  make_rand_str(s,length);
  Tcl_AppendResult(irp, s, NULL);
 } else
  Tcl_AppendResult(irp, "", NULL);
 return TCL_OK;
}

static int tcl_binname STDVAR
{
 Tcl_AppendResult(irp, binname, NULL);
 return TCL_OK;
}

static int tcl_bots STDVAR
{
  tand_t *bot;

  BADARGS(1, 1, "");
  for (bot = tandbot; bot; bot = bot->next)
     Tcl_AppendElement(irp, bot->bot);
   return TCL_OK;
}

static int tcl_botlist STDVAR
{
  tand_t *bot;
  char *p;
  char sh[2], string[20];
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
    CONST char *list[4];
#else
    char *list[4];
#endif

  BADARGS(1, 1, "");
  sh[1] = 0;
  list[3] = sh;
  list[2] = string;
  for (bot = tandbot; bot; bot = bot->next) {
    list[0] = bot->bot;
    list[1] = (bot->uplink == (tand_t *) 1) ? botnetnick : bot->uplink->bot;
    strncpyz(string, int_to_base10(bot->ver), sizeof string);
    sh[0] = bot->share;
    p = Tcl_Merge(4, list);
    Tcl_AppendElement(irp, p);
    Tcl_Free((char *) p);
  }
  return TCL_OK;
}

static int tcl_link STDVAR
{
  int x, i;
  char bot[HANDLEN + 1], bot2[HANDLEN + 1];

  BADARGS(2, 3, " ?via-bot? bot");
  strncpyz(bot, argv[1], sizeof bot);
  if (argc == 3) {
    x = 1;
    strncpyz(bot2, argv[2], sizeof bot2);
    i = nextbot(bot);
    if (i < 0)
      x = 0;
    else
      botnet_send_link(i, botnetnick, bot, bot2);
  } else
     x = botlink("", -2, bot);
  egg_snprintf(bot, sizeof bot, "%d", x);
  Tcl_AppendResult(irp, bot, NULL);
  return TCL_OK;
}

static int tcl_unlink STDVAR
{
  int i, x;
  char bot[HANDLEN + 1];

  BADARGS(2, 3, " bot ?comment?");
  strncpyz(bot, argv[1], sizeof bot);
  i = nextbot(bot);
  if (i < 0)
     x = 0;
  else {
    x = 1;
    if (!egg_strcasecmp(bot, dcc[i].nick))
      x = botunlink(-2, bot, argv[2]);
    else
      botnet_send_unlink(i, botnetnick, lastbot(bot), bot, argv[2]);
  }
  egg_snprintf(bot, sizeof bot, "%d", x);
  Tcl_AppendResult(irp, bot, NULL);
  return TCL_OK;
}

static int tcl_rehash STDVAR
{
  BADARGS(1, 1, " ");
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", USERF_REHASHING);
  do_restart = -2;
  return TCL_OK;
}

static int tcl_restart STDVAR
{
  BADARGS(1, 1, " ");
  if (!backgrd) {
    Tcl_AppendResult(interp, "You can't restart a -n bot", NULL);
    return TCL_ERROR;
  }
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", MISC_RESTARTING);
  do_restart = -1;
  return TCL_OK;
}

tcl_cmds tcldcc_cmds[] =
{
  {"binname",		tcl_binname},
  {"putidx",		tcl_putdcc},
  {"hand2idx",		tcl_hand2idx},
  {"valididx",		tcl_valididx},
  {"killdcc",		tcl_killdcc},
  {"putbot",		tcl_putbot},
  {"putallbots",	tcl_putallbots},
  {"idx2hand",		tcl_idx2hand},
  {"bots",		tcl_bots},
  {"botlist",		tcl_botlist},
  {"islinked",		tcl_islinked},
  {"randstring",        tcl_randstring},
  {"link",		tcl_link},
  {"unlink",		tcl_unlink},
  {"rehash",		tcl_rehash},
  {"restart",		tcl_restart},
  {NULL,		NULL}
};
