/*
 * mem.c -- handles:
 *   memory allocation and deallocation
 *   keeping track of what memory is being used by whom
 *
 */

#define MEMTBLSIZE 25000	/* yikes! */
#define COMPILING_MEM

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mod/modvals.h"

#define STR(x) x


extern module_entry	*module_list;

#ifdef DEBUG_MEM
unsigned long	memused = 0;
static int	lastused = 0;

struct {
  void	*ptr;
  int	 size;
  short	 line;
  char	 file[20];
} memtbl[MEMTBLSIZE];
#endif

/* Prototypes */
int expected_memory();
int expmem_chanprog();
int expmem_misc();
int expmem_fileq();
int expmem_users();
int expmem_dccutil();
int expmem_botnet();
int expmem_tcl();
int expmem_tclhash();
int expmem_tclmisc();
int expmem_net();
int expmem_modules();
int expmem_config();
int expmem_auth();
int expmem_tcldcc();
int expmem_dns();


/* Initialize the memory structure
 */
void init_mem()
{
#ifdef DEBUG_MEM
  int i;

  for (i = 0; i < MEMTBLSIZE; i++)
    memtbl[i].ptr = NULL;
#endif
}

/* Tell someone the gory memory details
 */
void tell_mem_status(char *nick)
{
#ifdef DEBUG_MEM
  float per;

  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(DP_HELP, "NOTICE %s :Memory table usage: %d/%d (%.1f%% full)\n",
	  nick, lastused, MEMTBLSIZE, per);
#endif
  dprintf(DP_HELP, "NOTICE %s :Think I'm using about %dk.\n", nick,
	  (int) (expected_memory() / 1024));
}

void tell_mem_status_dcc(int idx)
{
#ifdef DEBUG_MEM
  int exp;
  float per;

  exp = expected_memory();	/* in main.c ? */
  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(idx, STR("Memory table: %d/%d (%.1f%% full)\n"), lastused, MEMTBLSIZE,
	  per);
  per = ((exp * 1.0) / (memused * 1.0)) * 100.0;
  if (per != 100.0)
    dprintf(idx, STR("Memory fault: only accounting for %d/%ld (%.1f%%)\n"),
	    exp, memused, per);
  dprintf(idx, STR("Memory table itself occupies an additional %dk static\n"),
	  (int) (sizeof(memtbl) / 1024));
#endif
}

