/* EncryptedStream.c
 *
 */
#include "base64.h"
#include "misc.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include "EncryptedStream.h"
#include <stdarg.h>
#include "compat/compat.h"

int EncryptedStream::loadFile (const int fd) {
  if (!key.length()) return bd::Stream::loadFile(fd);
  if (bd::Stream::loadFile(fd) == 1)
    return 1;

  bd::String in_buf;

  /* Peak at the first few bytes to determine the algorithm used */
  if (str[0] == 0x7F && str[2] == 0x7F) {
    enc_flags = str[1];
    in_buf = str(3, str.length() - 3);
  } else {
    enc_flags |= ENC_NO_HEADER;

    // Old socksfile format?
    if (bd::String(str(0, 5)) == "+enc\n") {
      enc_flags |= (ENC_KEEP_NEWLINES|ENC_AES_256_ECB|ENC_BASE64_BROKEN);
      in_buf = str(5, str.length() - 5);
    } else {
      /* Peak at the first block to see if it matches a userfile or an old conf file */
      bd::String peek(decrypt_string(key, broken_base64Decode(str(0, 32))));
      if (peek(0, 4) == "#4v:" || peek(0, 2) == "! ")
        enc_flags |= (ENC_KEEP_NEWLINES|ENC_AES_256_ECB|ENC_BASE64_BROKEN);
      in_buf = str;
    }
  }

  bd::String IV;

  if (enc_flags & ENC_AES_256_CBC) {
    IV = in_buf(0, 16);
    in_buf += 16;
  }

  if (enc_flags & ENC_KEEP_NEWLINES) {
    bd::String buf(in_buf), line, out_buf;
    while (buf.length()) {
      line = newsplit(buf, '\n');
      unapply_filters(line, IV);
      line += '\n';
      out_buf += line;
    }
    in_buf = out_buf;
  } else {
    unapply_filters(in_buf, IV);
  }

  str = in_buf;
  return 0;
}

void EncryptedStream::apply_filters(bd::String& buf, const bd::String& IV) const {
  if (enc_flags & ENC_AES_256_CBC) {
    unsigned char* iv = (unsigned char*) strdup(IV.c_str());
    buf = encrypt_string_cbc(key, buf, iv);
    free(iv);
  } else if (enc_flags & ENC_AES_256_ECB)
    buf = encrypt_string(key, buf);

  if (enc_flags & ENC_BASE64_BROKEN)
    buf = broken_base64Encode(buf);
  if (enc_flags & ENC_BASE64)
    buf = bd::base64Encode(buf);
}

void EncryptedStream::unapply_filters(bd::String& buf, const bd::String& IV) const {
  if (enc_flags & ENC_BASE64_BROKEN)
    buf = broken_base64Decode(buf);
  if (enc_flags & ENC_BASE64)
    buf = bd::base64Decode(buf);

  if (enc_flags & ENC_AES_256_CBC) {
    unsigned char* iv = (unsigned char*) strdup(IV.c_str());
    buf = decrypt_string_cbc(key, buf, iv);
    free(iv);
  }
  else if (enc_flags & ENC_AES_256_ECB)
    buf = decrypt_string(key, buf);
}

int EncryptedStream::writeFile (const int fd) const {
  if (!key.length()) return bd::Stream::writeFile(fd);

  /* Encrypt the stream before writing it out */
  bd::String IV;
  if (enc_flags & ENC_AES_256_CBC) {
    char rand_string[17] = "";
    make_rand_str(rand_string, sizeof(rand_string) - 1);
    IV = bd::String(rand_string, 16);
  }

  bd::String out_buf;
  if (enc_flags & ENC_KEEP_NEWLINES) {
    bd::String buf(str), line;
    while (buf.length()) {
      line = newsplit(buf, '\n');
      apply_filters(line, IV);
      line += '\n';
      out_buf += line;
    }

  } else {
    out_buf = str;
    apply_filters(out_buf, IV);
  }

  if (enc_flags & ENC_NO_HEADER)
    return bd::Stream(out_buf).writeFile(fd);

  const char encoding[3] = {0x7F, enc_flags, 0x7F};
  bd::String encrypted(encoding, 3);
  if (enc_flags & ENC_AES_256_CBC)
    encrypted += IV;
  encrypted += out_buf;
  return bd::Stream(encrypted).writeFile(fd);
}
