/*
 * misc.c -- handles:
 *   copyfile() movefile()
 *
 */

#include "common.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "stat.h"
#include "misc_file.h"
#include "main.h"

/* Copy a file from one place to another (possibly erasing old copy).
 *
 * returns:  0 if OK
 *	     1 if can't open original file
 *	     2 if can't open new file
 *	     3 if original file isn't normal
 *	     4 if ran out of disk space
 */
int copyfile(const char *oldpath, const char *newpath)
{
  int fi;

#ifndef CYGWIN_HACKS
  fi = open(oldpath, O_RDONLY, 0);
#else
  fi = open(oldpath, O_RDONLY | O_BINARY, 0);
#endif
  if (fi < 0)
    return 1;

  struct stat st;

  fstat(fi, &st);
  if (!(st.st_mode & S_IFREG))
    return 3;

  int fo;

  fo = creat(newpath, (int) (st.st_mode & 0600));
  if (fo < 0) {
    close(fi);
    return 2;
  }

  char buf[512] = "";

  for (int x = 1; x > 0;) {
    x = read(fi, buf, 512);
    if (x > 0) {
      if (write(fo, buf, x) < x) {	/* Couldn't write */
	close(fo);
	close(fi);
	unlink(newpath);
	return 4;
      }
    }
  }
#ifdef HAVE_FSYNC
  fsync(fo);
#endif /* HAVE_FSYNC */
  close(fo);
  close(fi);
  return 0;
}

int movefile(const char *oldpath, const char *newpath)
{
#ifdef HAVE_RENAME
  /* Try to use rename first */
  if (!rename(oldpath, newpath))
    return 0;
#endif /* HAVE_RENAME */

  /* If that fails, fall back to just copying and then
   * deleting the file.
   */
  int ret = copyfile(oldpath, newpath);

  if (!ret)
    unlink(oldpath);
  return ret;
}

int is_file(const char *s)
{
  struct stat ss;
  int i = stat(s, &ss);

  if (i < 0)
    return 0;
  if ((ss.st_mode & S_IFREG) || (ss.st_mode & S_IFLNK))
    return 1;
  return 0;
}

int can_stat(const char *s)
{
  struct stat ss;
  int i = stat(s, &ss);

  if (i < 0)
    return 0;
  return 1;
}

int can_lstat(const char *s)
{
  struct stat ss;
  int i = lstat(s, &ss);

  if (i < 0)
    return 0;
  return 1;
}

int is_symlink(const char *s)
{
  struct stat ss;
  int i = lstat(s, &ss);

  if (i < 0)
    return 0;
  if (ss.st_mode & S_IFLNK)
    return 1;
  return 0;
}

int is_dir(const char *s)
{
  struct stat ss;
  int i = stat(s, &ss);

  if (i < 0)
    return 0;
  if (ss.st_mode & S_IFDIR)
    return 1;
  return 0;
}

int fixmod(const char *s)
{
#ifndef CYGWIN_HACKS
  if (!can_stat(s))
    return 1;
  return chmod(s, S_IRUSR | S_IWUSR | S_IXUSR);
#else
  return 0;
#endif /* !CYGWIN_HACKS */
}

Tempfile::Tempfile()
{
  len = strlen(tempdir) + 1 + 6 + 1;
  file = new char[len];
  sprintf(file, "%s.XXXXXX", tempdir);

  MakeTemp();
}

Tempfile::Tempfile(const char *prefix)
{
  len = strlen(tempdir) + 1 + strlen(prefix) + 1 + 6 + 1;
  file = new char[len];
  sprintf(file, "%s.%s-XXXXXX", tempdir, prefix);

  MakeTemp();
}

void Tempfile::MakeTemp()
{
  if ((fd = mkstemp(file)) < 0) {
    f = NULL;
    goto error;    
  }

  if ((f = fdopen(fd, "w+b")) == NULL)
    goto error;
  
  fchmod(fd, S_IRUSR | S_IWUSR);

  return;

error:
  putlog(LOG_ERRORS, "*", "Couldn't create temporary file '%s': %s", file, strerror(errno));
  delete this;
  fatal("Cannot create tempory file!", 0);
}

Tempfile::~Tempfile()
{
  unlink(file);
  if (f)
    fclose(f);
  else
    close(fd);
  delete[] file;
}

void check_tempdir()
{
  if (!can_stat(tempdir)) {
    if (mkdir(tempdir,  S_IRUSR | S_IWUSR | S_IXUSR)) {
      unlink(tempdir);
      if (!can_stat(tempdir))
        if (mkdir(tempdir, S_IRUSR | S_IWUSR | S_IXUSR))
          werr(ERR_TMPSTAT);
    }
  }
  if (fixmod(tempdir))
    werr(ERR_TMPMOD);

  /* test tempdir: it's vital */
  {
    Tempfile *testdir = new Tempfile("test");
    int result;

    fprintf(testdir->f, "\n");
    result = fflush(testdir->f);
    delete testdir;
    if (result)
      fatal(strerror(errno), 0);
  }
}