void debug_mem_to_dcc(int idx)
{
#ifdef DEBUG_MEM
#define MAX_MEM 14
  unsigned long exp[MAX_MEM], use[MAX_MEM], l;
  int i, j;
  char fn[20], sofar[81];
  module_entry *me;
  char *p;

  exp[0] = expmem_auth();
  exp[1] = expmem_chanprog();
  exp[2] = expmem_misc();
  exp[3] = expmem_users();
  exp[4] = expmem_net();
  exp[5] = expmem_dccutil();
  exp[6] = expmem_botnet();
  exp[7] = expmem_tcl();
  exp[8] = expmem_tclhash();
  exp[9] = expmem_tclmisc();
  exp[10] = expmem_modules(1);
  exp[11] = expmem_tcldcc();
  exp[12] = expmem_dns();
  exp[13] = expmem_config();
  for (me = module_list; me; me = me->next)
    me->mem_work = 0;
  for (i = 0; i < MAX_MEM; i++)
    use[i] = 0;
  for (i = 0; i < lastused; i++) {
    strcpy(fn, memtbl[i].file);
    p = strchr(fn, ':');
    if (p)
      *p = 0;
/*  this might be used to eliminate the pesky 'x'
    p = strchr(fn, 'x');
    if (p)
      *p = 0;
*/
    l = memtbl[i].size;
    if (!strcmp(fn, "xauth.c"))
      use[0] += l;
    else if (!strcmp(fn, "xchanprog.c"))
      use[1] += l;
    else if (!strcmp(fn, "xmisc.c"))
      use[2] += l;
    else if (!strcmp(fn, "xuserrec.c"))
      use[3] += l;
    else if (!strcmp(fn, "xnet.c"))
      use[4] += l;
    else if (!strcmp(fn, "xdccutil.c"))
      use[5] += l;
    else if (!strcmp(fn, "xbotnet.c"))
      use[6] += l;
    else if (!strcmp(fn, "xtcl.c"))
      use[7] += l;
    else if (!strcmp(fn, "xtclhash.c"))
      use[8] += l;
    else if (!strcmp(fn, "xtclmisc.c"))
      use[9] += l;
    else if (!strcmp(fn, "xmodules.c"))
      use[10] += l;
    else if (!strcmp(fn, "xtcldcc.c"))
      use[11] += l;
    else if (!strcmp(fn, "xdns.c"))
      use[12] += l;
    else if (!strcmp(fn, "xconfig.c"))
      use[13] += l;
    else if (p) {
      for (me = module_list; me; me = me->next)
	if (!strcmp(fn, me->name))
	  me->mem_work += l;
    } else
      dprintf(idx, STR("Not logging file %s!\n"), fn);
  }
  for (i = 0; i < MAX_MEM; i++) {
    switch (i) {
    case 0:
      strcpy(fn, "xauth.c");
      break;
    case 1:
      strcpy(fn, "xchanprog.c");
      break;
    case 2:
      strcpy(fn, "xmisc.c");
      break;
    case 3:
      strcpy(fn, "xuserrec.c");
      break;
    case 4:
      strcpy(fn, "xnet.c");
      break;
    case 5:
      strcpy(fn, "xdccutil.c");
      break;
    case 6:
      strcpy(fn, "xbotnet.c");
      break;
    case 7:
      strcpy(fn, "xtcl.c");
      break;
    case 8:
      strcpy(fn, "xtclhash.c");
      break;
    case 9:
      strcpy(fn, "xtclmisc.c");
      break;
    case 10:
      strcpy(fn, "xmodules.c");
      break;
    case 11:
      strcpy(fn, "xtcldcc.c");
      break;
    case 12:
      strcpy(fn, "xdns.c");
      break;
    case 13:
      strcpy(fn, "xconfig.c");
      break;
    }
    if (use[i] == exp[i]) {
      dprintf(idx, STR("File '%-10s' accounted for %lu/%lu (ok)\n"), fn, exp[i],
	      use[i]);
    } else {
      dprintf(idx, STR("File '%-10s' accounted for %lu/%lu (debug follows:)\n"),
	      fn, exp[i], use[i]);
      strcpy(sofar, "   ");
      for (j = 0; j < lastused; j++) {
	if ((p = strchr(memtbl[j].file, ':')))
	  *p = 0;
	if (!egg_strcasecmp(memtbl[j].file, fn)) {
	  if (p)
	    sprintf(&sofar[strlen(sofar)], "%-10s/%-4d:(%04d) ",
		    p + 1, memtbl[j].line, memtbl[j].size);
	  else
	    sprintf(&sofar[strlen(sofar)], "%-4d:(%04d) ",
		    memtbl[j].line, memtbl[j].size);

	  if (strlen(sofar) > 60) {
	    sofar[strlen(sofar) - 1] = 0;
	    dprintf(idx, "%s\n", sofar);
	    strcpy(sofar, "   ");
	  }
	}
	if (p)
	  *p = ':';
      }
      if (sofar[0]) {
	sofar[strlen(sofar) - 1] = 0;
	dprintf(idx, "%s\n", sofar);
      }
    }
  }
  for (me = module_list; me; me = me->next) {
    Function *f = me->funcs;
    int expt = 0;

    if ((f != NULL) && (f[MODCALL_EXPMEM] != NULL))
      expt = f[MODCALL_EXPMEM] ();
    if (me->mem_work == expt) {
      dprintf(idx, STR("Module '%-10s' accounted for %lu/%lu (ok)\n"), me->name,
	      expt, me->mem_work);
    } else {
      dprintf(idx, STR("Module '%-10s' accounted for %lu/%lu (debug follows:)\n"),
	      me->name, expt, me->mem_work);
      strcpy(sofar, "   ");
      for (j = 0; j < lastused; j++) {
	strcpy(fn, memtbl[j].file);
	if ((p = strchr(fn, ':')) != NULL) {
	  *p = 0;
	  if (!egg_strcasecmp(fn, me->name)) {
	    sprintf(&sofar[strlen(sofar)], "%-10s/%-4d:(%04X) ", p + 1,
		    memtbl[j].line, memtbl[j].size);
	    if (strlen(sofar) > 60) {
	      sofar[strlen(sofar) - 1] = 0;
	      dprintf(idx, "%s\n", sofar);
	      strcpy(sofar, "   ");
	    }
	    *p = ':';
	  }
	}
      }
      if (sofar[0]) {
	sofar[strlen(sofar) - 1] = 0;
	dprintf(idx, "%s\n", sofar);
      }
    }
  }
  dprintf(idx, STR("--- End of debug memory list.\n"));
#else
  dprintf(idx, STR("Compiled without extensive memory debugging (sorry).\n"));
#endif
  tell_netdebug(idx);
}

