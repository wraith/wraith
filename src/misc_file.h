/*
 * misc_file.h
 *   prototypes for misc_file.c
 *
 */

#ifndef _EGG_MISC_FILE_H
#define _EGG_MISC_FILE_H

#include <stdio.h>

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
    Tempfile();					//constructor
    Tempfile(const char *prefix);		//constructor with file prefix
    ~Tempfile();				//destructor

    FILE *f;
    int fd;
    char *file;
    size_t len;
  private:
    void MakeTemp();				//Used for mktemp() and checking
};

#endif				/* _EGG_MISC_FILE_H */
