// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_UTILS_
#define ASTERIA_UTILS_

#include "fwd.hpp"
#include "../rocket/tinyfmt_str.hpp"
#include "../rocket/format.hpp"
namespace asteria {

// Formatting
template<typename... LiteralT>
constexpr
array<const char*, sizeof...(LiteralT)>
make_string_template(const LiteralT&... templs)
  {
    return { templs... };
  }

template<size_t N, typename... ParamsT>
ROCKET_NEVER_INLINE ROCKET_FLATTEN
cow_string&
format(cow_string& str, const array<const char*, N>& templs, const ParamsT&... params)
  {
    // Reuse the storage of `str` to create a formatter.
    ::rocket::tinyfmt_str fmt;
    str.clear();
    fmt.set_string(::std::move(str), ::rocket::tinybuf::open_write);

    if(N > 0)
      format(fmt, templs[0], params...);

    for(size_t k = 1;  k < N;  ++k)
      fmt << '\n',
        format(fmt, templs[k], params...);

    str = fmt.extract_string();
    return str;
  }

template<typename... ParamsT>
ROCKET_NEVER_INLINE ROCKET_FLATTEN
cow_string&
format(cow_string& str, const char* templ, const ParamsT&... params)
  {
    // Reuse the storage of `str` to create a formatter.
    ::rocket::tinyfmt_str fmt;
    str.clear();
    fmt.set_string(::std::move(str), ::rocket::tinybuf::open_write);

    format(fmt, templ, params...);

    str = fmt.extract_string();
    return str;
  }

template<size_t N, typename... ParamsT>
ROCKET_NEVER_INLINE ROCKET_FLATTEN
cow_string
format_string(const array<const char*, N>& templs, const ParamsT&... params)
  {
    cow_string str;
    format(str, templs, params...);
    return str;
  }

template<typename... ParamsT>
ROCKET_NEVER_INLINE ROCKET_FLATTEN
cow_string
format_string(const char* templ, const ParamsT&... params)
  {
    cow_string str;
    format(str, templ, params...);
    return str;
  }

// Error handling
// Note string templates must be parenthesized.
ptrdiff_t
write_log_to_stderr(const char* file, long line, const char* func, cow_string&& msg);

[[noreturn]]
void
throw_runtime_error(const char* file, long line, const char* func, cow_string&& msg);

#define ASTERIA_TERMINATE(TEMPLATE, ...)  \
    (::asteria::write_log_to_stderr(__FILE__, __LINE__, __FUNCTION__,  \
       ::asteria::format_string(  \
         (::asteria::make_string_template TEMPLATE), ##__VA_ARGS__)  \
       ),  \
     ::std::terminate())

#define ASTERIA_THROW(TEMPLATE, ...)  \
    (::asteria::throw_runtime_error(__FILE__, __LINE__, __FUNCTION__,  \
       ::asteria::format_string(  \
         (::asteria::make_string_template TEMPLATE), ##__VA_ARGS__)  \
       ),  \
     ROCKET_UNREACHABLE())

// Type conversion
template<typename enumT>
ROCKET_CONST constexpr
typename ::std::underlying_type<enumT>::type
weaken_enum(enumT value) noexcept
  {
    return static_cast<typename ::std::underlying_type<enumT>::type>(value);
  }

// Saturation subtraction
template<typename uintT,
ROCKET_ENABLE_IF(::std::is_unsigned<uintT>::value && !::std::is_same<uintT, bool>::value)>
constexpr
uintT
subsat(uintT x, uintT y) noexcept
  {
    return (x < y) ? 0 : (x - y);
  }

// C character types
enum : uint8_t
  {
    cmask_space   = 0x01,  // [ \t\v\f\r\n]
    cmask_alpha   = 0x02,  // [A-Za-z]
    cmask_digit   = 0x04,  // [0-9]
    cmask_xdigit  = 0x08,  // [0-9A-Fa-f]
    cmask_namei   = 0x10,  // [A-Za-z_]
    cmask_blank   = 0x20,  // [ \t]
    cmask_cntrl   = 0x40,  // [[:cntrl:]]
  };

constexpr uint8_t cmask_table[128] =
  {
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x21, 0x61, 0x41, 0x41, 0x41, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x00, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x40,
  };

constexpr
uint8_t
get_cmask(char ch) noexcept
  {
    return ((ch & 0x7F) == ch) ? cmask_table[(uint8_t) ch] : 0;
  }

constexpr
bool
is_cmask(char ch, uint8_t mask) noexcept
  {
    return noadl::get_cmask(ch) & mask;
  }

// Numeric conversion
constexpr
bool
is_convertible_to_int64(double val) noexcept
  {
    return (-0x1p63 <= val) && (val < 0x1p63);
  }

constexpr
bool
is_exact_int64(double val) noexcept
  {
    return noadl::is_convertible_to_int64(val) && ((double)(int64_t) val == val);
  }

inline
int64_t
safe_double_to_int64(double val)
  {
    if(!noadl::is_convertible_to_int64(val))
      ::rocket::sprintf_and_throw<::std::invalid_argument>(
            "safe_double_to_int64: value `%.17g` is out of range for an `int64`",
            val);

    int64_t ival = (int64_t) val;
    if((double) ival != val)
      ::rocket::sprintf_and_throw<::std::invalid_argument>(
            "safe_double_to_int64: value `%.17g` is not an exact integer",
            val);

    return ival;
  }

// Gets a random number from hardware.
inline
uint64_t
generate_random_seed() noexcept
  {
    int hw_ok = 0;
    uint64_t val = 0;
#if defined(__RDSEED__)
#  ifdef __x86_64__
    hw_ok = ::_rdseed64_step((unsigned long long*) &val);
#  else
    hw_ok = ::_rdseed32_step((unsigned int*) &val);
#  endif
#endif
    return ROCKET_EXPECT(hw_ok) ? val : ::__rdtsc();
  }

// Negative array index wrapper
struct Wrapped_Index
  {
    uint64_t nappend;   // number of elements to append
    uint64_t nprepend;  // number of elements to prepend

    size_t rindex;  // wrapped index, within range if and only if
                    // both `nappend` and `nprepend` are zeroes

    constexpr
    Wrapped_Index(ptrdiff_t ssize, int64_t sindex) noexcept
      :
        nappend(0), nprepend(0),
        rindex(0)
      {
        ROCKET_ASSERT(ssize >= 0);

        if(sindex >= 0) {
          ptrdiff_t last_sindex = ssize - 1;
          this->nappend = (uint64_t) (::std::max(last_sindex, sindex) - last_sindex);
          this->rindex = (size_t) sindex;
        }
        else {
          ptrdiff_t first_sindex = 0 - ssize;
          this->nprepend = (uint64_t) (first_sindex - ::std::min(sindex, first_sindex));
          this->rindex = (size_t) (sindex + ssize) + (size_t) this->nprepend;
        }
      }
  };

constexpr
Wrapped_Index
wrap_array_index(ptrdiff_t ssize, int64_t sindex) noexcept
  {
    return Wrapped_Index(ssize, sindex);
  }

// UTF-8 conversion functions
bool
utf8_encode(char*& pos, char32_t cp) noexcept;

bool
utf8_encode(cow_string& text, char32_t cp);

bool
utf8_decode(char32_t& cp, const char*& pos, size_t avail) noexcept;

bool
utf8_decode(char32_t& cp, stringR text, size_t& offset);

// UTF-16 conversion functions
bool
utf16_encode(char16_t*& pos, char32_t cp) noexcept;

bool
utf16_encode(cow_u16string& text, char32_t cp);

bool
utf16_decode(char32_t& cp, const char16_t*& pos, size_t avail) noexcept;

bool
utf16_decode(char32_t& cp, const cow_u16string& text, size_t& offset);

// C-style quoting
tinyfmt&
c_quote(tinyfmt& fmt, const char* data, size_t size);

cow_string&
c_quote(cow_string& str, const char* data, size_t size);

}  // namespace asteria
#endif
