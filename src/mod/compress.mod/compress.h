#ifndef _EGG_MOD_COMPRESS_COMPRESS_H
#define _EGG_MOD_COMPRESS_COMPRESS_H
#define UFF_COMPRESS	0x000008
typedef enum
{ COMPF_ERROR, COMPF_SUCCESS } compf_result;
typedef enum
{ COMPF_UNCOMPRESSED, COMPF_COMPRESSED, COMPF_FAILED } compf_type;
#ifndef MAKING_COMPRESS
#define compress_to_file ((int (*)(char *, char *, int))(compress_funcs[4]))
#define compress_file ((int (*)(char *, int))(compress_funcs[5]))
#define uncompress_to_file ((int (*)(char *, char *))(uncompress_funcs[6]))
#define uncompress_file ((int (*)(char *))(uncompress_funcs[7]))
#define is_compressedfile ((int (*)(char *))(uncompress_funcs[8]))
#endif
#endif
