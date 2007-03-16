/* Base64.h
 *
 * Copyright (C) Bryan Drewery
 *
 * This program is private and may not be distributed, modified
 * or used without express permission of Bryan Drewery.
 *
 * THIS PROGRAM IS DISTRIBUTED WITHOUT ANY WARRANTY.
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 */
#ifndef _W_BASE64_H
#define _W_BASE64_H 1

#include "bdlib.h"

BDLIB_NS_BEGIN
class Base64 {
};

/**
  * @brief Encode a plaintext string into base64 (returns a buffer)
  * @param src A c-style string to encode
  * @param len Reference to length of string (to be updated on return)
  * @return An encoded NULL-terminated c-style string (must be free()d later)
  */
char *b64enc(const unsigned char *src, size_t *len);

/**
  * @brief Encode a plaintext string into base64 (using a given buffer)
  * @param data The c-style string to encode
  * @param len Reference to length of string (to be updated on return)
  * @param dest Reference to the buffer to encode into
  */
void b64enc_buf(const unsigned char *data, size_t *len, char *dest);

/**
  * @brief Decode a base64 encoded string into plaintext (returns a buffer)
  * @param src A c-style string to decode
  * @param len Reference to length of string (to be updated on return)
  * @return A decoded NULL-terminated c-style string (must be free()d later)
  */
char *b64dec(const unsigned char *data, size_t *len);

/**
  * @brief Decode a base64 encoded string into plaintext (using a given buffer)
  * @param data The c-style string to decode
  * @param len Reference to length of string (to be updated on return)
  * @param dest Reference to the buffer to decode into
  */
void b64dec_buf(const unsigned char *data, size_t *len, char *dest);

BDLIB_NS_END
#endif /* !_W_BASE64_H */ 
