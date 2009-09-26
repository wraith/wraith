#ifndef _ENCRYPTEDSTREAM_H
#define _ENCRYPTEDSTREAM_H 1

namespace bd {
  class String;
}
#include <iostream>
#include <bdlib/src/Stream.h>

class EncryptedStream : public bd::Stream {
  private:
        bd::String key;

  protected:

  public:
        EncryptedStream(const char* keyStr) : Stream(), key(bd::String(keyStr)) {};
        EncryptedStream(bd::String& keyStr) : Stream(), key(keyStr) {};
        EncryptedStream(EncryptedStream& stream) : Stream(stream), key(stream.key) {};

        virtual int gets(char *, size_t);
#ifdef __GNUC__
        /* GNU GCC DOC:
           Since non-static C++ methods have an implicit this argument, the arguments of such methods
           should be counted from two, not one, when giving values for string-index and first-to-check.
         */
        virtual void printf(const char*, ...) __attribute__ ((format(printf, 2, 3)));
#else
	virtual void printf(const char*, ...);
#endif

};
#endif
