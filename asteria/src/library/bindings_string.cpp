// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "bindings_string.hpp"
#include "argument_reader.hpp"
#include "simple_binding_wrapper.hpp"
#include "../utilities.hpp"
#include <bitset>

namespace Asteria {

    namespace {

    std::pair<D_string::const_iterator, D_string::const_iterator> do_slice(const D_string& text, D_string::const_iterator tbegin, const Opt<D_integer>& length)
      {
        if(!length || (*length >= text.end() - tbegin)) {
          // Get the subrange from `tbegin` to the end.
          return std::make_pair(tbegin, text.end());
        }
        if(*length <= 0) {
          // Return an empty range.
          return std::make_pair(tbegin, tbegin);
        }
        // Don't go past the end.
        return std::make_pair(tbegin, tbegin + static_cast<std::ptrdiff_t>(*length));
      }

    std::pair<D_string::const_iterator, D_string::const_iterator> do_slice(const D_string& text, const D_integer& from, const Opt<D_integer>& length)
      {
        auto slen = static_cast<std::int64_t>(text.size());
        if(from >= 0) {
          // Behave like `std::string::substr()` except that no exception is thrown when `from` is greater than `text.size()`.
          if(from >= slen) {
            return std::make_pair(text.end(), text.end());
          }
          return do_slice(text, text.begin() + static_cast<std::ptrdiff_t>(from), length);
        }
        // Wrap `from` from the end. Notice that `from + slen` will not overflow when `from` is negative and `slen` is not.
        auto rfrom = from + slen;
        if(rfrom >= 0) {
          // Get a subrange from the wrapped index.
          return do_slice(text, text.begin() + static_cast<std::ptrdiff_t>(rfrom), length);
        }
        // Get a subrange from the beginning of `text`, if the wrapped index is before the first byte.
        if(!length) {
          // Get the subrange from the beginning to the end.
          return std::make_pair(text.begin(), text.end());
        }
        if(*length <= 0) {
          // Return an empty range.
          return std::make_pair(text.begin(), text.begin());
        }
        // Get a subrange excluding the part before the beginning. Notice that `rfrom + *length` will not overflow when `rfrom` is negative and `*length` is not.
        return do_slice(text, text.begin(), rfrom + *length);
      }

    }

D_string std_string_slice(const D_string& text, const D_integer& from, const Opt<D_integer>& length)
  {
    auto range = do_slice(text, from, length);
    if((range.first == text.begin()) && (range.second == text.end())) {
      // Use reference counting as our advantage.
      return text;
    }
    return D_string(range.first, range.second);
  }

D_string std_string_replace_slice(const D_string& text, const D_integer& from, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, rocket::nullopt);
    // Replace the subrange.
    res.replace(range.first, range.second, replacement);
    return res;
  }

D_string std_string_replace_slice(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, length);
    // Replace the subrange.
    res.replace(range.first, range.second, replacement);
    return res;
  }

D_integer std_string_compare(const D_string& text1, const D_string& text2, const Opt<D_integer>& length)
  {
    if(!length || (*length >= PTRDIFF_MAX)) {
      // Compare the entire strings.
      return text1.compare(text2);
    }
    if(*length <= 0) {
      // There is nothing to compare.
      return 0;
    }
    // Compare two substrings.
    return text1.compare(0, static_cast<std::size_t>(*length), text2, 0, static_cast<std::size_t>(*length));
  }

D_boolean std_string_starts_with(const D_string& text, const D_string& prefix)
  {
    return text.starts_with(prefix);
  }

D_boolean std_string_ends_with(const D_string& text, const D_string& suffix)
  {
    return text.ends_with(suffix);
  }

    namespace {

    // https://en.wikipedia.org/wiki/Boyer-Moore-Horspool_algorithm
    template<typename IteratorT> Opt<IteratorT> do_find_opt(IteratorT tbegin, IteratorT tend, IteratorT pbegin, IteratorT pend)
      {
        auto plen = std::distance(pbegin, pend);
        if(plen <= 0) {
          // Return a match at the the beginning if the pattern is empty.
          return tbegin;
        }
        // Build a table according to the Bad Character Rule.
        std::array<std::ptrdiff_t, 0x100> bcr_table;
        for(std::size_t i = 0; i != 0x100; ++i) {
          bcr_table[i] = plen;
        }
        for(std::ptrdiff_t i = plen - 1; i != 0; --i) {
          bcr_table[pend[~i] & 0xFF] = i;
        }
        // Search for the pattern.
        auto tpos = tbegin;
        for(;;) {
          if(tend - tpos < plen) {
            return rocket::nullopt;
          }
          if(std::equal(pbegin, pend, tpos)) {
            break;
          }
          tpos += bcr_table[tpos[plen - 1] & 0xFF];
        }
        return rocket::move(tpos);
      }

    }

Opt<D_integer> std_string_find(const D_string& text, const D_string& pattern)
  {
    auto qit = do_find_opt(text.begin(), text.end(), pattern.begin(), pattern.end());
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find(const D_string& text, const D_integer& from, const D_string& pattern)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_opt(range.first, range.second, pattern.begin(), pattern.end());
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& pattern)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_opt(range.first, range.second, pattern.begin(), pattern.end());
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_rfind(const D_string& text, const D_string& pattern)
  {
    auto qit = do_find_opt(text.rbegin(), text.rend(), pattern.rbegin(), pattern.rend());
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - pattern.ssize();
  }

Opt<D_integer> std_string_rfind(const D_string& text, const D_integer& from, const D_string& pattern)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), pattern.rbegin(), pattern.rend());
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - pattern.ssize();
  }

Opt<D_integer> std_string_rfind(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& pattern)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), pattern.rbegin(), pattern.rend());
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - pattern.ssize();
  }

D_string std_string_find_and_replace(const D_string& text, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto qit = do_find_opt(res.begin(), res.end(), pattern.begin(), pattern.end());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(*qit, *qit + pattern.ssize(), replacement);
    return res;
  }

D_string std_string_find_and_replace(const D_string& text, const D_integer& from, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, rocket::nullopt);
    auto qit = do_find_opt(range.first, range.second, pattern.begin(), pattern.end());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(*qit, *qit + pattern.ssize(), replacement);
    return res;
  }

