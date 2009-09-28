#ifndef _ENCRYPTEDSTREAM_H
#define _ENCRYPTEDSTREAM_H 1

namespace bd {
  class String;
}
#include <iostream>
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

class EncryptedStream : public bd::Stream {
  private:
        bd::String key;

  protected:

  public:
        EncryptedStream(const char* keyStr) : Stream(), key(bd::String(keyStr)) {};
        EncryptedStream(bd::String& keyStr) : Stream(), key(keyStr) {};
        EncryptedStream(EncryptedStream& stream) : Stream(stream), key(stream.key) {};

        virtual bd::String gets(size_t, char delim = 0);
        virtual void puts (const bd::String& string);
};
#endif
