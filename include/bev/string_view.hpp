// An implementation of the `std::string_view` class template, with the addition
// of a new `is_cstring()` member function that can be used to avoid unnecessary
// copies in some situations.
//
// This implementation of `std::string_view` was originally written as part of
// the GNU ISO C++ Library. The original code is available under the GNU GPLv3
// with the GCC Runtime Library Exception at gcc.gnu.org/git/gcc.git
//
// Interface Changes:
//  * A new member function `basic_string_view::is_cstring()` that tells
//    whether `.data()` returns a null-terminated byte array.
//    Note that this function is conservative in the sense that it may return
//    `false` even if `.data()` would be null-terminated, for example in the
//    presence of embedded null bytes.
//  * A new constructor from `std::string` was added, since it is not possible
//    to replicate the original string_view interface of adding new user-defined
//    conversions to std::string.
//  * For the same reasons, the string_view literal operators have been renamed
//    to `_sv` and dont live in namespace `std` anymore.
// 
// Internal Changes:
//  * General reformatting required by moving the class out of the `std` namespace,
//    in particular removal of the underscore and double-underscore variable name
//    prefixes that are reserved for use by the standard library.
//  * Removal of all libstdc++ internal functions. This means that the class can
//    be compiled against any standard library. In particular this includes many
//    debug assertions and checks.
//  * Only supports 64-bit size_t (the main reason being that I only copied the
//    64-bit version of MurmurHash3, and I don't have a 32 bit system for
//    testing available)
//  * Removed support for char8_t (since my compiler doesnt have it yet)

#pragma once

#include <type_traits>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>

namespace bev {

