#include "main.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "stat.h"
int
copyfile (char *oldpath, char *newpath)
{
  int fi, fo, x;
  char buf[512];
  struct stat st;
#ifndef CYGWIN_HACKS
  fi = open (oldpath, O_RDONLY, 0);
#else
  fi = open (oldpath, O_RDONLY | O_BINARY, 0);
#endif
  if (fi < 0)
    return 1;
  fstat (fi, &st);
  if (!(st.st_mode & S_IFREG))
    return 3;
  fo = creat (newpath, (int) (st.st_mode & 0600));
  if (fo < 0)
    {
      close (fi);
      return 2;
    }
  for (x = 1; x > 0;)
    {
      x = read (fi, buf, 512);
      if (x > 0)
	{
	  if (write (fo, buf, x) < x)
	    {
	      close (fo);
	      close (fi);
	      unlink (newpath);
	      return 4;
	    }
	}
    }
#ifdef HAVE_FSYNC
  fsync (fo);
#endif
  close (fo);
  close (fi);
  return 0;
}

int
movefile (char *oldpath, char *newpath)
{
  int ret;
#ifdef HAVE_RENAME
  if (!rename (oldpath, newpath))
    return 0;
#endif
  ret = copyfile (oldpath, newpath);
  if (!ret)
    unlink (oldpath);
  return ret;
}
