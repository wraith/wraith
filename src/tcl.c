/*
 * tcl.c -- handles:
 *   the code for every command eggdrop adds to Tcl
 *   Tcl initialization
 *   getting and setting Tcl/eggdrop variables
 *
 */

#include <stdlib.h>		/* getenv()				*/
#include <locale.h>		/* setlocale()				*/
#include "common.h"
#include "misc.h"
#include "chanprog.h"
#include <sys/stat.h>


#if ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 1)) || (TCL_MAJOR_VERSION > 8)
#define USE_BYTE_ARRAYS
#endif

/* Used for read/write to internal strings */
typedef struct {
  char *str;			/* Pointer to actual string in eggdrop	     */
  int max;			/* max length (negative: read-only var
				   when protect is on) (0: read-only ALWAYS) */
  int flags;			/* 1 = directory			     */
} strinfo;

typedef struct {
  int *var;
  int ro;
} intinfo;

extern time_t	online_since;
extern int	backgrd, flood_telnet_thr, flood_telnet_time,
		shtime, allow_new_telnets, use_telnet_banner,
		default_flags, conmask, connect_timeout,
		firewallport, notify_users_at, flood_thr, ignore_time,
		reserved_port_min, reserved_port_max, localhub,
		enable_simul, dcc_total, debug_output, identtimeout,
		protect_telnet, dupwait_timeout, egg_numver, share_unlinks,
		dcc_sanitycheck, sort_users, tands, resolve_timeout,
		default_uflags, strict_host, userfile_perm;
extern char	origbotname[], botuser[], motdfile[], admin[], userfile[],
                firewall[], hostname[], hostname6[], myip[], myip6[],
		tempdir[], owner[], network[], botnetnick[],
		egg_version[], natip[], 
		dcc_prefix[];

extern struct dcc_t	*dcc;

int	    protect_readonly = 0;	/* turn on/off readonly protection */
char	    whois_fields[1025] = "";	/* fields to display in a .whois */
Tcl_Interp *interp;			/* eggdrop always uses the same
					   interpreter */
int	    dcc_flood_thr = 3;
int	    use_invites = 1;		/* Jason/drummer */
int	    use_exempts = 1;		/* Jason/drummer */
int	    force_expire = 0;		/* Rufus */
int	    remote_boots = 2;
int	    allow_dk_cmds = 1;
int	    must_be_owner = 1;
int	    max_dcc = 200;		/* needs at least 4 or 5 just to
					   get started. 20 should be enough   */
int	    quiet_save = 1;             /* quiet-save patch by Lucas	      */
int	    strtot = 0;
int 	    handlen = HANDLEN;
int	    utftot = 0;
int	    clientdata_stuff = 0;


/* Prototypes for tcl */
Tcl_Interp *Tcl_CreateInterp();

static void botnet_change(char *new)
{
  if (egg_strcasecmp(botnetnick, new)) {
    /* Trying to change bot's nickname */
    if (tands > 0) {
      putlog(LOG_MISC, "*", "* Tried to change my botnet nick, but I'm still linked to a botnet.");
      putlog(LOG_MISC, "*", "* (Unlink and try again.)");
      return;
    } else {
      if (botnetnick[0])
	putlog(LOG_MISC, "*", "* IDENTITY CHANGE: %s -> %s", botnetnick, new);
      strcpy(botnetnick, new);
    }
  }
}


/*
 *     Vars, traces, misc
 */

int init_dcc_max();

/* Used for read/write to integer couplets */
typedef struct {
  int *left;			/* left side of couplet */
  int *right;			/* right side */
} coupletinfo;

/* Read/write integer couplets (int1:int2) */
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *tcl_eggcouplet(ClientData cdata, Tcl_Interp *irp, CONST char *name1,
                            CONST char *name2, int flags)
#else
static char *tcl_eggcouplet(ClientData cdata, Tcl_Interp *irp, char *name1,
			    char *name2, int flags)
#endif
{
  char *s, s1[41];
  coupletinfo *cp = (coupletinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    egg_snprintf(s1, sizeof s1, "%d:%d", *(cp->left), *(cp->right));
    Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, name1,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		   tcl_eggcouplet, cdata);
  } else {			/* writes */
    s = (char *) Tcl_GetVar2(interp, name1, name2, 0);
    if (s != NULL) {
      int nr1, nr2;

      if (strlen(s) > 40)
	s[40] = 0;
      sscanf(s, "%d%*c%d", &nr1, &nr2);
      *(cp->left) = nr1;
      *(cp->right) = nr2;
    }
  }
  return NULL;
}

/* Read or write normal integer.
 */
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *tcl_eggint(ClientData cdata, Tcl_Interp *irp, CONST char *name1,
			CONST char *name2, int flags)
#else
static char *tcl_eggint(ClientData cdata, Tcl_Interp *irp, char *name1,
                        char *name2, int flags)
