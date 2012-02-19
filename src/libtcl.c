/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2010 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * tcl.c -- handles:
 *   libtcl handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libtcl.h"
#include ".defs/libtcl_defs.c"

#ifdef USE_SCRIPT_TCL
Tcl_Interp *global_interp = NULL;
#endif

void *libtcl_handle = NULL;
static bd::Array<bd::String> my_symbols;

void initialize_binds_tcl();

static int load_symbols(void *handle) {
#ifdef USE_SCRIPT_TCL
  const char *dlsym_error = NULL;

  DLSYM_GLOBAL(handle, Tcl_Eval);
  DLSYM_GLOBAL(handle, Tcl_GetStringResult);
  DLSYM_GLOBAL(handle, Tcl_DeleteInterp);
  DLSYM_GLOBAL(handle, Tcl_CreateCommand);
  DLSYM_GLOBAL(handle, Tcl_CreateInterp);
  DLSYM_GLOBAL(handle, Tcl_FindExecutable);
  DLSYM_GLOBAL(handle, Tcl_Init);
#endif

  return 0;
}

int load_libtcl() {
#ifndef USE_SCRIPT_TCL
  sdprintf("Not compiled with TCL support");
  return 1;
#else
  if (global_interp) {
    return 0;
  }
#endif

  bd::Array<bd::String> libs_list(bd::String("libtcl.so libtcl83.so libtcl8.3.so libtcl84.so libtcl8.4.so libtcl85.so libtcl8.5.so").split(' '));

  for (size_t i = 0; i < libs_list.length(); ++i) {
    dlerror(); // Clear Errors
    libtcl_handle = dlopen(bd::String(libs_list[i]).c_str(), RTLD_LAZY);
    if (libtcl_handle) break;
  }
  if (!libtcl_handle) {
    sdprintf("Unable to find libtcl");
    return 1;
  }

  load_symbols(libtcl_handle);

#ifdef USE_SCRIPT_TCL
  // create interp
  global_interp = Tcl_CreateInterp();
  Tcl_FindExecutable(binname);

  if (Tcl_Init(global_interp) != TCL_OK) {
    sdprintf("Tcl_Init error: %s", Tcl_GetStringResult(global_interp));
    return 1;
  }

  initialize_binds_tcl();
#endif
  return 0;
}

#ifdef USE_SCRIPT_TCL

#include "chanprog.h"
static int cmd_privmsg STDVAR {
  bd::String str = argv[2];
  for (int i = 3; i < argc; ++i)
    str += " " + bd::String(argv[i]);
  privmsg(argv[1], str, DP_SERVER);

  return TCL_OK;
}

void initialize_binds_tcl() {
  Tcl_CreateCommand(global_interp, "privmsg", (Tcl_CmdProc*) cmd_privmsg, NULL, NULL);
}

#endif

int unload_libtcl() {
  if (libtcl_handle) {
#ifdef USE_SCRIPT_TCL
    if (global_interp) {
      Tcl_DeleteInterp(global_interp);
      global_interp = NULL;
    }
#endif

    // Cleanup symbol table
    for (size_t i = 0; i < my_symbols.length(); ++i) {
      dl_symbol_table.remove(my_symbols[i]);
      static_cast<bd::String>(my_symbols[i]).clear();
    }
    my_symbols.clear();

    dlclose(libtcl_handle);
    libtcl_handle = NULL;
    return 0;
  }
  return 1;
}

#ifdef USE_SCRIPT_TCL
bd::String tcl_eval(const bd::String& str) {
  load_libtcl();
  if (!global_interp) return bd::String();
  if (Tcl_Eval(global_interp, str.c_str()) == TCL_OK) {
    return Tcl_GetStringResult(global_interp);
  } else
    return tcl_eval("set errorInfo");
  return bd::String();
}
#endif
