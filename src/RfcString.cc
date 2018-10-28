/* RfcString.cc
 *
 */
#include <stdlib.h>
#include "RfcString.h"

int
RfcString::compare(const RfcString& str, size_t n) const noexcept
{
  if (rfc_casecmp != _rfc_casecmp)
    return String::compare(str, n);
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
    if ((cmp = _rfc_toupper(*s1) - _rfc_toupper(*s2)) != 0)
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
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

size_t
RfcString::hash() const noexcept {
  if (my_hash != 0) return my_hash;
  if (rfc_casecmp != _rfc_casecmp)
    return String::hash();
  std::hash<value_type> hasher;
  size_t _hash = 5381;

  for(size_t i = 0; i < this->length(); ++i)
    _hash = ((_hash << 5) + _hash) + hasher(_rfc_toupper(this->data()[i]));
  return (my_hash = (_hash & 0x7FFFFFFF));
}