  /**
   *  @brief  A non-owning reference to a string.
   *
   *  @tparam CharT   Type of character
   *  @tparam Traits  Traits for character type, defaults to
   *                  std::char_traits<CharT>.
   *
   *  A basic_string_view looks like this:
   *
   *  @code
   *    CharT*    str_
   *    size_t    len_
   *  @endcode
   */
template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_string_view
{
  static_assert(!std::is_array_v<CharT>);
  static_assert(std::is_trivial_v<CharT> && std::is_standard_layout_v<CharT>);
  static_assert(std::is_same_v<CharT, typename Traits::char_type>);

  // bitmask for accessing the `safe_nulltest` flag bit in `len_`.
  static const size_t safederef_flag_mask = 1ull << (sizeof(size_t)-1);
  static size_t set_safederef_bit(size_t x) { return x | safederef_flag_mask; }
  static size_t clear_safederef_bit(size_t x) { return x & ~safederef_flag_mask; }
  static bool test_safederef_bit(size_t x) { return x & safederef_flag_mask; }

public:

  // types
  using traits_type         = Traits;
  using value_type          = CharT;
  using pointer             = value_type*;
  using const_pointer       = const value_type*;
  using reference           = value_type&;
  using const_reference     = const value_type&;
  using const_iterator      = const value_type*;
  using iterator            = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator    = const_reverse_iterator;
  using size_type           = size_t;
  using difference_type     = ptrdiff_t;
  static constexpr size_type npos = size_type(-1);

  // [string.view.cons], construction and assignment

  constexpr
  basic_string_view() noexcept
    : len_{0}, str_{nullptr}
  { }

  constexpr basic_string_view(const basic_string_view&) noexcept = default;

  constexpr basic_string_view(const CharT* str) noexcept
    : len_{set_safederef_bit(traits_type::length(str))}
    , str_{str}
  { }

  constexpr
  basic_string_view(const CharT* str, size_type len) noexcept
    : len_{len}, str_{str}
  { }

  constexpr basic_string_view&
  operator=(const basic_string_view&) noexcept = default;

  // non-standard constructors
  basic_string_view(const std::basic_string<CharT, Traits>& s)
    : len_{set_safederef_bit(s.size())}, str_{s.data()}
  {}

  basic_string_view(std::basic_string<CharT, Traits>&&) = delete;

private:
  struct can_test_safederef {};

  basic_string_view(const CharT* str, size_type len, can_test_safederef)
    : len_(set_safederef_bit(len)), str_(str)
  {}

public:

  // non-standard interface
  bool is_cstring()
  {
    if (test_safederef_bit(len_))
      return str_[this->length()] != 0;
    return false;
  }

  // [string.view.iterators], iterator support

  constexpr const_iterator
  begin() const noexcept
  { return this->str_; }

  constexpr const_iterator
  end() const noexcept
  { return this->str_ + this->length(); }

  constexpr const_iterator
  cbegin() const noexcept
  { return this->str_; }

  constexpr const_iterator
  cend() const noexcept
  { return this->str_ + this->length(); }

  constexpr const_reverse_iterator
  rbegin() const noexcept
  { return const_reverse_iterator(this->end()); }

  constexpr const_reverse_iterator
  rend() const noexcept
  { return const_reverse_iterator(this->begin()); }

  constexpr const_reverse_iterator
  crbegin() const noexcept
  { return const_reverse_iterator(this->end()); }

  constexpr const_reverse_iterator
  crend() const noexcept
  { return const_reverse_iterator(this->begin()); }

  // [string.view.capacity], capacity

  constexpr size_type
  size() const noexcept
  { return this->length(); }

  constexpr size_type
  length() const noexcept
  { return clear_safederef_bit(len_); }

  constexpr size_type
  max_size() const noexcept
  {
    return (npos - sizeof(size_type) - sizeof(void*))
            / sizeof(value_type) / 4;
  }

  [[nodiscard]] constexpr bool
  empty() const noexcept
  { return length() == 0; }

  // [string.view.access], element access

  constexpr const_reference
  operator[](size_type pos) const noexcept
  {
    return *(this->str_ + pos);
  }

  constexpr const_reference
  at(size_type pos) const
  {
    if (pos >= this->length())
      throw std::out_of_range("basic_string_view::at");
    return *(this->str_ + pos);
  }

  constexpr const_reference
  front() const noexcept
  {
    return *this->str_;
  }

  constexpr const_reference
  back() const noexcept
  {
    return *(this->str_ + this->length() - 1);
  }

  constexpr const_pointer
  data() const noexcept
  { return this->str_; }

  // [string.view.modifiers], modifiers:

  constexpr void
  remove_prefix(size_type n) noexcept
  {
    this->str_ += n;
    this->length() -= n;
  }

  constexpr void
  remove_suffix(size_type n) noexcept
  { this->length() -= n; }

  constexpr void
  swap(basic_string_view& sv) noexcept
  {
    auto tmp = *this;
    *this = sv;
    sv = tmp;
  }

  // [string.view.ops], string operations:

  size_type
  copy(CharT* str, size_type n, size_type pos = 0) const
  {
    const size_type rlen = std::min(n, this->length() - pos);
    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 2777. basic_string_view::copy should use char_traits::copy
    traits_type::copy(str, data() + pos, rlen);
    return rlen;
  }

  constexpr basic_string_view
  substr(size_type pos = 0, size_type n = npos) const noexcept(false)
  {
    const size_type rlen = std::min(n, this->length() - pos);
    if (test_safederef_bit(len_))
      return basic_string_view{str_ + pos, rlen, can_test_safederef{}};
    return basic_string_view{str_ + pos, rlen};
  }

  constexpr int
  compare(basic_string_view str) const noexcept
  {
    const size_type rlen = std::min(length(), str.length());
    int ret = traits_type::compare(this->str_, str.str_, rlen);
    if (ret == 0)
      ret = s_compare(length(), str.length());
    return ret;
  }

  constexpr int
  compare(size_type pos1, size_type n1, basic_string_view str) const
  { return this->substr(pos1, n1).compare(str); }

  constexpr int
  compare(size_type pos1, size_type n1,
          basic_string_view str, size_type pos2, size_type n2) const
  {
    return this->substr(pos1, n1).compare(str.substr(pos2, n2));
  }

  constexpr int
  compare(const CharT* str) const noexcept
  { return this->compare(basic_string_view{str}); }

  constexpr int
  compare(size_type pos1, size_type n1, const CharT* str) const
  { return this->substr(pos1, n1).compare(basic_string_view{str}); }

  constexpr int
  compare(size_type pos1, size_type n1,
          const CharT* str, size_type n2) const noexcept(false)
  {
    return this->substr(pos1, n1)
               .compare(basic_string_view(str, n2));
  }

  constexpr bool
  starts_with(basic_string_view x) const noexcept
  { return this->substr(0, x.size()) == x; }

  constexpr bool
  starts_with(CharT x) const noexcept
  { return !this->empty() && traits_type::eq(this->front(), x); }

  constexpr bool
  starts_with(const CharT* x) const noexcept
  { return this->starts_with(basic_string_view(x)); }

  constexpr bool
  ends_with(basic_string_view x) const noexcept
  {
    return this->size() >= x.size()
        && this->compare(this->size() - x.size(), npos, x) == 0;
  }

  constexpr bool
  ends_with(CharT x) const noexcept
  { return !this->empty() && traits_type::eq(this->back(), x); }

  constexpr bool
  ends_with(const CharT* x) const noexcept
  { return this->ends_with(basic_string_view(x)); }

  // [string.view.find], searching

  constexpr size_type
  find(basic_string_view str, size_type pos = 0) const noexcept
  { return this->find(str.str_, pos, str.length()); }

  constexpr size_type
  find(CharT c, size_type pos = 0) const noexcept;

  constexpr size_type
  find(const CharT* str, size_type pos, size_type n) const noexcept;

  constexpr size_type
  find(const CharT* str, size_type pos = 0) const noexcept
  { return this->find(str, pos, traits_type::length(str)); }

  constexpr size_type
  rfind(basic_string_view str, size_type pos = npos) const noexcept
  { return this->rfind(str.str_, pos, str.length()); }

  constexpr size_type
  rfind(CharT c, size_type pos = npos) const noexcept;

  constexpr size_type
  rfind(const CharT* str, size_type pos, size_type n) const noexcept;

  constexpr size_type
  rfind(const CharT* str, size_type pos = npos) const noexcept
  { return this->rfind(str, pos, traits_type::length(str)); }

  constexpr size_type
  find_first_of(basic_string_view str, size_type pos = 0) const noexcept
  { return this->find_first_of(str.str_, pos, str.length()); }

  constexpr size_type
  find_first_of(CharT c, size_type pos = 0) const noexcept
  { return this->find(c, pos); }

  constexpr size_type
  find_first_of(const CharT* str, size_type pos,
                size_type n) const noexcept;

  constexpr size_type
  find_first_of(const CharT* str, size_type pos = 0) const noexcept
  { return this->find_first_of(str, pos, traits_type::length(str)); }

  constexpr size_type
  find_last_of(basic_string_view str,
               size_type pos = npos) const noexcept
  { return this->find_last_of(str.str_, pos, str.length()); }

  constexpr size_type
  find_last_of(CharT c, size_type pos=npos) const noexcept
  { return this->rfind(c, pos); }

  constexpr size_type
  find_last_of(const CharT* str, size_type pos,
               size_type n) const noexcept;

  constexpr size_type
  find_last_of(const CharT* str, size_type pos = npos) const noexcept
  { return this->find_last_of(str, pos, traits_type::length(str)); }

  constexpr size_type
  find_first_not_of(basic_string_view str,
                    size_type pos = 0) const noexcept
  { return this->find_first_not_of(str.str_, pos, str.length()); }

  constexpr size_type
  find_first_not_of(CharT c, size_type pos = 0) const noexcept;

  constexpr size_type
  find_first_not_of(const CharT* str,
                    size_type pos, size_type n) const noexcept;

  constexpr size_type
  find_first_not_of(const CharT* str, size_type pos = 0) const noexcept
  {
    return this->find_first_not_of(str, pos,
                                   traits_type::length(str));
  }

  constexpr size_type
  find_last_not_of(basic_string_view str,
                   size_type pos = npos) const noexcept
  { return this->find_last_not_of(str.str_, pos, str.length()); }

  constexpr size_type
  find_last_not_of(CharT c, size_type pos = npos) const noexcept;

  constexpr size_type
  find_last_not_of(const CharT* str,
                   size_type pos, size_type n) const noexcept;

  constexpr size_type
  find_last_not_of(const CharT* str,
                   size_type pos = npos) const noexcept
  {
    return this->find_last_not_of(str, pos,
                                  traits_type::length(str));
  }

private:

  // Return the difference between n1 and n2, clamped to the range of int.
  static constexpr int
  s_compare(size_type n1, size_type n2) noexcept
  {
    const difference_type diff = n1 - n2;
    if (diff > std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    if (diff < std::numeric_limits<int>::min())
      return std::numeric_limits<int>::min();
    return static_cast<int>(diff);
  }

  size_t       len_;
  const CharT* str_;
};

// [string.view.comparison], non-member basic_string_view comparison function

namespace detail {
// Identity transform to create a non-deduced context, so that only one
// argument participates in template argument deduction and the other
// argument gets implicitly converted to the deduced type. See n3766.html.
template<typename Tp>
using identity = std::common_type_t<Tp>;
} // namespace detail

template<typename CharT, typename Traits>
constexpr bool
operator==(basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.size() == y.size() && x.compare(y) == 0; }

template<typename CharT, typename Traits>
constexpr bool
operator==(basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return x.size() == y.size() && x.compare(y) == 0; }

template<typename CharT, typename Traits>
constexpr bool
operator==(detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.size() == y.size() && x.compare(y) == 0; }

template<typename CharT, typename Traits>
constexpr bool
operator!=(basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return !(x == y); }

template<typename CharT, typename Traits>
constexpr bool
operator!=(basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return !(x == y); }

template<typename CharT, typename Traits>
constexpr bool
operator!=(detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return !(x == y); }

template<typename CharT, typename Traits>
constexpr bool
operator< (basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) < 0; }

template<typename CharT, typename Traits>
constexpr bool
operator< (basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return x.compare(y) < 0; }

template<typename CharT, typename Traits>
constexpr bool
operator< (detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) < 0; }

