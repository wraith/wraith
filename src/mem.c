
/* 
 * mem.c -- handles:
 *   memory allocation and deallocation
 *   keeping track of what memory is being used by whom
 * 
 * dprintf'ized, 15nov1995
 * 
 * $Id: mem.c,v 1.12 2000/01/08 21:23:14 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#define LOG_MISC 32
#define MEMTBLSIZE 25000	/* yikes! */

#define MEMPAD_BYTES 10
#define MEMPAD_CHAR '£'
#define STR(x) x
#if HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef int (*Function) ();

#include "hook.h"

void fatal(char *, int);
char *degarble(int, char *);
extern char base64[];

#ifdef DEBUG_MEM
unsigned long memused = 0;
static int lastused = 0;

struct {
  void *ptr;
  int size;
  short line;
  char file[20];
} memtbl[MEMTBLSIZE];

#endif

#ifdef HAVE_DPRINTF
#define dprintf dprintf_eggdrop
#endif

#define DP_HELP         0x7FF4
#define LCAT_ERROR		"error"

/* prototypes */
#if !defined(HAVE_PRE7_5_TCL) && defined(__STDC__)
void dprintf(int arg1, ...);
void log(char *arg1, ...);
#else
void dprintf();
void log();
#endif

int expected_memory();
int expmem_main();
int expmem_chanprog();
int expmem_channels();
int expmem_misc();
int expmem_fileq();
int expmem_users();
int expmem_dccutil();
int expmem_botnet();
int expmem_tcl();
int expmem_tclhash();
int expmem_net();
int expmem_tcldcc();
int expmem_log();
void tell_netdebug();
void do_module_report(int, int, char *);

/* initialize the memory structure */
void init_mem()
{
#ifdef DEBUG_MEM
  int i;

  for (i = 0; i < MEMTBLSIZE; i++)
    memtbl[i].ptr = NULL;
#endif
}

/* tell someone the gory memory details */
void tell_mem_status(char *nick)
{
#ifdef DEBUG_MEM
  float per;

  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(DP_HELP, "NOTICE %s :Memory table usage: %d/%d (%.1f%% full)\n", nick, lastused, MEMTBLSIZE, per);
#endif
  dprintf(DP_HELP, STR("NOTICE %s :Think I'm using about %dk.\n"), nick, (int) (expected_memory() / 1024));
}

void tell_mem_status_dcc(int idx)
{
#ifdef DEBUG_MEM
  int exp;
  float per;

  exp = expected_memory();	/* in main.c ? */
  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(idx, "Memory table: %d/%d (%.1f%% full)\n", lastused, MEMTBLSIZE, per);
  per = ((exp * 1.0) / (memused * 1.0)) * 100.0;
  if (per != 100.0)
    dprintf(idx, "Memory fault: only accounting for %d/%ld (%.1f%%), %ld bytes unaccounted\n", exp, memused, per, memused - exp);
  dprintf(idx, "Memory table itself occupies an additional %dk static\n", (int) (sizeof(memtbl) / 1024));
#endif
}

