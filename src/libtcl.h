#ifndef _LIBTCL_H
#define _LIBTCL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <tcl.h>

typedef int (*Tcl_Eval_t)(Tcl_Interp*, const char*);
typedef void (*Tcl_AppendResult_t)(Tcl_Interp*, ...);
typedef void (*Tcl_CreateCommand_t)(Tcl_Interp*, const char*, Tcl_CmdProc*, ClientData, Tcl_CmdDeleteProc*);
typedef const char* (*Tcl_GetStringResult_t)(Tcl_Interp*);
typedef int (*Tcl_DeleteInterp_t)(Tcl_Interp*);
typedef Tcl_Interp* (*Tcl_CreateInterp_t)();
typedef void (*Tcl_FindExecutable_t)(const char*);
typedef int (*Tcl_Init_t)(Tcl_Interp*);

#include ".defs/libtcl_defs.h"

#define STDVAR (ClientData cd, Tcl_Interp *interp, int argc, const char *argv[])

#define BADARGS(nl, nh, example) do {                               \
	if ((argc < (nl)) || (argc > (nh))) {                       \
		Tcl_AppendResult(interp, "wrong # args: should be \"", \
			argv[0], (example), "\"", NULL);            \
		return TCL_ERROR;                                   \
	}                                                           \
} while (0)

extern Tcl_Interp *global_interp;

int load_libtcl();
int unload_libtcl();
bd::String tcl_eval(const bd::String&);


#endif /* !_LIBTCL_H */