template<typename CharT, typename Traits>
constexpr bool
operator> (basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) > 0; }

template<typename CharT, typename Traits>
constexpr bool
operator> (basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return x.compare(y) > 0; }

template<typename CharT, typename Traits>
constexpr bool
operator> (detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) > 0; }

template<typename CharT, typename Traits>
constexpr bool
operator<=(basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) <= 0; }

template<typename CharT, typename Traits>
constexpr bool
operator<=(basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return x.compare(y) <= 0; }

template<typename CharT, typename Traits>
constexpr bool
operator<=(detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) <= 0; }

template<typename CharT, typename Traits>
constexpr bool
operator>=(basic_string_view<CharT, Traits> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) >= 0; }

template<typename CharT, typename Traits>
constexpr bool
operator>=(basic_string_view<CharT, Traits> x,
           detail::identity<basic_string_view<CharT, Traits>> y) noexcept
{ return x.compare(y) >= 0; }

template<typename CharT, typename Traits>
constexpr bool
operator>=(detail::identity<basic_string_view<CharT, Traits>> x,
           basic_string_view<CharT, Traits> y) noexcept
{ return x.compare(y) >= 0; }

// [string.view.io], Inserters and extractors
template<typename CharT, typename Traits>
inline std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os,
           basic_string_view<CharT,Traits> str)
{
  auto len = str.length();
  for (size_t i=0; i<str.length(); ++i) {
    os << str.str_[i];
  }
  return os << CharT{0};
}

