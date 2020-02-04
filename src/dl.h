#ifndef _DL_H
#define _DL_H

#include "common.h"
#include <dlfcn.h>

#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>

extern const char *dlsym_error;

#define DLSYM(_handle, x) \
  dlerror(); \
  x##_t x; \
  *(void **) (&x) = dlsym(_handle, #x); \
  dlsym_error = dlerror(); \
  if (dlsym_error) { \
    sdprintf("%s", dlsym_error); \
    return(1); \
  }

#define DLSYM_GLOBAL_FWDCOMPAT(_handle, x) do { \
  dlerror(); \
  if ((dl_symbol_table[#x] = (FunctionPtr) ((x##_t) dlsym(_handle, #x))) == \
    NULL && \
    dlerror() && \
    (dl_symbol_table[#x] = \
      (FunctionPtr) ((x##_t) dlsym(NULL, "_" #x))) == NULL) { \
    dlsym_error = dlerror(); \
    if (dlsym_error) { \
      fprintf(stderr, "%s", dlsym_error); \
      return(1); \
    } \
  } else { \
    my_symbols << #x; \
  } \
} while (0)

#define DLSYM_GLOBAL(_handle, x) do { \
  dlerror(); \
  dl_symbol_table[#x] = (FunctionPtr) ((x##_t) dlsym(_handle, #x)); \
  dlsym_error = dlerror(); \
  if (dlsym_error) { \
    fprintf(stderr, "%s", dlsym_error); \
    return(1); \
  } \
  my_symbols << #x; \
} while (0)

#define DLSYM_GLOBAL_SIMPLE(_handle, x) ( \
  dl_symbol_table[#x] = (FunctionPtr) ((x##_t) dlsym(_handle, #x)), \
  dl_symbol_table[#x] \
)

#define DLSYM_VAR(x) ((x##_t)dl_symbol_table[#x])

extern bd::HashTable<bd::String, FunctionPtr> dl_symbol_table;

#ifdef GENERATE_DEFS
#undef DLSYM_GLOBAL
#undef DLSYM_GLOBAL_FWDCOMPAT
#endif

#endif /* !_DL_H_ */
