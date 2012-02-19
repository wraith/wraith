#ifndef _LIBTCL_H
#define _LIBTCL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#ifdef USE_SCRIPT_TCL

#include ".defs/libtcl_pre.h"

#include <tcl.h>

#include ".defs/libtcl_post.h"


#define STDVAR (ClientData cd, Tcl_Interp *interp, int argc, const char *argv[])

extern Tcl_Interp *global_interp;
bd::String tcl_eval(const bd::String&);
#endif

int load_libtcl();
int unload_libtcl();


#endif /* !_LIBTCL_H */
