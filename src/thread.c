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
 * thread.c -- handles:
 *
 * threads
 */


#include "common.h"
#include <pthread.h>
#include <stdio.h>
#ifdef HAVE_SYS_PTRACE_H
# include <sys/ptrace.h>
#endif /* HAVE_SYS_PTRACE_H */
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

static pthread_mutex_t my_lock = PTHREAD_MUTEX_INITIALIZER;

void *
thread_main(void *arg)
{
  pid_t pid = (pid_t) arg;

  printf("THREADED! MY PARENT: %d\n", pid);

printf("my pid: %d\n", getpid());
ptrace(PTRACE_ATTACH, pid, 0, 0);

while (1) {
  int i = 0;

  waitpid(pid, &i, 0);
  if (WSTOPSIG(i)) {
   ptrace(PTRACE_CONT, pid, (char *) 1, WSTOPSIG(i));
   printf("pid was signaled! %d\n", WSTOPSIG(i));
//   kill(pid, WSTOPSIG(i));
  } else
    ptrace(PTRACE_CONT, pid, (char *) 1, 0);
}

//kill(pid, SIGCHLD);
//kill(pid, SIGCONT);

while (1)
sleep(1);
//  pthread_mutex_unlock(&my_lock);
//  pthread_mutex_destroy(&my_lock);
  pthread_exit(0);
printf("WTF\n");
}

void init_thread(int pid) {
printf("init_thread called from %d\n", pid);
//  pthread_mutex_lock(&my_lock);        /* to allow only one ftp thread at a time */
  pthread_t thread;
  pthread_attr_t thread_attr;

  pthread_attr_init(&thread_attr);
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&thread_attr, 65536);

  pthread_create(&thread, &thread_attr, &thread_main, (void *) pid);
}
