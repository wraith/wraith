/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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
 * misc.c -- handles:
 *   copyfile() movefile()
 *
 */


#include "common.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include "stat.h"
#include "misc_file.h"
#include "main.h"
#include "shell.h"
#include "binary.h"

static bool looking = 0;

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

  fi = open(oldpath, O_RDONLY, 0);
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
  fsync(fo);
  close(fo);
  close(fi);
  return 0;
}

int movefile(const char *oldpath, const char *newpath)
{
  /* Try to use rename first */
  if (!rename(oldpath, newpath))
    return 0;

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
  if (!can_stat(s))
    return 1;
  return chmod(s, BINMOD);
}

Tempfile::Tempfile(const char *_prefix, bool _useFopen)
{
  this->useFopen = _useFopen;
  this->f = NULL;
  this->fd = -1;
  if (_prefix) {
    plen = strlen(_prefix) + 1;
    this->prefix = new char[plen];
    strlcpy(this->prefix, _prefix, plen);
  } else {
    this->prefix = NULL;
    plen = -1; /* to swallow the '-' */
  }

  AllocTempfile();
}

void Tempfile::AllocTempfile()
{
  len = strlen(tempdir) + 1 + plen + 1 + 6 + 1;
  file = new char[len];

  if (prefix)
    simple_snprintf(file, len, "%s.%s-XXXXXX", tempdir, prefix);
  else
    simple_snprintf(file, len, "%s.XXXXXX", tempdir);

  MakeTemp();
}

void Tempfile::MakeTemp()
{
  if ((fd = mkstemp(file)) < 0) {
    f = NULL;
    goto error;    
  }

  if (this->useFopen) {
    if ((f = fdopen(fd, "w+b")) == NULL)
      goto error;
  }
  
  fchmod(fd, S_IRUSR | S_IWUSR);

  error = 0;
  return;

error:
  putlog(LOG_ERRORS, "*", "Couldn't create temporary file '%s': %s", file, strerror(errno));
  error = 1;
  /* Since we failed to create a file in the given tempdir, let's try finding a new one */
  if (!looking) {
    /* ... Not finding a new tempdir is fatal. */
    if (FindDir() == ERROR) {
      delete[] file;
      file = NULL;
      return;
    }
    /* ... If we found one, let's try all over! */
    else {
      error = 0;
      delete[] file;
      file = NULL;
      AllocTempfile();
    }
  }
}

void Tempfile::my_close()
{
  if (f) {
    fclose(f);
    f = NULL;
  } else if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

Tempfile::~Tempfile()
{
  if (this->prefix)
    delete[] this->prefix;
  unlink(file);
  my_close();
  delete[] file;
}

static bool check_tempdir(bool do_mod)
{
  mkdir_p(tempdir);

  if (!can_stat(tempdir))
    return 0;

  if (do_mod && fixmod(tempdir))
    return 0;

  /* test tempdir: it's vital */
  Tempfile *testdir = new Tempfile("test");

  /* There was an error creating a file in this directory, return to move on in list of dirs */
  if (!testdir || testdir->error) {
    delete testdir;
    return 0;
  }

  fprintf(testdir->f, "\n");
  int result = fflush(testdir->f);
  delete testdir;
  if (result) {
    sdprintf("%s: %s", tempdir, strerror(errno));
    return 0;
  }
  return 1;
}

bool Tempfile::FindDir()
{
  /* this is temporary until we make tmpdir customizable */

  looking = 1;

  /* If this is a hub, use, "./tmp/" */
  if (conf.bots && conf.bots->nick && conf.bots->hub) {
    simple_snprintf(tempdir, DIRMAX, "%s/tmp/", dirname(binname));
    if (check_tempdir(0)) {
      looking = 0;
      return OK;
    }
  }
  
  /* The dirs we WANT to use aren't accessible, try a random one instead to get the job done. */
  clear_tmpdir = 0;

  const char *dirs[] = {
    "/tmp/",
    "/usr/tmp/",
    "/var/tmp/",
    "./",
    NULL
  };

  for (int i = 0; dirs[i]; i++) {
    if (!realpath(dirs[i], tempdir)) {
      ;
    }
    tempdir[(DIRMAX < PATH_MAX ? DIRMAX : PATH_MAX) - 1] = 0;
    size_t len = strlen(tempdir);
    tempdir[len] = '/';
    tempdir[len + 1] = '\0';
    if (check_tempdir(0)) {
      looking = 0;
      return OK;
    }
  }

  return ERROR;
}
/* vim: set sts=2 sw=2 ts=8 et: */
