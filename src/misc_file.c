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
#include "shell.h"
#include "binary.h"

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
  if (!can_stat(s))
    return 1;
  return chmod(s, S_IRUSR | S_IWUSR | S_IXUSR);
}

Tempfile::Tempfile()
{
  len = strlen(tempdir) + 1 + 6 + 1;
  file = new char[len];
  simple_sprintf(file, "%s.XXXXXX", tempdir);

  MakeTemp();
}

Tempfile::Tempfile(const char *prefix)
{
  len = strlen(tempdir) + 1 + strlen(prefix) + 1 + 6 + 1;
  file = new char[len];
  simple_sprintf(file, "%s.%s-XXXXXX", tempdir, prefix);

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

  error = 0;
  return;

error:
  putlog(LOG_ERRORS, "*", "Couldn't create temporary file '%s': %s", file, strerror(errno));
  error = 1;
//  fatal("Cannot create temporary file!", 0);
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
  int result;

  fprintf(testdir->f, "\n");
  result = fflush(testdir->f);
  delete testdir;
  if (result) {
    sdprintf("%s: %s", tempdir, strerror(errno));
    return 0;
  }
  return 1;
}

void Tempfile::FindDir()
{
  /* this is temporary until we make tmpdir customizable */
#ifdef CYGWIN_HACKS
  simple_snprintf(tempdir, DIRMAX, "./tmp/");
  if (!check_tempdir(0)) {
    clear_tmpdir = 0;
    simple_snprintf(tempdir, DIRMAX, "./");
  }
  return;
#else

  /* If this is a hub, use, "./tmp/" */
  if (conf.bots && conf.bots->nick && conf.bots->hub) {
    simple_snprintf(tempdir, DIRMAX, "%s/tmp/", conf.binpath);
    if (check_tempdir(0))
      return;
  }
  
/* WHY
  else if (conf.homedir && conf.homedir[0]) {
    //need to create ~/.ssh/  
    simple_snprintf(tempdir, DIRMAX, "%s/.ssh/", conf.homedir);
    clear_tmpdir = 0;
    check_tempdir(0);
    clear_tmpdir = 1;
    simple_snprintf(tempdir, DIRMAX, "%s/.ssh/.../", conf.homedir);
  }
*/

  /* The dirs we WANT to use aren't accessible, try a random one instead to get the job done. */

  clear_tmpdir = 0;

  char *dirs[] = {
    "/tmp/",
    "/usr/tmp/",
    "/var/tmp/",
    "./",
    NULL
  };
  int i = 0;
  bool found = 0;
  char *pack_hash = NULL;

  if (settings_crypt == PACK_DEC) {
    pack_hash = MD5(settings.packname);
    pack_hash[5] = 0;
  }

  for (i = 0; dirs[i]; i++) {
    if (pack_hash)
      simple_snprintf(tempdir, DIRMAX, "%s.%s/", dirs[i], pack_hash);
    else
      strlcpy(tempdir, dirs[i], DIRMAX);
    if (check_tempdir(0)) {
      found = 1;
      break;
    }
  }

  if (!found)
    werr(ERR_TMPSTAT);
#endif /* CYGWIN_HACKS */
}
