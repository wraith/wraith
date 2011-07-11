#ifndef _LIBTCL_H
#define _LIBTCL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#ifdef HAVE_LIBTCL

#include ".defs/libtcl_pre.h"

#include <tcl.h>

#include ".defs/libtcl_post.h"

typedef int (*Tcl_Eval_t)(Tcl_Interp*, const char*);
typedef Tcl_Command (*Tcl_CreateCommand_t)(Tcl_Interp*, const char*, Tcl_CmdProc*, ClientData, Tcl_CmdDeleteProc*);
typedef const char* (*Tcl_GetStringResult_t)(Tcl_Interp*);
typedef void (*Tcl_DeleteInterp_t)(Tcl_Interp*);
typedef Tcl_Interp* (*Tcl_CreateInterp_t)(void);
typedef void (*Tcl_FindExecutable_t)(const char*);
typedef int (*Tcl_Init_t)(Tcl_Interp*);

#define STDVAR (ClientData cd, Tcl_Interp *interp, int argc, const char *argv[])

extern Tcl_Interp *global_interp;
bd::String tcl_eval(const bd::String&);
#endif

int load_libtcl();
int unload_libtcl();


#endif /* !_LIBTCL_H */