D_string std_string_find_and_replace(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, length);
    auto qit = do_find_opt(range.first, range.second, pattern.begin(), pattern.end());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(*qit, *qit + pattern.ssize(), replacement);
    return res;
  }

D_string std_string_rfind_and_replace(const D_string& text, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto qit = do_find_opt(res.rbegin(), res.rend(), pattern.rbegin(), pattern.rend());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(qit->base() - pattern.ssize(), qit->base(), replacement);
    return res;
  }

D_string std_string_rfind_and_replace(const D_string& text, const D_integer& from, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, rocket::nullopt);
    auto qit = do_find_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), pattern.rbegin(), pattern.rend());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(qit->base() - pattern.ssize(), qit->base(), replacement);
    return res;
  }

D_string std_string_rfind_and_replace(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& pattern, const D_string& replacement)
  {
    D_string res = text;
    auto range = do_slice(res, from, length);
    auto qit = do_find_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), pattern.rbegin(), pattern.rend());
    if(!qit) {
      // Make use of reference counting if no match has been found.
      return res;
    }
    // Replace the subrange.
    res.replace(qit->base() - pattern.ssize(), qit->base(), replacement);
    return res;
  }

    namespace {

    template<typename IteratorT> Opt<IteratorT> do_find_of_opt(IteratorT begin, IteratorT end, const D_string& set, bool match)
      {
        // Make a lookup table.
        std::bitset<256> table;
        for(auto it = set.begin(); it != set.end(); ++it) {
          table.set(*it & 0xFF);
        }
        // Search the range.
        for(auto it = rocket::move(begin); it != end; ++it) {
          if(table.test(*it & 0xFF) == match) {
            return rocket::move(it);
          }
        }
        return rocket::nullopt;
      }

    }