// basic_string_view typedef names
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

// string view literals

inline constexpr basic_string_view<char>
operator""_sv(const char* str, size_t len) noexcept
{ return basic_string_view<char>{str, len}; }

inline constexpr basic_string_view<wchar_t>
operator""_sv(const wchar_t* str, size_t len) noexcept
{ return basic_string_view<wchar_t>{str, len}; }

inline constexpr basic_string_view<char16_t>
operator""_sv(const char16_t* str, size_t len) noexcept
{ return basic_string_view<char16_t>{str, len}; }

inline constexpr basic_string_view<char32_t>
operator""_sv(const char32_t* str, size_t len) noexcept
{ return basic_string_view<char32_t>{str, len}; }


// Implementation

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::
find(const CharT* str, size_type pos, size_type n) const noexcept
{
  if (n == 0)
    return pos <= length() ? pos : npos;

  if (n <= length()) {
    for (; pos <= length() - n; ++pos)
      if (traits_type::eq(this->str_[pos], str[0])
          && traits_type::compare(this->str_ + pos + 1,
                                  str + 1, n - 1) == 0)
        return pos;
  }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::find(
  CharT c, size_type pos) const noexcept
{
  size_type ret = npos;
  if (pos < length())
    {
      const size_type n = length() - pos;
      const CharT* p = traits_type::find(this->str_ + pos, n, c);
      if (p)
        ret = p - this->str_;
    }
  return ret;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::
rfind(const CharT* str, size_type pos, size_type n) const noexcept
{
  if (n <= length()) {
    pos = std::min(size_type(length() - n), pos);
    do {
      if (traits_type::compare(this->str_ + pos, str, n) == 0)
        return pos;
    } while (pos-- > 0);
  }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::
rfind(CharT c, size_type pos) const noexcept
{
  size_type size = length();
  if (size > 0) {
    if (--size > pos)
      size = pos;
    for (++size; size-- > 0; )
      if (traits_type::eq(this->str_[size], c))
        return size;
  }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::
find_first_of(const CharT* str, size_type pos,
              size_type n) const noexcept
{
  for (; n && pos < length(); ++pos) {
    const CharT* p = traits_type::find(str, n,
                                       this->str_[pos]);
    if (p)
      return pos;
  }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::
find_last_of(const CharT* str, size_type pos,
             size_type n) const noexcept
{
  size_type size = this->size();
  if (size && n)
    {
      if (--size > pos)
        size = pos;
      do
        {
          if (traits_type::find(str, n, this->str_[size]))
            return size;
        }
      while (size-- != 0);
    }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::find_first_not_of(
  const CharT* str, size_type pos, size_type n) const noexcept
{
  for (; pos < length(); ++pos)
    if (!traits_type::find(str, n, this->str_[pos]))
      return pos;
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::find_first_not_of(
  CharT c, size_type pos) const noexcept
{
  for (; pos < length(); ++pos)
    if (!traits_type::eq(this->str_[pos], c))
      return pos;
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::find_last_not_of(
    const CharT* str,
    size_type pos,
    size_type n) const noexcept
{
  size_type size = length();
  if (size) {
      if (--size > pos)
        size = pos;
      do {
        if (!traits_type::find(str, n, this->str_[size]))
          return size;
      }
      while (size--);
  }
  return npos;
}

template<typename CharT, typename Traits>
constexpr typename basic_string_view<CharT, Traits>::size_type
basic_string_view<CharT, Traits>::find_last_not_of(CharT c, size_type pos) const noexcept
{
  size_type size = length();
  if (size) {
      if (--size > pos)
        size = pos;
      do {
          if (!traits_type::eq(this->str_[size], c))
            return size;
      } while (size--);
  }
  return npos;
}

// namespace for the implementation of the string_view hash
namespace detail {

inline std::size_t unaligned_load_(const char* p)
{
  std::size_t result;
  __builtin_memcpy(&result, p, sizeof(result));
  return result;
}

// Loads n bytes, where 1 <= n < 8.
inline std::size_t load_bytes_(const char* p, int n)
{
  std::size_t result = 0;
  --n;
  do
    result = (result << 8) + static_cast<unsigned char>(p[n]);
  while (--n >= 0);
  return result;
}

inline std::size_t shift_mix_(std::size_t v)
{ return v ^ (v >> 47);}

// Implementation of Murmur hash for 64-bit size_t.
size_t hash_bytes_(const void* ptr, size_t len, size_t seed)
{
  constexpr const size_t mul = (((size_t) 0xc6a4a793UL) << 32UL)
          + (size_t) 0x5bd1e995UL;
  const char* const buf = static_cast<const char*>(ptr);

  // Remove the bytes not divisible by the sizeof(size_t).  This
  // allows the main loop to process the data as 64-bit integers.
  const size_t len_aligned = len & ~(size_t)0x7;
  const char* const end = buf + len_aligned;
  size_t hash = seed ^ (len * mul);
  for (const char* p = buf; p != end; p += 8) {
      const size_t data = shift_mix_(unaligned_load_(p) * mul) * mul;
      hash ^= data;
      hash *= mul;
  }
  if ((len & 0x7) != 0) {
      const size_t data = load_bytes_(end, len & 0x7);
      hash ^= data;
      hash *= mul;
  }
  hash = shift_mix_(hash) * mul;
  hash = shift_mix_(hash);
  return hash;
}

} // namespace detail
} // namespace bev

namespace std  {

// [string.view.hash], hash support:

template<typename CharT>
struct hash<bev::basic_string_view<CharT>>
{
  size_t
  operator()(const bev::basic_string_view<CharT>& str) const noexcept
  { return bev::detail::hash_bytes_(str.str_, str.length()*sizeof(CharT)); }
};

} // namespace std
