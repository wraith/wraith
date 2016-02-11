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
 * compress.c -- part of compress.mod
 *   uses the compression library libz to compress and uncompress the
 *   userfiles during the sharing process
 *
 * Written by Fabian Knittel <fknittel@gmx.de>. Based on zlib examples
 * by Jean-loup Gailly and Miguel Albrecht.
 *
 */
/*
 * Copyright (C) 2000, 2001, 2002 Eggheads Development Team
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


#define MODULE_NAME "compress"

#include "src/common.h"
#include "src/misc_file.h"
#include "src/misc.h"

#define free_func zlib_free_func
#include <zlib.h>
#undef free_func
#include <string.h>
#include <errno.h>

#include "src/mod/share.mod/share.h"

#ifdef HAVE_MMAP
#  include <sys/types.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif /* HAVE_MMAP */
#include "compress.h"

#define BUFLEN	512

static unsigned int compressed_files;	/* Number of files compressed.      */
static unsigned int uncompressed_files;	/* Number of files uncompressed.    */
static unsigned int share_compressed;	/* Compress userfiles when sharing? */
static unsigned int compress_level = 9;	/* Default compression used.	    */


static int uncompress_to_file(char *f_src, char *f_target);
static int compress_to_file(char *f_src, char *f_target, int mode_num);
int compress_file(char *filename, int mode_num);
int uncompress_file(char *filename);
static int is_compressedfile(char *filename);


/*
 *    Misc functions.
 */

static int is_compressedfile(char *filename)
{
  if (!is_file(filename))
    return COMPF_FAILED;

  char buf1[50] = "", buf2[50] = "";
  FILE *fin = NULL;
  int len1, len2, i;

  /* Read data with zlib routines.
   */
  fin = gzopen(filename, "rb");
  if (!fin)
    return COMPF_FAILED;
  len1 = gzread(fin, buf1, sizeof(buf1));
  if (len1 < 0)
    return COMPF_FAILED;
  if (gzclose(fin) != Z_OK)
    return COMPF_FAILED;

  /* Read raw data.
   */
  fin = fopen(filename, "rb");
  if (!fin)
    return COMPF_FAILED;
  len2 = fread(buf2, 1, sizeof(buf2), fin);
  if (ferror(fin)) {
    fclose(fin);
    return COMPF_FAILED;
  }
  fclose(fin);

  /* Compare what we found.
   */
  if (len1 != len2)
    return COMPF_COMPRESSED;
  for (i = 0; i < sizeof(buf1); i++)
    if (buf1[i] != buf2[i])
      return COMPF_COMPRESSED;
  return COMPF_UNCOMPRESSED;
}


/*
 *    General compression / uncompression functions
 */

/* Uncompresses a file `f_src' and saves it as `f_target'.
 */
static int uncompress_to_file(char *f_src, char *f_target)
{
  if (!is_file(f_src)) {
    putlog(LOG_MISC, "*", "Failed to uncompress file `%s': not a file.",
	   f_src);
    return COMPF_ERROR;
  }

  char buf[BUFLEN] = "";
  int len;
  FILE *fin = NULL, *fout = NULL;

  fin = gzopen(f_src, "rb");
  if (!fin) {
    putlog(LOG_MISC, "*", "Failed to uncompress file `%s': gzopen failed.",
	   f_src);
    return COMPF_ERROR;
  }

  fout = fopen(f_target, "wb");
  if (!fout) {
    putlog(LOG_MISC, "*", "Failed to uncompress file `%s': open failed: %s.",
	   f_src, strerror(errno));
    return COMPF_ERROR;
  }

  while (1) {
    len = gzread(fin, buf, sizeof(buf));
    if (len < 0) {
      putlog(LOG_MISC, "*", "Failed to uncompress file `%s': gzread failed.",
	     f_src);
      return COMPF_ERROR;
    }
    if (!len)
      break;
    if ((int) fwrite(buf, (unsigned int) len, 1, fout) != 1) {
      putlog(LOG_MISC, "*", "Failed to uncompress file `%s': fwrite failed: %s.",
	     f_src, strerror(errno));
      return COMPF_ERROR;
    }
  }
  if (fclose(fout)) {
    putlog(LOG_MISC, "*", "Failed to uncompress file `%s': fclose failed: %s.",
	   f_src, strerror(errno));
    return COMPF_ERROR;
  }
  if (gzclose(fin) != Z_OK) {
    putlog(LOG_MISC, "*", "Failed to uncompress file `%s': gzclose failed.",
	   f_src);
    return COMPF_ERROR;
  }
  uncompressed_files++;
  return COMPF_SUCCESS;
}

/* Enforce limits.
 */
inline static void adjust_mode_num(int *mode)
{
  if (*mode > 9)
    *mode = 9;
  else if (*mode < 0)
    *mode = 0;
}

#ifdef HAVE_MMAP
/* Attempt to compress in one go, by mmap'ing the file to memory.
 */
static int compress_to_file_mmap(FILE *fout, FILE *fin)
{
    size_t len;
    int ifd = fileno(fin);
    char *buf = NULL;
    struct stat st;

    /* Find out size of file */
    if (fstat(ifd, &st) < 0)
      return COMPF_ERROR;
    if (st.st_size <= 0)
      return COMPF_ERROR;

    /* mmap file contents to memory */
    buf = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, ifd, 0);
    if (buf < 0)
      return COMPF_ERROR;

    /* Compress the whole file in one go */
    len = gzwrite(fout, buf, st.st_size);
    if (len != (int) st.st_size)
      return COMPF_ERROR;

    munmap(buf, st.st_size);
    fclose(fin);
    if (gzclose(fout) != Z_OK)
      return COMPF_ERROR;
    return COMPF_SUCCESS;
}
#endif /* HAVE_MMAP */

