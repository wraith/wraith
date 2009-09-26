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

void EncryptedStream::printf (const char* format, ...)
{
  char va_out[1024] = "";
  va_list va;
  size_t len = 0;

  va_start(va, format);
  len = vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  bd::String string(va_out, len);
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