#endif
{
  char *s, s1[40];
  long l;
  intinfo *ii = (intinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    /* Special cases */
    if ((int *) ii->var == &conmask)
      strcpy(s1, masktype(conmask));
    else if ((int *) ii->var == &default_flags) {
      struct flag_record fr = {FR_GLOBAL, 0, 0, 0, 0, 0};
      fr.global = default_flags;
      fr.udef_global = default_uflags;
      build_flags(s1, &fr, 0);
    } else if ((int *) ii->var == &userfile_perm) {
      egg_snprintf(s1, sizeof s1, "0%o", userfile_perm);
    } else
      egg_snprintf(s1, sizeof s1, "%d", *(int *) ii->var);
    Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, name1,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		   tcl_eggint, cdata);
    return NULL;
  } else {			/* Writes */
    s = (char *) Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (s != NULL) {
      if ((int *) ii->var == &conmask) {
	if (s[0])
	  conmask = logmodes(s);
	else
	  conmask = LOG_MODES | LOG_MISC | LOG_CMDS;
      } else if ((int *) ii->var == &default_flags) {
	struct flag_record fr = {FR_GLOBAL, 0, 0, 0, 0, 0};

	break_down_flags(s, &fr, 0);
	default_flags = sanity_check(fr.global); /* drummer */
	default_uflags = fr.udef_global;
      } else if ((int *) ii->var == &userfile_perm) {
	int p = oatoi(s);

	if (p <= 0)
	  return "invalid userfile permissions";
	userfile_perm = p;
      } else if ((ii->ro == 2) || ((ii->ro == 1) && protect_readonly)) {
	return "read-only variable";
      } else {
	if (Tcl_ExprLong(interp, s, &l) == TCL_ERROR)
	  return interp->result;
	if ((int *) ii->var == &max_dcc) {
	  if (l < max_dcc)
	    return "you can't DECREASE max-dcc";
	  max_dcc = l;
	  init_dcc_max();
	} else
	  *(ii->var) = (int) l;
      }
    }
    return NULL;
  }
}

/* Read/write normal string variable
 */
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *tcl_eggstr(ClientData cdata, Tcl_Interp *irp, CONST char *name1,
                        CONST char *name2, int flags)
#else
static char *tcl_eggstr(ClientData cdata, Tcl_Interp *irp, char *name1,
			char *name2, int flags)
#endif
{
  char *s;
  strinfo *st = (strinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    if ((st->str == firewall) && (firewall[0])) {
      char s1[127];

      egg_snprintf(s1, sizeof s1, "%s:%d", firewall, firewallport);
      Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    } else
      Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS) {
      Tcl_TraceVar(interp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES |
		   TCL_TRACE_UNSETS, tcl_eggstr, cdata);
      if ((st->max <= 0) && (protect_readonly || (st->max == 0)))
	return "read-only variable"; /* it won't return the error... */
    }
    return NULL;
  } else {			/* writes */
    if ((st->max <= 0) && (protect_readonly || (st->max == 0))) {
      Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
      return "read-only variable";
    }
#ifdef USE_BYTE_ARRAYS
#undef malloc
#undef free
    {
         Tcl_Obj *obj;
         unsigned char *bytes;
         int len;

         obj = Tcl_GetVar2Ex(interp, name1, name2, 0);
         if (!obj) return(NULL);
         len = 0;
         bytes = Tcl_GetByteArrayFromObj(obj, &len);
         if (!bytes) return(NULL);
         s = malloc(len+1);
         memcpy(s, bytes, len);
         s[len] = 0;
    }
#else
    s = (char *) Tcl_GetVar2(interp, name1, name2, 0);
#endif
    if (s != NULL) {
      if (strlen(s) > abs(st->max))
	s[abs(st->max)] = 0;
      if (st->str == botnetnick)
	botnet_change(s);
      else if (st->str == firewall) {
	splitc(firewall, s, ':');
	if (!firewall[0])
	  strcpy(firewall, s);
	else
	  firewallport = atoi(s);
      } else
	strcpy(st->str, s);
      if ((st->flags) && (s[0])) {
	if (st->str[strlen(st->str) - 1] != '/')
	  strcat(st->str, "/");
      }
#ifdef USE_BYTE_ARRAYS
      free(s);
#endif
    }
    return NULL;
  }
}

static tcl_coups def_tcl_coups[] =
{
  {"telnet-flood",	&flood_telnet_thr,	&flood_telnet_time},
  {"reserved-portrange", &reserved_port_min, &reserved_port_max},
  {NULL,		NULL,			NULL}
};

/* Set up Tcl variables that will hook into eggdrop internal vars via
 * trace callbacks.
 */

void add_tcl_coups(tcl_coups *); 		/* prototype */

static void init_traces()
{
  add_tcl_coups(def_tcl_coups);
}


