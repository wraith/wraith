/* RfcString.cc
 *
 */
#include <stdlib.h>
#include "RfcString.h"

bool
RfcString::rfc_equal(const char c1, const char c2) noexcept {
  if (c1 == c2)
    return true;
  return (rfc_toupper(c1) == rfc_toupper(c2));
}

int
RfcString::compare(const RfcString& str, size_t n) const noexcept
{
  /* Same string? */
  if (cbegin() == str.cbegin() && length() == str.length())
    return 0;
  if (n == npos)
    n = std::max(length(), str.length());
  auto s1 = cbegin();
  auto s2 = str.cbegin();
  /* XXX: std::lexicographical_compare_3way would be nice ... */
  int cmp = 0;
  while (n > 0 && s1 != cend() && s2 != str.cend()) {
    if ((cmp = rfc_toupper(*s1) - rfc_toupper(*s2)) != 0)
      return cmp;
    ++s1;
    ++s2;
    --n;
  }
  if (n == 0)
    return 0;
  else if (s1 != cend())
    return 1;
  else if (s2 != str.cend())
    return -1;
  else
    return 0;
  return rfc_toupper(*s1) - rfc_toupper(*s2);
}

size_t
RfcString::hash() const noexcept {
  if (my_hash != 0) return my_hash;
  std::hash<value_type> hasher;
  size_t _hash = 5381;

  for(size_t i = 0; i < this->length(); ++i)
    _hash = ((_hash << 5) + _hash) + hasher(rfc_toupper(this->data()[i]));
  return (my_hash = (_hash & 0x7FFFFFFF));
}
