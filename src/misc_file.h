/*
 * misc_file.h
 *   prototypes for misc_file.c
 *
 */

#ifndef _EGG_MISC_FILE_H
#define _EGG_MISC_FILE_H

int copyfile(const char *, const char *);
int movefile(const char *, const char *);
int is_file(const char *);
int can_stat(const char *);
int can_lstat(const char *);
int is_symlink(const char *);
int is_dir(const char *);
int fixmod(const char *);

#endif				/* _EGG_MISC_FILE_H */
