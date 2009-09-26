/* EncryptedStream.c
 *
 */
#include "base64.h"
#include <bdlib/src/String.h>
#include "EncryptedStream.h"
#include <stdarg.h>
#include "compat/compat.h"

int EncryptedStream::gets (char *_data, size_t maxSize) {
  size_t size = bd::Stream::gets(_data, maxSize);
  if (key.length()) {
    bd::String tmp(_data, size);
    if (tmp[tmp.length() - 1] == '\n')
      --tmp;
    bd::String decrypted(decrypt_string(key, broken_base64Decode(tmp)));
    decrypted += '\n';
    strlcpy(_data, decrypted.c_str(), maxSize);
    return decrypted.length();
  }
  return size;
}

void EncryptedStream::puts (const bd::String& str_in)
{
  bd::String string(str_in);
  if (key.length()) {
    if (string[string.length() - 1] == '\n')
      --string;
    bd::String encrypted(broken_base64Encode(encrypt_string(key, string)));
    encrypted += '\n';
    bd::Stream::puts(encrypted);
    return;
  }
  bd::Stream::puts(string);
}
