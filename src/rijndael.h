#ifndef __RIJNDAEL_H
#define __RIJNDAEL_H

#include "main.h"
#include "eggdrop.h"

/* Set this to 16/24/32 which ofcourse maps to 128/192/256 bits */
#define CRYPT_KEYSIZE          16

/* Set this to any number you feel like, this is the number of
   "prepared" keys that are cached for later use. Better set it
   as low as possible, scanning a few thousand keys for a match
   (yes i scan the list :/ ) would cost more than the caching
   would help speed. But then, set it too low and the cache could
   be pretty useless. Remeber there will be two cache entries for
   a key, one encrypt and one decrypt
*/

#define CRYPT_CACHE         4

/* These are what u need to "extern" in anything using this src */

/* Encrypt binary data, return a malloc'd buffer the caller must free
   datalen rounded up to a multiplum of the block size (16/32/64) */
char *encrypt_binary(char *key, char *data, int *datalen);

/* Decrypt binary data, return a malloc'd buffer the caller must free. */
char *decrypt_binary(char *key, char *data, int datalen);

/* Encrypt a string, return a malloc'd buffer the caller must free. return
   data is base 64 encoded */
char *encrypt_string(char *key, char *data);

/* Decrypt a string (must be base 64 encoded) and returns the original 
   data in a malloced buffer that, again, the caller must free */
char *decrypt_string(char *key, char *data);

#endif /* __RIJNDAEL_ALG_H */
