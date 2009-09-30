/* EncryptedStream.c
 *
 */
#include "base64.h"
#include <bdlib/src/String.h>
#include "EncryptedStream.h"
#include <stdarg.h>
#include "compat/compat.h"

bd::String EncryptedStream::gets (size_t maxSize, char delim) {
  if (!key.length()) return bd::Stream::gets(maxSize, delim);
  bd::String tmp(bd::Stream::gets(maxSize, delim));
  if (delim && tmp[tmp.length() - 1] == delim)
    --tmp;
  bd::String decrypted(decrypt_string(key, broken_base64Decode(tmp)));
  /* The delimeter (\n) is not encrypted */
  if (delim)
    decrypted += delim;
  return decrypted;
}

void EncryptedStream::puts (const bd::String& str_in)
{
  if (loading) {
    bd::Stream::puts(str_in);
    return;
  }
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