/* Compresses a file `f_src' and saves it as `f_target'.
 */
static int compress_to_file(char *f_src, char *f_target, int mode_num)
{
  char buf[BUFLEN] = "", mode[5] = "";
  FILE *fin = NULL, *fout = NULL;
  size_t len;
  int ret = COMPF_ERROR;

  adjust_mode_num(&mode_num);
  simple_snprintf(mode, sizeof mode, "wb%d", mode_num);

  if (!is_file(f_src)) {
    putlog(LOG_MISC, "*", "Failed to compress file `%s': not a file.",
	   f_src);
    goto err;
  }
  fin = fopen(f_src, "rb");
  if (!fin) {
    putlog(LOG_MISC, "*", "Failed to compress file `%s': open failed: %s.",
	   f_src, strerror(errno));
    goto err;
  }

  fout = gzopen(f_target, mode);
  if (!fout) {
    putlog(LOG_MISC, "*", "Failed to compress file `%s': gzopen failed.",
	   f_src);
    goto err;
  }

#ifdef HAVE_MMAP
  if (compress_to_file_mmap(fout, fin) == COMPF_SUCCESS) {
    compressed_files++;
    return COMPF_SUCCESS;
  } else {
    /* To be on the safe side, close the file before attempting
     * to write again.
     */
    gzclose(fout);
    fout = gzopen(f_target, mode);
  }
#endif /* HAVE_MMAP */

  while (1) {
    len = fread(buf, 1, sizeof(buf), fin);
    if (ferror(fin)) {
      putlog(LOG_MISC, "*", "Failed to compress file `%s': fread failed: %s",
	     f_src, strerror(errno));
      goto err;
    }
    if (!len)
      break;
    if (gzwrite(fout, buf, (unsigned int) len) != len) {
      putlog(LOG_MISC, "*", "Failed to compress file `%s': gzwrite failed.",
	     f_src);
      goto err;
    }
  }

  if (gzclose(fout) != Z_OK) {
    putlog(LOG_MISC, "*", "Failed to compress file `%s': gzclose failed.",
	   f_src);
    goto err;
    return COMPF_ERROR;
  }
  compressed_files++;
  ret = COMPF_SUCCESS;
err:
  if (fin)
    fclose(fin);
  return ret;
}

/* Compresses a file `filename' and saves it as `filename'.
 */
int compress_file(char *filename, int mode_num)
{
  char *temp_fn = NULL, randstr[5] = "";
  int ret;

  /* Create temporary filename. */
  temp_fn = (char *) calloc(1, strlen(filename) + 5);
  make_rand_str(randstr, 4);
  strcpy(temp_fn, filename);
  strcat(temp_fn, randstr);

  /* Compress file. */
  ret = compress_to_file(filename, temp_fn, mode_num);

  /* Overwrite old file with compressed version.  Only do so
   * if the compression routine succeeded.
   */
  if (ret == COMPF_SUCCESS)
    movefile(temp_fn, filename);

  free(temp_fn);
  return ret;
}

/* Uncompresses a file `filename' and saves it as `filename'.
 */
int uncompress_file(char *filename)
{
  char *temp_fn = NULL, randstr[5] = "";
  int ret;

  /* Create temporary filename. */
  temp_fn = (char *) calloc(1, strlen(filename) + 5);
  make_rand_str(randstr, 4);
  strcpy(temp_fn, filename);
  strcat(temp_fn, randstr);

  /* Uncompress file. */
  ret = uncompress_to_file(filename, temp_fn);

  /* Overwrite old file with uncompressed version.  Only do so
   * if the uncompression routine succeeded.
   */
  if (ret == COMPF_SUCCESS)
    movefile(temp_fn, filename);

  free(temp_fn);
  return ret;
}


/*
 *    Userfile feature releated functions
 */

static int uff_comp(int idx, char *filename)
{
  putlog(LOG_BOTS, "*", "Compressing user file for %s.", dcc[idx].nick);
  return compress_file(filename, compress_level);
}

static int uff_uncomp(int idx, char *filename)
{
  putlog(LOG_BOTS, "*", "Uncompressing user file from %s.", dcc[idx].nick);
  return uncompress_file(filename);
}

static int uff_ask_compress(int idx)
{
  if (share_compressed)
    return 1;
  else
    return 0;
}

static uff_table_t compress_uff_table[] = {
  {"compress",	UFF_COMPRESS,	uff_ask_compress, 100, uff_comp, uff_uncomp},
  {NULL,	0,		NULL,		    0,	   NULL,       NULL}
};

/*
 *    Compress module related code
 */

int compress_report(int idx, int details)
{
  if (details) {

    dprintf(idx, "    %u file%s compressed\n", compressed_files,
            (compressed_files != 1) ? "s" : "");
    dprintf(idx, "    %u file%s uncompressed\n", uncompressed_files,
            (uncompressed_files != 1) ? "s" : "");
  }

  return 0;
}

void compress_init()
{
  uff_addtable(compress_uff_table);
}
/* vim: set sts=2 sw=2 ts=8 et: */