void debug_mem_to_dcc(int idx)
{
#ifdef DEBUG_MEM
#define MAX_MEM 12
  unsigned long exp[MAX_MEM],
    use[MAX_MEM],
    l;
  int i,
    j;
  char fn[20],
    sofar[81];
  char *p;

  exp[0] = expmem_main();
  exp[1] = expmem_chanprog();
  exp[2] = expmem_misc();
  exp[3] = expmem_users();
  exp[4] = expmem_net();
  exp[5] = expmem_dccutil();
  exp[6] = expmem_botnet();
  exp[7] = expmem_tcl();
  exp[8] = expmem_tclhash();
  exp[9] = expmem_channels();
  exp[10] = expmem_tcldcc();
  exp[11] = expmem_log();
  for (i = 0; i < MAX_MEM; i++)
    use[i] = 0;
  for (i = 0; i < lastused; i++) {
    strcpy(fn, memtbl[i].file);
    p = strchr(fn, ':');
    if (p)
      *p = 0;
    l = memtbl[i].size;
    if (!strcasecmp(fn, "main.c") || !strcasecmp(fn, "xmain.c"))
      use[0] += l;
    else if (!strcasecmp(fn, "chanprog.c") || !strcasecmp(fn, "xchanprog.c"))
      use[1] += l;
    else if (!strcasecmp(fn, "xmisc.c") || !strcasecmp(fn, "misc.c"))
      use[2] += l;
    else if (!strcasecmp(fn, "xuserrec.c") || !strcasecmp(fn, "userrec.c"))
      use[3] += l;
    else if (!strcasecmp(fn, "net.c") || !strcasecmp(fn, "xnet.c"))
      use[4] += l;
    else if (!strcasecmp(fn, "dccutil.c") || !strcasecmp(fn, "xdccutil.c"))
      use[5] += l;
    else if (!strcasecmp(fn, "xbotnet.c") || !strcasecmp(fn, "botnet.c"))
      use[6] += l;
    else if (!strcasecmp(fn, "tcl.c") || !strcasecmp(fn, "xtcl.c"))
      use[7] += l;
    else if (!strcasecmp(fn, "xtclhash.c") || !strcasecmp(fn, "tclhash.c"))
      use[8] += l;
    else if (!strcasecmp(fn, "xchannels.c") || !strcasecmp(fn, "channels.c"))
      use[9] += l;
    else if (!strcasecmp(fn, "xtcldcc.c") || !strcasecmp(fn, "tcldcc.c"))
      use[10] += l;
    else if (!strcasecmp(fn, "xlog.c") || !strcasecmp(fn, "log.c"))
      use[11] += l;
    else {
      dprintf(idx, "Not logging file %s!\n", fn);
    }
    if (p)
      *p = ':';
  }
  for (i = 0; i < MAX_MEM; i++) {
    switch (i) {
    case 0:
      strcpy(fn, "main.c");
      break;
    case 1:
      strcpy(fn, "chanprog.c");
      break;
    case 2:
      strcpy(fn, "misc.c");
      break;
    case 3:
      strcpy(fn, "userrec.c");
      break;
    case 4:
      strcpy(fn, "net.c");
      break;
    case 5:
      strcpy(fn, "dccutil.c");
      break;
    case 6:
      strcpy(fn, "botnet.c");
      break;
    case 7:
      strcpy(fn, "tcl.c");
      break;
    case 8:
      strcpy(fn, "tclhash.c");
      break;
    case 9:
      strcpy(fn, "channels.c");
      break;
    case 10:
      strcpy(fn, "tcldcc.c");
      break;
    case 11:
      strcpy(fn, "log.c");
      break;
    }
    if (use[i] == exp[i]) {
      dprintf(idx, "File '%-10s' accounted for %lu/%lu (ok)\n", fn, exp[i], use[i]);
    } else {
      dprintf(idx, "File '%-10s' accounted for %lu/%lu (difference %li) (debug follows:)\n", fn, exp[i], use[i], use[i] - exp[i]);
      strcpy(sofar, "   ");
      for (j = 0; j < lastused; j++) {
	if ((p = strchr(memtbl[j].file, ':')))
	  *p = 0;
	if (!strcasecmp(memtbl[j].file, fn) || !strcasecmp(memtbl[j].file + 1, fn)) {
	  if (p)
	    sprintf(&sofar[strlen(sofar)], "%-10s/%-4d:(%04d) ", p + 1, memtbl[j].line, memtbl[j].size);
	  else
	    sprintf(&sofar[strlen(sofar)], "%-4d:(%04d) ", memtbl[j].line, memtbl[j].size);

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
  dprintf(idx, "--- End of debug memory list.\n");
#else
  dprintf(idx, STR("Compiled without extensive memory debugging (sorry).\n"));
#endif
  tell_netdebug(idx);
}

#ifdef DEBUG_MEM
void *n_malloc(int size, char *file, int line)
#else
void *n_malloc(int size)
#endif
{
  void *x;

#ifdef DEBUG_MEM
  int i = 0;

  x = (void *) malloc(size + MEMPAD_BYTES * 2);
#else
  x = (void *) malloc(size + 4);
#endif

  if (x == NULL) {
#ifdef DEBUG_MEM
    log(LCAT_ERROR, STR("*** FAILED MALLOC (%d): %s (%s/%d)"), size, strerror(errno), file, line);
#else
    log(LCAT_ERROR, STR("*** FAILED MALLOC (%d): %s"), size, strerror(errno));
#endif
    fatal(STR("Memory allocation failed"), 0);
  }
#ifdef DEBUG_MEM
  if (lastused == MEMTBLSIZE) {
    log(LCAT_ERROR, "*** MEMORY TABLE FULL: %s (%d)", file, line);
    return x;
  }
  i = lastused;
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  strncpy(memtbl[i].file, file, sizeof(memtbl[i].file));
  memtbl[i].file[sizeof(memtbl[i].file)-1]=0;
#ifdef MEM_LOG
  printf("%s/%u (+%u = %ul)\n", file, line, size, memused + size);
#endif
  memused += size;
  lastused++;

  memset(x, MEMPAD_CHAR, MEMPAD_BYTES);
  x += size + MEMPAD_BYTES;
  memset(x, MEMPAD_CHAR, MEMPAD_BYTES);
  x -= size;
#else
  * (int *) x = size;
  x += 4;
#endif
  return x;
}

#ifdef DEBUG_MEM
void *n_realloc(void *ptr, int size, char *file, int line)
#else
void *n_realloc(void *ptr, int size)
#endif
{
  void *x;
  int i = 0;

#ifdef DEBUG_MEM
  int n = 0;
#endif
  /* ptr == NULL is valid. Avoiding duplicate code further down */
  if (!ptr)
#ifdef DEBUG_MEM
    return n_malloc(size, file, line);
#else
    return n_malloc(size);
#endif

#ifdef DEBUG_MEM
  ptr -= MEMPAD_BYTES;

  for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
  if (i == lastused) {
    log(LCAT_ERROR, "*** ATTEMPTING TO REALLOC NON-MALLOC'D PTR: %s (%d)", file, line);
    return NULL;
  }

  /* verify that the front padding is ok */
  x = ptr;
  for (n = 0; n < MEMPAD_BYTES; n++) {
    if (*(char *) x++ != MEMPAD_CHAR) {
      log(LCAT_ERROR, "*** BUFFER OVERRUN (before): %s (%d), alloced: %s (%d)", file, line, memtbl[i].file, memtbl[i].line);
      n = MEMPAD_BYTES;
    }
  }

  /* and then the end padding */
  x = ptr + memtbl[i].size + MEMPAD_BYTES;
  for (n = 0; n < MEMPAD_BYTES; n++) {
    if (*(char *) x++ != MEMPAD_CHAR) {
      log(LCAT_ERROR, "*** BUFFER OVERRUN (after): %s (%d), alloced: %s (%d)", file, line, memtbl[i].file, memtbl[i].line);
      n = MEMPAD_BYTES;
    }
  }

  x = (void *) realloc(ptr, size + MEMPAD_BYTES * 2);

  if (x == NULL) {
    i = i;
    log(LCAT_ERROR, "*** FAILED REALLOC %s (%d)", file, line);
    return NULL;
  }

  memused -= memtbl[i].size;
#ifdef MEM_LOG
  if (memtbl[i].size != size)
    printf("%s/%u (-%u = %ul, +%u = %ul)\n", file, line, memtbl[i].size, memused, size, memused + size);
#endif
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  strncpy(memtbl[i].file, file, sizeof(memtbl[i].file));
  memtbl[i].file[sizeof(memtbl[i].file)-1]=0;
  memused += size;

  x += size + MEMPAD_BYTES;
  memset(x, MEMPAD_CHAR, MEMPAD_BYTES);
  x -= size;
#else
  ptr -= 4;
  x = (void *) realloc(ptr, size+4);
  if (x == NULL) {
    i = i;
#ifdef DEBUG_MEM
    log(LCAT_ERROR, "*** FAILED REALLOC %s (%d)", file, line);
#else
    log(LCAT_ERROR, "*** FAILED REALLOC");
#endif
    return NULL;
  }
  * (int *) x = size;
  x += 4;
#endif

  return x;
}

#ifdef DEBUG_MEM
void n_free(void *ptr, char *file, int line)
#else
void n_free(void *ptr)
#endif
{
  int i = 0;

#ifdef DEBUG_MEM
  int n = 0;
  void *x;
#endif

  if (ptr == NULL) {
#ifdef DEBUG_MEM
    log(LCAT_ERROR, STR("*** ATTEMPTING TO FREE NULL PTR: %s (%d)"), file, line);
#else
    log(LCAT_ERROR, STR("*** ATTEMPTING TO FREE NULL PTR"));
#endif
    i = i;
    return;
  }
#ifdef DEBUG_MEM
  /* give tcl builtins an escape mechanism */
  if (line) {
    ptr -= MEMPAD_BYTES;

    for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
    if (i == lastused) {
      log(LCAT_ERROR, "*** ATTEMPTING TO FREE NON-MALLOC'D PTR: %s (%d)", file, line);
      return;
    }

    /* verify that the front padding is ok */
    x = ptr;
    for (n = 0; n < MEMPAD_BYTES; n++) {
      if (*(char *) x++ != MEMPAD_CHAR) {
	log(LCAT_ERROR, "*** BUFFER OVERRUN (before): %s (%d), alloced: %s (%d)", file, line, memtbl[i].file, memtbl[i].line);
	n = MEMPAD_BYTES;
      }
    }

    /* and then the end padding */
    x = ptr + memtbl[i].size + MEMPAD_BYTES;
    for (n = 0; n < MEMPAD_BYTES; n++) {
      if (*(char *) x++ != MEMPAD_CHAR) {
	log(LCAT_ERROR, "*** BUFFER OVERRUN (after): %s (%d), alloced: %s (%d)", file, line, memtbl[i].file, memtbl[i].line);
	n = MEMPAD_BYTES;
      }
    }

    memused -= memtbl[i].size;
#ifdef MEM_LOG
    printf("%s/%u (-%u = %ul)\n", file, line, memtbl[i].size, memused);
#endif
    lastused--;
    memtbl[i].ptr = memtbl[lastused].ptr;
    memtbl[i].size = memtbl[lastused].size;
    memtbl[i].line = memtbl[lastused].line;
    strcpy(memtbl[i].file, memtbl[lastused].file);
  }
#else
  {
    int i;
    char * x;
    x = ptr;
    ptr -= 4;
    for (i=0;i < * (int *) ptr;i++) 
      *x++ = base64[random() % 64];
  }
#endif
  free(ptr);
}








