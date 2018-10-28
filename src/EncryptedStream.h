#ifndef _ENCRYPTEDSTREAM_H
#define _ENCRYPTEDSTREAM_H 1

namespace bd {
  class String;
}
#include <iostream>
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ENC_AES_256_ECB 	1
#define ENC_AES_256_CBC		2
#define ENC_BASE64_BROKEN	4
#define ENC_BASE64		8
#define ENC_KEEP_NEWLINES	16
#define ENC_NO_HEADER		32

#define ENC_DEFAULT 		(ENC_AES_256_CBC)

class EncryptedStream : public bd::Stream {
  private:
        bd::String key;
        mutable char enc_flags;
        void apply_filters(bd::String& buf, const bd::String& IV) const;
        void unapply_filters(bd::String& buf, const bd::String& IV) const;

  protected:

  public:
        EncryptedStream(const char* keyStr) : Stream(), key(bd::String(keyStr)), enc_flags(ENC_DEFAULT) {};
        EncryptedStream(bd::String& keyStr) : Stream(), key(keyStr), enc_flags(ENC_DEFAULT) {};
        EncryptedStream(EncryptedStream& stream) : Stream(stream), key(stream.key), enc_flags(ENC_DEFAULT) {};

        void setFlags(const char _enc_flags) const noexcept {
          enc_flags = _enc_flags;
        }
        virtual int loadFile(const int fd);
        using bd::Stream::loadFile;

        virtual int writeFile(const int fd) const;
        using bd::Stream::writeFile;
};
#endif