/* Not going through Tcl's crazy main() system (what on earth was he
 * smoking?!) so we gotta initialize the Tcl interpreter
 */
void init_tcl(int argc, char **argv)
{
#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1) || (TCL_MAJOR_VERSION > 8)
  const char *encoding;
  char *langEnv;
#endif
  int j;
  char pver[1024] = "";

/* This must be done *BEFORE* Tcl_SetSystemEncoding(),
 * or Tcl_SetSystemEncoding() will cause a segfault.
 */
  /* This is used for 'info nameofexecutable'.
   * The filename in argv[0] must exist in a directory listed in
   * the environment variable PATH for it to register anything.
   */
  Tcl_FindExecutable(argv[0]);

  /* Initialize the interpreter */
  interp = Tcl_CreateInterp();

  /* Set Tcl variable tcl_interactive to 0 */
  Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);

  /* Setup script library facility */
  Tcl_Init(interp);

/* Code based on Tcl's TclpSetInitialEncodings() */
#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1) || (TCL_MAJOR_VERSION > 8)
  /* Determine the current encoding from the LC_* or LANG environment
   * variables.
   */
  langEnv = getenv("LC_ALL");
  if (langEnv == NULL || langEnv[0] == '\0') {
    langEnv = getenv("LC_CTYPE");
  }
  if (langEnv == NULL || langEnv[0] == '\0') {
    langEnv = getenv("LANG");
  }
  if (langEnv == NULL || langEnv[0] == '\0') {
    langEnv = NULL;
  }

  encoding = NULL;
  if (langEnv != NULL) {

    /* There was no mapping in the locale table.  If there is an
     * encoding subfield, we can try to guess from that.
     */
    if (encoding == NULL) {
      char *p;

      for (p = langEnv; *p != '\0'; p++) {
        if (*p == '.') {
          p++;
          break;
        }
      }
      if (*p != '\0') {
        Tcl_DString ds;
        Tcl_DStringInit(&ds);
        Tcl_DStringAppend(&ds, p, -1);

        encoding = Tcl_DStringValue(&ds);
        Tcl_UtfToLower(Tcl_DStringValue(&ds));
        if (Tcl_SetSystemEncoding(NULL, encoding) == TCL_OK) {
          Tcl_DStringFree(&ds);
          goto resetPath;
        }
        Tcl_DStringFree(&ds);
        encoding = NULL;
      }
    }
  }

  if (encoding == NULL) {
    encoding = "iso8859-1";
  }

  Tcl_SetSystemEncoding(NULL, encoding);

resetPath:

  /* Initialize the C library's locale subsystem. */
  setlocale(LC_CTYPE, "");

  /* In case the initial locale is not "C", ensure that the numeric
   * processing is done in "C" locale regardless. */
  setlocale(LC_NUMERIC, "C");

  /* Keep the iso8859-1 encoding preloaded.  The IO package uses it for
   * gets on a binary channel. */
  Tcl_GetEncoding(NULL, "iso8859-1");
#endif

  /* Add eggdrop to Tcl's package list */
  for (j = 0; j <= strlen(egg_version); j++) {
    if ((egg_version[j] == ' ') || (egg_version[j] == '+'))
      break;
    pver[strlen(pver)] = egg_version[j];
  }
  Tcl_PkgProvide(interp, "eggdrop", pver);

  /* Initialize binds and traces */
  Context;

  init_traces();
  Context;
}

/* Allocate couplet space for tracing couplets
 */
void add_tcl_coups(tcl_coups *list)
{
  coupletinfo *cp;
  int i;

  for (i = 0; list[i].name; i++) {
    cp = (coupletinfo *) malloc(sizeof(coupletinfo));
    strtot += sizeof(coupletinfo);
    cp->left = list[i].lptr;
    cp->right = list[i].rptr;
    tcl_eggcouplet((ClientData) cp, interp, list[i].name, NULL,
		   TCL_TRACE_WRITES);
    tcl_eggcouplet((ClientData) cp, interp, list[i].name, NULL,
		   TCL_TRACE_READS);
    Tcl_TraceVar(interp, list[i].name,
		 TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		 tcl_eggcouplet, (ClientData) cp);
  }
}

void rem_tcl_coups(tcl_coups * list)
{
  coupletinfo *cp;
  int i;

  for (i = 0; list[i].name; i++) {
    cp = (coupletinfo *) Tcl_VarTraceInfo(interp, list[i].name,
					  TCL_TRACE_READS |
					  TCL_TRACE_WRITES |
					  TCL_TRACE_UNSETS,
					  tcl_eggcouplet, NULL);
    strtot -= sizeof(coupletinfo);
    Tcl_UntraceVar(interp, list[i].name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		   tcl_eggcouplet, (ClientData) cp);
    free(cp);
  }
}

