/*
 * misc_file.h
 *   prototypes for misc_file.c
 *
 */

#ifndef _EGG_MISC_FILE_H
#define _EGG_MISC_FILE_H

#include <stdio.h>

#define BINMOD		S_IRUSR | S_IWUSR | S_IXUSR

int copyfile(const char *, const char *);
int movefile(const char *, const char *);
int is_file(const char *);
int can_stat(const char *);
int can_lstat(const char *);
int is_symlink(const char *);
int is_dir(const char *);
int fixmod(const char *);

class Tempfile 
{
  public:
    Tempfile() { Tempfile(NULL); };					//constructor
    Tempfile(const char *prefix, bool useFopen = 1);		//constructor with file prefix

    void AllocTempfile();			//constructor with file prefix
    void my_close();
    ~Tempfile();				//destructor
    static bool FindDir() noexcept;

    bool error;					//exceptions are lame.
    FILE *f;
    char *file;
    int fd;
    size_t len;

  private:
    char *prefix;
    int plen;
    void MakeTemp() noexcept;			//Used for mktemp() and checking
    bool useFopen;
};

#endif				/* _EGG_MISC_FILE_H */