void *my_malloc(int size)
{
  void *x;

  x = (void *) malloc(size);
  if (x == NULL) 
    fatal("Memory allocation failed", 0);
  return x;
}

void *n_malloc(int size, const char *file, int line)
{
  void	*x;
#ifdef DEBUG_MEM
  int	 i = 0;
  char	*p;
#endif

  x = (void *) malloc(size);
  if (x == NULL) {
    putlog(LOG_MISC, "*", "*** FAILED MALLOC %s (%d) (%d): %s", file, line,
	   size, strerror(errno));
    fatal("Memory allocation failed", 0);
  }
#ifdef DEBUG_MEM
  if (lastused == MEMTBLSIZE) {
    putlog(LOG_MISC, "*", "*** MEMORY TABLE FULL: %s (%d)", file, line);
    fatal("Memory table full", 0);
  }
  i = lastused;
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  p = strrchr(file, '/');
  strncpy(memtbl[i].file, p ? p + 1 : file, 19);
  memtbl[i].file[19] = 0;
  memused += size;
  lastused++;
#endif
  return x;
}

void *my_realloc(void *ptr, int size)
{
  void *x;

  if (!ptr)
    return my_malloc(size);

  x = (void *) realloc(ptr, size);
  if (x == NULL && size > 0)
    return NULL;
  return x;
}

void *n_realloc(void *ptr, int size, const char *file, int line)
{
  void *x;
  int i = 0;
#ifdef DEBUG_MEM
  char *p;
#endif

  /* ptr == NULL is valid. Avoiding duplicate code further down */
  if (!ptr)
    return n_malloc(size, file, line);

  x = (void *) realloc(ptr, size);
  if (x == NULL && size > 0) {
    i = i;
    putlog(LOG_MISC, "*", "*** FAILED REALLOC %s (%d)", file, line);
    return NULL;
  }
#ifdef DEBUG_MEM
  for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
  if (i == lastused) {
    putlog(LOG_MISC, "*", "*** ATTEMPTING TO REALLOC NON-MALLOC'D PTR: %s (%d)",
	   file, line);
    return NULL;
  }
  memused -= memtbl[i].size;
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  p = strrchr(file, '/');
  strncpy(memtbl[i].file, p ? p + 1 : file, 19);
  memtbl[i].file[19] = 0;
  memused += size;
#endif
  return x;
}

void my_free(void *ptr)
{
  if (ptr == NULL)
    return;
  free(ptr);
}

void n_free(void *ptr, const char *file, int line)
{
  int i = 0;

  if (ptr == NULL) {
    putlog(LOG_MISC, "*", "*** ATTEMPTING TO FREE NULL PTR: %s (%d)",
	   file, line);
    i = i;
    return;
  }
#ifdef DEBUG_MEM
  /* Give tcl builtins an escape mechanism */
  if (line) {
    for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
    if (i == lastused) {
      putlog(LOG_MISC, "*", "*** ATTEMPTING TO FREE NON-MALLOC'D PTR: %s (%d)",
	     file, line);
      return;
    }
    memused -= memtbl[i].size;
    lastused--;
    memtbl[i].ptr = memtbl[lastused].ptr;
    memtbl[i].size = memtbl[lastused].size;
    memtbl[i].line = memtbl[lastused].line;
    strcpy(memtbl[i].file, memtbl[lastused].file);
  }
#endif
  free(ptr);
}


#ifdef S_GARBLESTRINGS
#define GARBLE_BUFFERS 40
unsigned char *garble_buffer[GARBLE_BUFFERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int garble_ptr = (-1);

char *degarble(int len, char *g)
{
  int i;
  unsigned char x;

  garble_ptr++;
  if (garble_ptr == GARBLE_BUFFERS)
    garble_ptr = 0;
  if (garble_buffer[garble_ptr])
    free(garble_buffer[garble_ptr]);
  garble_buffer[garble_ptr] = malloc(len + 1);
  x = 0xFF;
  for (i = 0; i < len; i++) {
    garble_buffer[garble_ptr][i] = g[i] ^ x;
    x = garble_buffer[garble_ptr][i];
  }
  garble_buffer[garble_ptr][len] = 0;
  return (char *) garble_buffer[garble_ptr];
}
#endif /* S_GARBLESTRINGS */

