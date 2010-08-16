#ifndef _DL_H
#define _DL_H

#include "common.h"
#include <dlfcn.h>

#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>

#define DLSYM(_handle, x) \
  dlerror(); \
  x##_t x; \
  *(void **) (&x) = dlsym(_handle, #x); \
  dlsym_error = dlerror(); \
  if (dlsym_error) { \
    sdprintf("%s", dlsym_error); \
    return(1); \
  }

#define DLSYM_GLOBAL(_handle, x) do { \
  dlerror(); \
  dl_symbol_table[#x] = (FunctionPtr) ((x##_t) dlsym(_handle, #x)); \
  dlsym_error = dlerror(); \
  if (dlsym_error) { \
    sdprintf("%s", dlsym_error); \
    return(1); \
  } \
} while (0)

#define DLSYM_VAR(x) ((x##_t)dl_symbol_table[#x])

extern bd::HashTable<bd::String, FunctionPtr> dl_symbol_table;

#endif /* !_DL_H_ */
