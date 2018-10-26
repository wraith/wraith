#ifndef _RFCSTRING_H
#define _RFCSTRING_H 1

namespace bd {
  class String;
}
#include "rfc1459.h"
#include <bdlib/src/String.h>

class RfcString : public bd::String {
  private:
    static bool rfc_equal(const char c1, const char c2) __attribute__((pure));

  protected:

  public:
    using String::String;
    RfcString(const String &str) : String(str) {};
    RfcString(String &&str) : String(std::move(str)) {};

    int compare(const RfcString& str, size_t n = npos) const
      __attribute__((pure));
    friend bool operator==(const RfcString&, const RfcString&);
    friend bool operator!=(const RfcString&, const RfcString&);
    friend bool operator<(const RfcString&, const RfcString&);
    friend bool operator<=(const RfcString&, const RfcString&);
    friend bool operator>(const RfcString&, const RfcString&);
    friend bool operator>=(const RfcString&, const RfcString&);

    virtual size_t hash() const;
};

inline bool __attribute__((pure))
operator==(const RfcString& lhs, const RfcString& rhs) {
  return (lhs.length() == rhs.length() &&
      lhs.compare(rhs) == 0);
}

inline bool __attribute__((pure))
operator!=(const RfcString& lhs, const RfcString& rhs) {
  return ! (lhs == rhs);
}

inline bool __attribute__((pure))
operator<(const RfcString& lhs, const RfcString& rhs) {
  return (lhs.compare(rhs) < 0);
}

inline bool __attribute__((pure))
operator<=(const RfcString& lhs, const RfcString& rhs) {
  return ! (rhs < lhs);
}

inline bool __attribute__((pure))
operator>(const RfcString& lhs, const RfcString& rhs) {
  return (rhs < lhs);
}

inline bool __attribute__((pure))
operator>=(const RfcString& lhs, const RfcString& rhs) {
  return ! (lhs < rhs);
}

namespace std {
  template<>
  struct hash<RfcString>
    {
          inline size_t operator()(const RfcString& val) const {
            return val.hash();
          }
    };
}

#endif