Opt<D_integer> std_string_find_any_of(const D_string& text, const D_string& accept)
  {
    auto qit = do_find_of_opt(text.begin(), text.end(), accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find_any_of(const D_string& text, const D_integer& from, const D_string& accept)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_of_opt(range.first, range.second, accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find_any_of(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& accept)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_of_opt(range.first, range.second, accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find_not_of(const D_string& text, const D_string& reject)
  {
    auto qit = do_find_of_opt(text.begin(), text.end(), reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find_not_of(const D_string& text, const D_integer& from, const D_string& reject)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_of_opt(range.first, range.second, reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_find_not_of(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& reject)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_of_opt(range.first, range.second, reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return *qit - text.begin();
  }

Opt<D_integer> std_string_rfind_any_of(const D_string& text, const D_string& accept)
  {
    auto qit = do_find_of_opt(text.rbegin(), text.rend(), accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

Opt<D_integer> std_string_rfind_any_of(const D_string& text, const D_integer& from, const D_string& accept)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_of_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

Opt<D_integer> std_string_rfind_any_of(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& accept)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_of_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), accept, true);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

Opt<D_integer> std_string_rfind_not_of(const D_string& text, const D_string& reject)
  {
    auto qit = do_find_of_opt(text.rbegin(), text.rend(), reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

Opt<D_integer> std_string_rfind_not_of(const D_string& text, const D_integer& from, const D_string& reject)
  {
    auto range = do_slice(text, from, rocket::nullopt);
    auto qit = do_find_of_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

Opt<D_integer> std_string_rfind_not_of(const D_string& text, const D_integer& from, const Opt<D_integer>& length, const D_string& reject)
  {
    auto range = do_slice(text, from, length);
    auto qit = do_find_of_opt(std::make_reverse_iterator(range.second), std::make_reverse_iterator(range.first), reject, false);
    if(!qit) {
      return rocket::nullopt;
    }
    return text.rend() - *qit - 1;
  }

D_string std_string_reverse(const D_string& text)
  {
    // This is an easy matter, isn't it?
    return D_string(text.rbegin(), text.rend());
  }

    namespace {

    inline D_string::shallow_type do_reject(const Opt<D_string>& reject)
      {
        return reject ? rocket::sref(*reject) : rocket::sref(" \t");
      }

    }

D_string std_string_trim(const D_string& text, const Opt<D_string>& reject)
  {
    auto rchars = do_reject(reject);
    if(rchars.length() == 0) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Get the index of the first byte to keep.
    auto start = text.find_first_not_of(rchars);
    if(start == D_string::npos) {
      // There is no byte to keep. Return an empty string.
      return rocket::clear;
    }
    // Get the index of the last byte to keep.
    auto end = text.find_last_not_of(rchars);
    if((start == 0) && (end == text.size() - 1)) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Return the remaining part of `text`.
    return text.substr(start, end + 1 - start);
  }

D_string std_string_ltrim(const D_string& text, const Opt<D_string>& reject)
  {
    auto rchars = do_reject(reject);
    if(rchars.length() == 0) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Get the index of the first byte to keep.
    auto start = text.find_first_not_of(rchars);
    if(start == D_string::npos) {
      // There is no byte to keep. Return an empty string.
      return rocket::clear;
    }
    if(start == 0) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Return the remaining part of `text`.
    return text.substr(start);
  }

D_string std_string_rtrim(const D_string& text, const Opt<D_string>& reject)
  {
    auto rchars = do_reject(reject);
    if(rchars.length() == 0) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Get the index of the last byte to keep.
    auto end = text.find_last_not_of(rchars);
    if(end == D_string::npos) {
      // There is no byte to keep. Return an empty string.
      return rocket::clear;
    }
    if(end == text.size() - 1) {
      // There is no byte to strip. Make use of reference counting.
      return text;
    }
    // Return the remaining part of `text`.
    return text.substr(0, end + 1);
  }

D_string std_string_to_upper(const D_string& text)
  {
    // Use reference counting as our advantage.
    D_string res = text;
    char* wptr = nullptr;
    // Translate each character.
    for(std::size_t i = 0; i < res.size(); ++i) {
      char ch = res[i];
      if((ch < 'a') || ('z' < ch)) {
        continue;
      }
      // Fork the string as needed.
      if(ROCKET_UNEXPECT(!wptr)) {
        wptr = res.mut_data();
      }
      wptr[i] = static_cast<char>(ch - 'a' + 'A');
    }
    return res;
  }

D_string std_string_to_lower(const D_string& text)
  {
    // Use reference counting as our advantage.
    D_string res = text;
    char* wptr = nullptr;
    // Translate each character.
    for(std::size_t i = 0; i < res.size(); ++i) {
      char ch = res[i];
      if((ch < 'A') || ('Z' < ch)) {
        continue;
      }
      // Fork the string as needed.
      if(ROCKET_UNEXPECT(!wptr)) {
        wptr = res.mut_data();
      }
      wptr[i] = static_cast<char>(ch - 'A' + 'a');
    }
    return res;
  }

D_string std_string_translate(const D_string& text, const D_string& inputs, const Opt<D_string>& outputs)
  {
    // Use reference counting as our advantage.
    D_string res = text;
    char* wptr = nullptr;
    // Translate each character.
    for(std::size_t i = 0; i < res.size(); ++i) {
      char ch = res[i];
      auto ipos = inputs.find(ch);
      if(ipos == D_string::npos) {
        continue;
      }
      // Fork the string as needed.
      if(ROCKET_UNEXPECT(!wptr)) {
        wptr = res.mut_data();
      }
      if(!outputs || (ipos >= outputs->size())) {
        // Erase the byte if there is no replacement.
        // N.B. This must cause no reallocation.
        res.erase(i--, 1);
        continue;
      }
      wptr[i] = outputs->data()[ipos];
    }
    return res;
  }

D_array std_string_explode(const D_string& text, const Opt<D_string>& delim, const Opt<D_integer>& limit)
  {
    if(limit && (*limit <= 0)) {
      ASTERIA_THROW_RUNTIME_ERROR("The limit of number of segments must be greater than zero (got `", *limit, "`).");
    }
    D_array segments;
    if(text.empty()) {
      // Return an empty array.
      return segments;
    }
    if(!delim || delim->empty()) {
      // Split every byte.
      segments.reserve(text.size());
      rocket::for_each(text, [&](char ch) { segments.emplace_back(D_string(1, ch));  });
      return segments;
    }
    // Break `text` down.
    std::size_t bpos = 0;
    std::size_t epos;
    for(;;) {
      if(limit && (segments.size() >= static_cast<std::uint64_t>(*limit - 1))) {
        segments.emplace_back(text.substr(bpos));
        break;
      }
      epos = text.find(*delim, bpos);
      if(epos == Cow_String::npos) {
        segments.emplace_back(text.substr(bpos));
        break;
      }
      segments.emplace_back(text.substr(bpos, epos - bpos));
      bpos = epos + delim->size();
    }
    return segments;
  }

D_string std_string_implode(const D_array& segments, const Opt<D_string>& delim)
  {
    D_string text;
    // Deal with nasty separators.
    auto rpos = segments.begin();
    if(rpos != segments.end()) {
      for(;;) {
        text += rpos->check<D_string>();
        if(++rpos == segments.end()) {
          break;
        }
        if(!delim) {
          continue;
        }
        text += *delim;
      }
    }
    return text;
  }

D_string std_string_hex_encode(const D_string& text, const Opt<D_string>& delim, const Opt<D_boolean>& uppercase)
  {
    D_string hstr;
    auto rpos = text.begin();
    if(rpos == text.end()) {
      // Return an empty string; no delimiter is added.
      return hstr;
    }
    std::size_t ndcs = delim ? delim->size() : 0;
    bool upc = uppercase.value_or(false);
    // Reserve storage for digits.
    hstr.reserve(2 + (ndcs + 2) * (text.size() - 1));
    for(;;) {
      // Encode a byte.
      static constexpr char s_digits[] = "00112233445566778899aAbBcCdDeEfF";
      hstr += s_digits[(*rpos & 0xF0) / 8 + upc];
      hstr += s_digits[(*rpos & 0x0F) * 2 + upc];
      if(++rpos == text.end()) {
        break;
      }
      if(!delim) {
        continue;
      }
      hstr += *delim;
    }
    return hstr;
  }

Opt<D_string> std_string_hex_decode(const D_string& hstr)
  {
    D_string text;
    // Remember the value of a previous digit. `-1` means no such digit exists.
    int dprev = -1;
    for(char ch : hstr) {
      // Identify this character.
      static constexpr char s_table[] = "00112233445566778899aAbBcCdDeEfF \f\n\r\t\v";
      auto pos = std::char_traits<char>::find(s_table, sizeof(s_table) - 1, ch);
      if(!pos) {
        // Fail due to an invalid character.
        return rocket::nullopt;
      }
      auto dcur = static_cast<int>(pos - s_table) / 2;
      if(dcur >= 16) {
        // Ignore space characters.
        // But if we have had a digit, flush it.
        if(dprev != -1) {
          text.push_back(static_cast<char>(dprev));
          dprev = -1;
        }
        continue;
      }
      if(dprev == -1) {
        // Save this digit.
        dprev = dcur;
        continue;
      }
      // We have got two digits now.
      // Make a byte and write it.
      text.push_back(static_cast<char>(dprev * 16 + dcur));
      dprev = -1;
    }
    // If we have had a digit, flush it.
    if(dprev != -1) {
      text.push_back(static_cast<char>(dprev));
      dprev = -1;
    }
    return rocket::move(text);
  }

    namespace {

    bool do_utf8_encode_one(D_string& text, const D_integer& code_point, const Opt<D_boolean>& permissive)
      {
        auto value = code_point;
        if(((0xD800 <= value) && (value < 0xE000)) || (0x110000 <= value)) {
          // Code point value is reserved or too large.
          if(permissive != true) {
            return false;
          }
          // Replace it with the replacement character.
          value = 0xFFFD;
        }
        char32_t cpnt = value & 0x1FFFFF;
        // Encode it.
        auto encode_one = [&](unsigned shift, unsigned mask)
          {
            text.push_back(static_cast<char>((~mask << 1) | ((cpnt >> shift) & mask)));
          };
        if(cpnt < 0x80) {
          encode_one( 0, 0xFF);
          return true;
        }
        if(cpnt < 0x800) {
          encode_one( 6, 0x1F);
          encode_one( 0, 0x3F);
          return true;
        }
        if(cpnt < 0x10000) {
          encode_one(12, 0x0F);
          encode_one( 6, 0x3F);
          encode_one( 0, 0x3F);
          return true;
        }
        encode_one(18, 0x07);
        encode_one(12, 0x3F);
        encode_one( 6, 0x3F);
        encode_one( 0, 0x3F);
        return true;
      }

    }

Opt<D_string> std_string_utf8_encode(const D_integer& code_point, const Opt<D_boolean>& permissive)
  {
    D_string text;
    text.reserve(4);
    if(!do_utf8_encode_one(text, code_point, permissive)) {
      return rocket::nullopt;
    }
    return rocket::move(text);
  }

Opt<D_string> std_string_utf8_encode(const D_array& code_points, const Opt<D_boolean>& permissive)
  {
    D_string text;
    text.reserve(code_points.size() * 3);
    for(const auto& elem : code_points) {
      if(!do_utf8_encode_one(text, elem.check<D_integer>(), permissive)) {
        return rocket::nullopt;
      }
    }
    return rocket::move(text);
  }

Opt<D_array> std_string_utf8_decode(const D_string& text, const Opt<D_boolean>& permissive)
  {
    D_array code_points;
    code_points.reserve(text.size());
    for(std::size_t i = 0; i < text.size(); ++i) {
      // Read the first byte.
      char32_t cpnt = text[i] & 0xFF;
      if(cpnt < 0x80) {
        // This sequence contains only one byte.
        code_points.emplace_back(D_integer(cpnt));
        continue;
      }
      if((cpnt < 0xC0) || (0xF8 <= cpnt)) {
        // This is not a leading character.
        if(permissive != true) {
          return rocket::nullopt;
        }
        // Re-interpret it as an isolated byte.
        code_points.emplace_back(D_integer(cpnt));
        continue;
      }
      // Calculate the number of bytes in this code point.
      auto u8len = static_cast<std::size_t>(2 + (cpnt >= 0xE0) + (cpnt >= 0xF0));
      ROCKET_ASSERT(u8len >= 2);
      ROCKET_ASSERT(u8len <= 4);
      if(u8len > text.size() - i) {
        // No enough characters have been provided.
        if(permissive != true) {
          return rocket::nullopt;
        }
        // Re-interpret it as an isolated byte.
        code_points.emplace_back(D_integer(cpnt));
        continue;
      }
      // Unset bits that are not part of the payload.
      cpnt &= UINT32_C(0xFF) >> u8len;
      // Accumulate trailing code units.
      std::size_t k;
      for(k = 1; k < u8len; ++k) {
        char32_t next = text[++i] & 0xFF;
        if((next < 0x80) || (0xC0 <= next)) {
          // This trailing character is not valid.
          break;
        }
        cpnt = (cpnt << 6) | (next & 0x3F);
      }
      if(k != u8len) {
        // An error has been encountered when parsing trailing characters.
        if(permissive != true) {
          return rocket::nullopt;
        }
        // Replace this character.
        code_points.emplace_back(D_integer(0xFFFD));
        continue;
      }
      if(((0xD800 <= cpnt) && (cpnt < 0xE000)) || (0x110000 <= cpnt)) {
        // Code point value is reserved or too large.
        if(permissive != true) {
          return rocket::nullopt;
        }
        // Replace this character.
        code_points.emplace_back(D_integer(0xFFFD));
        continue;
      }
      // Re-encode it and check for overlong sequences.
      auto mlen = static_cast<std::size_t>(1 + (cpnt >= 0x80) + (cpnt >= 0x800) + (cpnt >= 0x10000));
      if(mlen != u8len) {
        // Overlong sequences are not allowed.
        if(permissive != true) {
          return rocket::nullopt;
        }
        // Replace this character.
        code_points.emplace_back(D_integer(0xFFFD));
        continue;
      }
      code_points.emplace_back(D_integer(cpnt));
    }
    return rocket::move(code_points);
  }

    namespace {

    template<typename IntegerT, bool bigendT> bool do_pack_one(D_string& text, const D_integer& value)
      {
        // Define temporary storage.
        std::array<char, sizeof(IntegerT)> stor_le;
        std::uint64_t word = 0;
        // Read an integer.
        word = static_cast<std::uint64_t>(value);
        // Write it in little-endian order.
        for(auto& byte : stor_le) {
          byte = static_cast<char>(word);
          word >>= 8;
        }
        // Append this word.
        if(bigendT) {
          text.append(stor_le.rbegin(), stor_le.rend());
        } else {
          text.append(stor_le.begin(), stor_le.end());
        }
        return true;
      }

    template<typename IntegerT, bool bigendT> D_array do_unpack(const D_string& text)
      {
        D_array values;
        // Define temporary storage.
        std::array<char, sizeof(IntegerT)> stor_be;
        std::uint64_t word = 0;
        // How many words will the result have?
        auto nwords = text.size() / stor_be.size();
        if(text.size() != nwords * stor_be.size()) {
          ASTERIA_THROW_RUNTIME_ERROR("The length of the source string must be a multiple of `", stor_be.size(), "` (got `", text.size(), "`).");
        }
        values.reserve(nwords);
        // Unpack integers.
        for(std::size_t i = 0; i < nwords; ++i) {
          // Read some bytes in big-endian order.
          if(bigendT) {
            std::copy_n(text.data() + i * stor_be.size(), stor_be.size(), stor_be.begin());
          } else {
            std::copy_n(text.data() + i * stor_be.size(), stor_be.size(), stor_be.rbegin());
          }
          // Assembly the word.
          for(const auto& byte : stor_be) {
            word <<= 8;
            word |= static_cast<unsigned char>(byte);
          }
          // Append the word.
          values.emplace_back(D_integer(static_cast<IntegerT>(word)));
        }
        return values;
      }

    }

D_string std_string_pack8(const D_integer& value)
  {
    D_string text;
    text.reserve(1);
    do_pack_one<std::int8_t, false>(text, value);
    return text;
  }

D_string std_string_pack8(const D_array& values)
  {
    D_string text;
    text.reserve(values.size());
    for(const auto& elem : values) {
      do_pack_one<std::int8_t, false>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack8(const D_string& text)
  {
    return do_unpack<std::int8_t, false>(text);
  }

D_string std_string_pack16be(const D_integer& value)
  {
    D_string text;
    text.reserve(2);
    do_pack_one<std::int16_t, true>(text, value);
    return text;
  }

D_string std_string_pack16be(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 2);
    for(const auto& elem : values) {
      do_pack_one<std::int16_t, true>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack16be(const D_string& text)
  {
    return do_unpack<std::int16_t, true>(text);
  }

D_string std_string_pack16le(const D_integer& value)
  {
    D_string text;
    text.reserve(2);
    do_pack_one<std::int16_t, false>(text, value);
    return text;
  }

D_string std_string_pack16le(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 2);
    for(const auto& elem : values) {
      do_pack_one<std::int16_t, false>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack16le(const D_string& text)
  {
    return do_unpack<std::int16_t, false>(text);
  }

D_string std_string_pack32be(const D_integer& value)
  {
    D_string text;
    text.reserve(4);
    do_pack_one<std::int32_t, true>(text, value);
    return text;
  }

D_string std_string_pack32be(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 4);
    for(const auto& elem : values) {
      do_pack_one<std::int32_t, true>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack32be(const D_string& text)
  {
    return do_unpack<std::int32_t, true>(text);
  }

D_string std_string_pack32le(const D_integer& value)
  {
    D_string text;
    text.reserve(4);
    do_pack_one<std::int32_t, false>(text, value);
    return text;
  }

D_string std_string_pack32le(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 4);
    for(const auto& elem : values) {
      do_pack_one<std::int32_t, false>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack32le(const D_string& text)
  {
    return do_unpack<std::int32_t, false>(text);
  }

D_string std_string_pack64be(const D_integer& value)
  {
    D_string text;
    text.reserve(8);
    do_pack_one<std::int64_t, true>(text, value);
    return text;
  }

D_string std_string_pack64be(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 8);
    for(const auto& elem : values) {
      do_pack_one<std::int64_t, true>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack64be(const D_string& text)
  {
    return do_unpack<std::int64_t, true>(text);
  }

D_string std_string_pack64le(const D_integer& value)
  {
    D_string text;
    text.reserve(8);
    do_pack_one<std::int64_t, false>(text, value);
    return text;
  }

D_string std_string_pack64le(const D_array& values)
  {
    D_string text;
    text.reserve(values.size() * 8);
    for(const auto& elem : values) {
      do_pack_one<std::int64_t, false>(text, elem.check<D_integer>());
    }
    return text;
  }

D_array std_string_unpack64le(const D_string& text)
  {
    return do_unpack<std::int64_t, false>(text);
  }

void create_bindings_string(D_object& result, API_Version /*version*/)
  {
    //===================================================================
    // `std.string.slice()`
    //===================================================================
    result.insert_or_assign(rocket::sref("slice"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.slice(text, from, [length])`\n"
                     "  * Copies a subrange of `text` to create a new byte string. Bytes\n"
                     "    are copied from `from` if it is non-negative, and from\n"
                     "    `lengthof(text) + from` otherwise. If `length` is set to an\n"
                     "    `integer`, no more than this number of bytes will be copied. If\n"
                     "    it is absent, all bytes from `from` to the end of `text` will\n"
                     "    be copied. If `from` is outside `text`, an empty `string` is\n"
                     "    returned.\n"
                     "  * Returns the specified substring of `text`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.slice"), args);
            // Parse arguments.
            D_string text;
            D_integer from;
            Opt<D_integer> length;
            if(reader.start().g(text).g(from).g(length).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_slice(text, from, length) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.replace_slice()`
    //===================================================================
    result.insert_or_assign(rocket::sref("replace_slice"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.replace_slice(text, from, replacement)`\n"
                     "  * Replaces all bytes from `from` to the end of `text` with\n"
                     "    `replacement` and returns the new byte string. If `from` is\n"
                     "    negative, it specifies an offset from the end of `text`. This\n"
                     "    function returns a new `string` without modifying `text`.\n"
                     "  * Returns a `string` with the subrange replaced.\n"
                     "`std.string.replace_slice(text, from, [length], replacement)`\n"
                     "  * Replaces a subrange of `text` with `replacement` to create a\n"
                     "    new byte string. `from` specifies the start of the subrange to\n"
                     "    replace. If `from` is negative, it specifies an offset from the\n"
                     "    end of `text`. `length` specifies the maximum number of bytes\n"
                     "    to replace. If it is set to `null`, this function is equivalent\n"
                     "    to `replace_slice(text, from, replacement)`. This function\n"
                     "    returns a new `string` without modifying `text`.\n"
                     "  * Returns a `string` with the subrange replaced.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.replace"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_integer from;
            D_string replacement;
            if(reader.start().g(text).g(from).save_state(state).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_replace_slice(text, from, replacement) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_replace_slice(text, from, length, replacement) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.find()`
    //===================================================================
    result.insert_or_assign(rocket::sref("find"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.find(text, pattern)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`.\n"
                     "  * Returns the subscript of the first byte of the first match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"
                     "`std.string.find(text, from, pattern)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the first byte of the first match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"
                     "`std.string.find(text, from, [length], pattern)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from, length)`.\n"
                     "  * Returns the subscript of the first byte of the first match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.find"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string pattern;
            if(reader.start().g(text).save_state(state).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_find(text, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_find(text, from, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_find(text, from, length, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.rfind()`
    //===================================================================
    result.insert_or_assign(rocket::sref("rfind"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.rfind(text, pattern)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`.\n"
                     "  * Returns the subscript of the first byte of the last match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"
                     "`std.string.rfind(text, from, pattern)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the first byte of the last match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"
                     "`std.string.rfind(text, from, [length], pattern)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`.\n"
                     "  * Returns the subscript of the first byte of the last match of\n"
                     "    `pattern` in `text` if one is found, which is always\n"
                     "    non-negative; otherwise `null`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.rfind"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string pattern;
            if(reader.start().g(text).save_state(state).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind(text, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind(text, from, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(pattern).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind(text, from, length, pattern);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.find_and_replace()`
    //===================================================================
    result.insert_or_assign(rocket::sref("find_and_replace"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.find_and_replace(text, pattern, replacement)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. This\n"
                     "    function returns a new `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"
                     "`std.string.find_and_replace(text, from, pattern, replacement)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from)`. This function returns a new\n"
                     "    `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"
                     "`std.string.find_and_replace(text, from, [length], pattern, replacement)`\n"
                     "  * Searches `text` for the first occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from, length)`. This function returns\n"
                     "    a new `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.find_and_replace"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string pattern;
            D_string replacement;
            if(reader.start().g(text).save_state(state).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_find_and_replace(text, pattern, replacement) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_find_and_replace(text, from, pattern, replacement) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_find_and_replace(text, from, length, pattern, replacement) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.rfind_and_replace()`
    //===================================================================
    result.insert_or_assign(rocket::sref("rfind_and_replace"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.rfind_and_replace(text, pattern, replacement)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. This\n"
                     "    function returns a new `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"
                     "`std.string.rfind_and_replace(text, from, pattern, replacement)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from)`. This function returns a new\n"
                     "    `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"
                     "`std.string.rfind_and_replace(text, from, [length], pattern, replacement)`\n"
                     "  * Searches `text` for the last occurrence of `pattern`. If a\n"
                     "    match is found, it is replaced with `replacement`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from, length)`. This function returns\n"
                     "    a new `string` without modifying `text`.\n"
                     "  * Returns the string with `pattern` replaced. If `text` does not\n"
                     "    contain `pattern`, it is returned intact.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.rfind_and_replace"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string pattern;
            D_string replacement;
            if(reader.start().g(text).save_state(state).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_rfind_and_replace(text, pattern, replacement) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_rfind_and_replace(text, from, pattern, replacement) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(pattern).g(replacement).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_rfind_and_replace(text, from, length, pattern, replacement) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.find_any_of()`
    //===================================================================
    result.insert_or_assign(rocket::sref("find_any_of"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.find_any_of(text, accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.find_any_of(text, from, accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.find_any_of(text, from, [length], accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from, length)`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.find_any_of"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string accept;
            if(reader.start().g(text).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_any_of(text, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_any_of(text, from, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_any_of(text, from, length, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.rfind_any_of()`
    //===================================================================
    result.insert_or_assign(rocket::sref("rfind_any_of"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.rfind_any_of(text, accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.rfind_any_of(text, from, accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.rfind_any_of(text, from, [length], accept)`\n"
                     "  * Searches `text` for bytes that exist in `accept`. The search\n"
                     "    operation is performed on the same subrange that would be\n"
                     "    returned by `slice(text, from, length)`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.rfind_any_of"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string accept;
            if(reader.start().g(text).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_any_of(text, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_any_of(text, from, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_any_of(text, from, length, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.find_not_of()`
    //===================================================================
    result.insert_or_assign(rocket::sref("find_not_of"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.find_not_of(text, reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.find_not_of(text, from, reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.find_not_of(text, from, [length], reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from, length)`.\n"
                     "  * Returns the subscript of the first byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.find_not_of"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string accept;
            if(reader.start().g(text).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_not_of(text, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_not_of(text, from, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_find_not_of(text, from, length, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.rfind_not_of()`
    //===================================================================
    result.insert_or_assign(rocket::sref("rfind_not_of"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.rfind_not_of(text, reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.rfind_not_of(text, from, reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from)`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"
                     "`std.string.rfind_not_of(text, from, [length], reject)`\n"
                     "  * Searches `text` for bytes that does not exist in `reject`. The\n"
                     "    search operation is performed on the same subrange that would\n"
                     "    be returned by `slice(text, from, length)`.\n"
                     "  * Returns the subscript of the last byte found, which is always\n"
                     "    non-negative; or `null` if no such byte exists.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.rfind_not_of"), args);
            Argument_Reader::State state;
            // Parse arguments.
            D_string text;
            D_string accept;
            if(reader.start().g(text).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_not_of(text, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            D_integer from;
            if(reader.load_state(state).g(from).save_state(state).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_not_of(text, from, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            Opt<D_integer> length;
            if(reader.load_state(state).g(length).g(accept).finish()) {
              // Call the binding function.
              auto qindex = std_string_rfind_not_of(text, from, length, accept);
              if(!qindex) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qindex) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.compare()`
    //===================================================================
    result.insert_or_assign(rocket::sref("compare"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.compare(text1, text2, [length])`\n"
                     "  * Performs lexicographical comparison on two byte strings. If\n"
                     "    `length` is set to an `integer`, no more than this number of\n"
                     "    bytes are compared. This function behaves like the `strncmp()`\n"
                     "    function in C, except that null characters do not terminate\n"
                     "    strings.\n"
                     "  * Returns a positive `integer` if `text1` compares greater than\n"
                     "    `text2`, a negative `integer` if `text1` compares less than\n"
                     "    `text2`, or zero if `text1` compares equal to `text2`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.compare"), args);
            // Parse arguments.
            D_string text1;
            D_string text2;
            Opt<D_integer> length;
            if(reader.start().g(text1).g(text2).g(length).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_compare(text1, text2, length) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.starts_with()`
    //===================================================================
    result.insert_or_assign(rocket::sref("starts_with"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.starts_with(text, prefix)`\n"
                     "  * Checks whether `prefix` is a prefix of `text`. The empty\n"
                     "    `string` is considered to be a prefix of any string.\n"
                     "  * Returns `true` if `prefix` is a prefix of `text`; otherwise\n"
                     "    `false`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.starts_with"), args);
            // Parse arguments.
            D_string text;
            D_string prefix;
            if(reader.start().g(text).g(prefix).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_starts_with(text, prefix) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.ends_with()`
    //===================================================================
    result.insert_or_assign(rocket::sref("ends_with"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.ends_with(text, suffix)`\n"
                     "  * Checks whether `suffix` is a suffix of `text`. The empty\n"
                     "    `string` is considered to be a suffix of any string.\n"
                     "  * Returns `true` if `suffix` is a suffix of `text`; otherwise\n"
                     "    `false`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.ends_with"), args);
            // Parse arguments.
            D_string text;
            D_string suffix;
            if(reader.start().g(text).g(suffix).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_ends_with(text, suffix) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.reverse()`
    //===================================================================
    result.insert_or_assign(rocket::sref("reverse"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.reverse(text)`\n"
                     "  * Reverses a byte `string`. This function returns a new `string`\n"
                     "    without modifying `text`.\n"
                     "  * Returns the reversed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.reverse"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_reverse(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.trim()`
    //===================================================================
    result.insert_or_assign(rocket::sref("trim"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.trim(text, [reject])`\n"
                     "  * Removes the longest prefix and suffix consisting solely bytes\n"
                     "    from `reject`. If `reject` is empty, no byte is removed. If\n"
                     "    `reject` is not specified, spaces and tabs are removed. This\n"
                     "    function returns a new `string` without modifying `text`.\n"
                     "  * Returns the trimmed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.trim"), args);
            // Parse arguments.
            D_string text;
            Opt<D_string> reject;
            if(reader.start().g(text).g(reject).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_trim(text, reject) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.ltrim()`
    //===================================================================
    result.insert_or_assign(rocket::sref("ltrim"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.ltrim(text, [reject])`\n"
                     "  * Removes the longest prefix consisting solely bytes from\n"
                     "    `reject`. If `reject` is empty, no byte is removed. If `reject`\n"
                     "    is not specified, spaces and tabs are removed. This function\n"
                     "    returns a new `string` without modifying `text`.\n"
                     "  * Returns the trimmed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.ltrim"), args);
            // Parse arguments.
            D_string text;
            Opt<D_string> reject;
            if(reader.start().g(text).g(reject).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_ltrim(text, reject) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.rtrim()`
    //===================================================================
    result.insert_or_assign(rocket::sref("rtrim"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.rtrim(text, [reject])`\n"
                     "  * Removes the longest suffix consisting solely bytes from\n"
                     "    `reject`. If `reject` is empty, no byte is removed. If `reject`\n"
                     "    is not specified, spaces and tabs are removed. This function\n"
                     "    returns a new `string` without modifying `text`.\n"
                     "  * Returns the trimmed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.rtrim"), args);
            // Parse arguments.
            D_string text;
            Opt<D_string> reject;
            if(reader.start().g(text).g(reject).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_rtrim(text, reject) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.to_upper()`
    //===================================================================
    result.insert_or_assign(rocket::sref("to_upper"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.to_upper(text)`\n"
                     "  * Converts all lowercase English letters in `text` to their\n"
                     "    uppercase counterparts. This function returns a new `string`\n"
                     "    without modifying `text`.\n"
                     "  * Returns a new `string` after the conversion.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.to_upper"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_to_upper(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.to_lower()`
    //===================================================================
    result.insert_or_assign(rocket::sref("to_lower"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.to_lower(text)`\n"
                     "  * Converts all lowercase English letters in `text` to their\n"
                     "    uppercase counterparts. This function returns a new `string`\n"
                     "    without modifying `text`.\n"
                     "  * Returns a new `string` after the conversion.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.to_lower"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_to_lower(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.translate()`
    //===================================================================
    result.insert_or_assign(rocket::sref("translate"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("``std.string.translate(text, inputs, [outputs])`\n"
                     "  * Performs bytewise translation on the given string. For every\n"
                     "    byte in `text` that is also found in `inputs`, if there is a\n"
                     "    corresponding replacement byte in `outputs` with the same\n"
                     "    subscript, it is replaced with the latter; if no replacement\n"
                     "    exists, because `outputs` is shorter than `inputs` or is null,\n"
                     "    it is deleted. If `outputs` is longer than `inputs`, excess\n"
                     "    bytes are ignored. Bytes that do not exist in `inputs` are left\n"
                     "    intact. This function returns a new `string` without modifying\n"
                     "    `text`.\n"
                     "  * Returns the translated `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.translate"), args);
            // Parse arguments.
            D_string text;
            D_string inputs;
            Opt<D_string> outputs;
            if(reader.start().g(text).g(inputs).g(outputs).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_translate(text, inputs, outputs) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.explode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("explode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.explode(text, [delim], [limit])`\n"
                     "  * Breaks `text` down into segments, separated by `delim`. If\n"
                     "    `delim` is `null` or an empty `string`, every byte becomes a\n"
                     "    segment. If `limit` is set to a positive `integer`, there will\n"
                     "    be no more segments than this number; the last segment will\n"
                     "    contain all the remaining bytes of the `text`.\n"
                     "  * Returns an `array` containing the broken-down segments. If\n"
                     "    `text` is empty, an empty `array` is returned.\n"
                     "  * Throws an exception if `limit` is zero or negative.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.explode"), args);
            // Parse arguments.
            D_string text;
            Opt<D_string> delim;
            Opt<D_integer> limit;
            if(reader.start().g(text).g(delim).g(limit).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_explode(text, delim, limit) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.implode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("implode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.implode(segments, [delim])`\n"
                     "  * Concatenates elements of an array, `segments`, to create a new\n"
                     "    `string`. All segments shall be `string`s. If `delim` is\n"
                     "    specified, it is inserted between adjacent segments.\n"
                     "  * Returns a `string` containing all segments. If `segments` is\n"
                     "    empty, an empty `string` is returned.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.implode"), args);
            // Parse arguments.
            D_array segments;
            Opt<D_string> delim;
            if(reader.start().g(segments).g(delim).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_implode(segments, delim) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.hex_encode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("hex_encode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.hex_encode(text, [delim], [uppercase])`\n"
                     "  * Encodes all bytes in `text` as 2-digit hexadecimal numbers and\n"
                     "    concatenates them. If `delim` is specified, it is inserted\n"
                     "    between adjacent bytes. If `uppercase` is set to `true`,\n"
                     "    hexadecimal digits above `9` are encoded as `ABCDEF`; otherwise\n"
                     "    they are encoded as `abcdef`.\n"
                     "  * Returns the encoded `string`. If `text` is empty, an empty\n"
                     "    `string` is returned.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.hex_encode"), args);
            // Parse arguments.
            D_string text;
            Opt<D_string> delim;
            Opt<D_boolean> uppercase;
            if(reader.start().g(text).g(delim).g(uppercase).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_string_hex_encode(text, delim, uppercase) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.hex_decode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("hex_decode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.hex_decode(hstr)`\n"
                     "  * Decodes all hexadecimal digits from `hstr` and converts them to\n"
                     "    bytes. Whitespaces are ignored. Characters that are neither\n"
                     "    hexadecimal digits nor whitespaces will cause parse errors.\n"
                     "  * Returns a `string` containing decoded bytes. If `hstr` is empty\n"
                     "    or consists only whitespaces, an empty `string` is returned. In\n"
                     "    the case of parse errors, `null` is returned.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.hex_decode"), args);
            // Parse arguments.
            D_string hstr;
            if(reader.start().g(hstr).finish()) {
              // Call the binding function.
              auto qtext = std_string_hex_decode(hstr);
              if(!qtext) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qtext) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.utf8_encode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("utf8_encode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.utf8_encode(code_points, [permissive])`\n"
                     "  * Encodes code points from `code_points` into an UTF-8 `string`.\n"
                     "  `code_points` can be either an `integer` or an `array` of\n"
                     "  `integer`s. When an invalid code point is encountered, if\n"
                     "  `permissive` is set to `true`, it is replaced with the\n"
                     "  replacement character `\"\\uFFFD\"` and consequently encoded as\n"
                     "  `\"\\xEF\\xBF\\xBD\"`; otherwise this function fails.\n"
                     "  * Returns the encoded `string` on success; otherwise `null`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.utf8_encode"), args);
            // Parse arguments.
            D_integer code_point;
            Opt<D_boolean> permissive;
            if(reader.start().g(code_point).g(permissive).finish()) {
              // Call the binding function.
              auto qtext = std_string_utf8_encode(code_point, permissive);
              if(!qtext) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qtext) };
              return rocket::move(xref);
            }
            D_array code_points;
            if(reader.start().g(code_points).g(permissive).finish()) {
              // Call the binding function.
              auto qtext = std_string_utf8_encode(code_points, permissive);
              if(!qtext) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qtext) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.utf8_decode()`
    //===================================================================
    result.insert_or_assign(rocket::sref("utf8_decode"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.utf8_decode(text, [permissive])`\n"
                     "  * Decodes `text`, which is expected to be a `string` containing\n"
                     "    UTF-8 code units, into an `array` of code points, represented\n"
                     "    as `integer`s. When an invalid code sequence is encountered, if\n"
                     "    `permissive` is set to `true`, all code units of it are\n"
                     "    re-interpreted as isolated bytes according to ISO/IEC 8859-1;\n"
                     "    otherwise this function fails.\n"
                     "  * Returns an `array` containing decoded code points; otherwise\n"
                     "    `null`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.utf8_decode"), args);
            // Parse arguments.
            D_string text;
            Opt<D_boolean> permissive;
            if(reader.start().g(text).g(permissive).finish()) {
              // Call the binding function.
              auto qres = std_string_utf8_decode(text, permissive);
              if(!qres) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { rocket::move(*qres) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack8()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack8"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack8(values)`\n"
                     "  * Packs a series of 8-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integer`s, all of\n"
                     "    which are truncated to 8 bits then copied into a `string`.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack8"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack8(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack8(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack8()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack8"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack8(text)`\n"
                     "  * Unpacks 8-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 8-bit integers, all of\n"
                     "    which are sign-extended to 64 bits then copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack8"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack8(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack16be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack16be"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack16be(values)`\n"
                     "  * Packs a series of 16-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are truncated to 16 bits then copied into a `string` in\n"
                     "    the big-endian byte order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack16be"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack16be(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack16be(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack16be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack16be"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack16be(text)`\n"
                     "  * Unpacks 16-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 16-bit integers in the\n"
                     "    big-endian byte order, all of which are sign-extended to 64\n"
                     "    bits then copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 2.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack16be"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack16be(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack16le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack16le"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack16le(values)`\n"
                     "  * Packs a series of 16-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are truncated to 16 bits then copied into a `string` in\n"
                     "    the little-endian byte order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack16le"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack16le(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack16le(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack16le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack16le"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack16le(text)`\n"
                     "  * Unpacks 16-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 16-bit integers in the\n"
                     "    little-endian byte order, all of which are sign-extended to 64\n"
                     "    bits then copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 2.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack16le"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack16le(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack32be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack32be"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack32be(values)`\n"
                     "  * Packs a series of 32-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are truncated to 32 bits then copied into a `string` in\n"
                     "    the big-endian byte order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack32be"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack32be(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack32be(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack32be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack32be"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack32be(text)`\n"
                     "  * Unpacks 32-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 32-bit integers in the\n"
                     "    big-endian byte order, all of which are sign-extended to 64\n"
                     "    bits then copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 4.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack32be"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack32be(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack32le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack32le"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack32le(values)`\n"
                     "  * Packs a series of 32-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are truncated to 32 bits then copied into a `string` in\n"
                     "    the little-endian byte order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack32le"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack32le(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack32le(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack32le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack32le"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack32le(text)`\n"
                     "  * Unpacks 32-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 32-bit integers in the\n"
                     "    little-endian byte order, all of which are sign-extended to 64\n"
                     "    bits then copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 4.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack32le"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack32le(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack64be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack64be"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack64be(values)`\n"
                     "  * Packs a series of 64-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are copied into a `string` in the big-endian byte order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack64be"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack64be(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack64be(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack64be()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack64be"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack64be(text)`\n"
                     "  * Unpacks 64-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 64-bit integers in the\n"
                     "    big-endian byte order, all of which are copied into an `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 8.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack64be"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack64be(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.pack64le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("pack64le"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.string.pack64le(values)`\n"
                     "  * Packs a series of 64-bit integers into a `string`. `values` can\n"
                     "    be either an `integer` or an `array` of `integers`, all of\n"
                     "    which are copied into a `string` in the little-endian byte\n"
                     "    order.\n"
                     "  * Returns the packed `string`.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.pack64le"), args);
            // Parse arguments.
            D_integer value;
            if(reader.start().g(value).finish()) {
              // Call the binding function.
              auto text = std_string_pack64le(value);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            D_array values;
            if(reader.start().g(values).finish()) {
              // Call the binding function.
              auto text = std_string_pack64le(values);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(text) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.string.unpack64le()`
    //===================================================================
    result.insert_or_assign(rocket::sref("unpack64le"),
      D_function(make_simple_binding(
        rocket::sref("`std.string.unpack64le(text)`\n"
                     "  * Unpacks 64-bit integers from a `string`. The contents of `text`\n"
                     "    are re-interpreted as contiguous signed 64-bit integers in the\n"
                     "    little-endian byte order, all of which are copied into an\n"
                     "    `array`.\n"
                     "  * Returns an `array` containing unpacked integers.\n"
                     "  * Throws an exception if the length of `text` is not a multiple\n"
                     "    of 8.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.string.unpack64le"), args);
            // Parse arguments.
            D_string text;
            if(reader.start().g(text).finish()) {
              // Call the binding function.
              auto values = std_string_unpack64le(text);
              // Forward the result.
              Reference_Root::S_temporary xref = { rocket::move(values) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // End of `std.string`
    //===================================================================
  }

}  // namespace Asteria
