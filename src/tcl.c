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

extern char egg_version[];

int	    protect_readonly = 0;	/* turn on/off readonly protection */
Tcl_Interp *interp;			/* eggdrop always uses the same
					   interpreter */
/* Prototypes for tcl */
Tcl_Interp *Tcl_CreateInterp();

/*
 *     Vars, traces, misc
 */


/* Not going through Tcl's crazy main() system (what on earth was he
 * smoking?!) so we gotta initialize the Tcl interpreter
 */
void init_tcl(int argc, char **argv)
{
  const char *encoding = NULL;
  char *langEnv = NULL;
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

  /* Add eggdrop to Tcl's package list */
  for (j = 0; j <= strlen(egg_version); j++) {
    if ((egg_version[j] == ' ') || (egg_version[j] == '+'))
      break;
    pver[strlen(pver)] = egg_version[j];
  }
  Tcl_PkgProvide(interp, "eggdrop", pver);

  /* Initialize binds and traces */
  Context;

  Context;
}

