// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#include "../precompiled.ipp"
#include "json.hpp"
#include "../argument_reader.hpp"
#include "../binding_generator.hpp"
#include "../runtime/runtime_error.hpp"
#include "../runtime/global_context.hpp"
#include "../compiler/token_stream.hpp"
#include "../compiler/compiler_error.hpp"
#include "../compiler/enums.hpp"
#include "../utils.hpp"
namespace asteria {
namespace {

struct Indenter
  {
    virtual
    ~Indenter();

    virtual
    void
    break_line(tinyfmt& fmt) const = 0;

    virtual
    void
    increment_level() = 0;

    virtual
    void
    decrement_level() = 0;

    virtual
    bool
    has_indention() const noexcept = 0;
  };

Indenter::
~Indenter()
  {
  }

class Indenter_none final
  :
    public Indenter
  {
  public:
    explicit
    Indenter_none() = default;

  public:
    void
    break_line(tinyfmt& /*fmt*/) const override
      {
      }

    void
    increment_level() override
      {
      }

    void
    decrement_level() override
      {
      }

    bool
    has_indention() const noexcept override
      {
        return false;
      }
  };

class Indenter_string final
  :
    public Indenter
  {
  private:
    cow_string m_add;
    cow_string m_cur;

  public:
    explicit
    Indenter_string(stringR add)
      :
        m_add(add), m_cur(sref("\n"))
      {
      }

  public:
    void
    break_line(tinyfmt& fmt) const override
      {
        fmt << this->m_cur;
      }

    void
    increment_level() override
      {
        this->m_cur.append(this->m_add);
      }

    void
    decrement_level() override
      {
        this->m_cur.pop_back(this->m_add.size());
      }

    bool
    has_indention() const noexcept override
      {
        return this->m_add.size() != 0;
      }
  };

class Indenter_spaces final
  :
    public Indenter
  {
  private:
    size_t m_add;
    size_t m_cur;

  public:
    explicit
    Indenter_spaces(int64_t add)
      :
        m_add(::rocket::clamp_cast<size_t>(add, 0, 10)), m_cur(0)
      {
      }

  public:
    void
    break_line(tinyfmt& fmt) const override
      {
        static constexpr char spaces[] = "                       ";
        static constexpr size_t nspaces = ::rocket::xstrlen(spaces);

        // When `step` is zero, separate fields with a single space.
        if(ROCKET_EXPECT(this->m_add == 0)) {
          fmt << spaces[0];
          return;
        }

        // Otherwise, terminate the current line, and indent the next.
        size_t nrem = this->m_cur;
        fmt << '\n';
        while(ROCKET_UNEXPECT(nrem > nspaces)) {
          nrem -= nspaces;
          fmt.putn(spaces, nspaces);
        }
        fmt.putn(spaces, nrem);
      }

    void
    increment_level() override
      {
        this->m_cur += this->m_add;
      }

    void
    decrement_level() override
      {
        this->m_cur -= this->m_add;
      }

    bool
    has_indention() const noexcept override
      {
        return this->m_add != 0;
      }
  };

void
do_quote_string(tinyfmt& fmt, stringR str)
  {
    // Although JavaScript uses UCS-2 rather than UTF-16, the JSON specification
    // adopts UTF-16.
    fmt << '\"';
    size_t offset = 0;
    while(offset < str.size()) {
      // Convert UTF-8 to UTF-16.
      char32_t cp;
      if(!utf8_decode(cp, str, offset))
        // Invalid UTF-8 code units are replaced with the replacement character.
        cp = 0xFFFD;

      // Escape double quotes, backslashes, and control characters.
      switch(cp) {
        case '\"':
          fmt << "\\\"";
          break;

        case '\\':
          fmt << "\\\\";
          break;

        case '\b':
          fmt << "\\b";
          break;

        case '\f':
          fmt << "\\f";
          break;

        case '\n':
          fmt << "\\n";
          break;

        case '\r':
          fmt << "\\r";
          break;

        case '\t':
          fmt << "\\t";
          break;

        default: {
          if((0x20 <= cp) && (cp <= 0x7E)) {
            // Write printable characters as is.
            fmt << (char) cp;
            break;
          }

          // Encode the character in UTF-16.
          char16_t ustr[2];
          char16_t* epos = ustr;
          utf16_encode(epos, cp);

          // Write code units.
          ::rocket::ascii_numput nump;
          for(auto p = ustr;  p != epos;  ++p) {
            nump.put_XU(*p, 4);
            char seq[8] = { "\\u" };
            ::std::memcpy(seq + 2, nump.data() + 2, 4);
            fmt.putn(seq, 6);
          }
          break;
        }
      }
    }
    fmt << '\"';
  }

void
do_format_object_key(tinyfmt& fmt, bool json5, const Indenter& indent, stringR name)
  {
    // Write the key.
    if(json5 && name.size() && is_cmask(name[0], cmask_namei)
         && ::std::all_of(name.begin() + 1, name.end(),
               [](char c) { return is_cmask(c, cmask_namei | cmask_digit);  }))
      fmt << name;
    else
      do_quote_string(fmt, name);

    // Write the colon.
    if(indent.has_indention())
      fmt << ": ";
    else
      fmt << ':';
  }

bool
do_find_uncensored(V_object::const_iterator& curp, const V_object& object)
  {
    for(;;)
      if(curp == object.end())
        return false;
      else if(::rocket::is_none_of(curp->second.type(),
                 { type_null, type_boolean, type_integer, type_real, type_string,
                   type_array, type_object }))
        ++curp;
      else
        return true;
  }

struct Xformat_array
  {
    const V_array* refa;
    V_array::const_iterator curp;
  };

struct Xformat_object
  {
    const V_object* refo;
    V_object::const_iterator curp;
  };

using Xformat = ::rocket::variant<Xformat_array, Xformat_object>;

V_string
do_format_nonrecursive(const Value& value, bool json5, Indenter& indent)
  {
    // Transform recursion to iteration using a handwritten stack.
    ::rocket::tinyfmt_str fmt;
    auto qval = &value;
    cow_vector<Xformat> stack;

    // Format a value. `qval` must always point to a valid value here.
  format_next:
    switch(weaken_enum(qval->type())) {
      case type_boolean:
        // Write `true` or `false`.
        fmt << qval->as_boolean();
        break;

      case type_integer:
        // Write the integer in decimal.
        fmt << (double) qval->as_integer();
        break;

      case type_real: {
        double real = qval->as_real();
        if(::std::isfinite(real)) {
          // Write the real in decimal.
          fmt << real;
        }
        else if(!json5) {
          // Censor the value.
          fmt << "null";
        }
        else if(!::std::isnan(real)) {
          // JSON5 allows infinities in ECMAScript form.
          fmt << "Infinity";
        }
        else {
          // JSON5 allows NaNs in ECMAScript form.
          fmt << "NaN";
        }
        break;
      }

      case type_string:
        // Write the quoted string.
        do_quote_string(fmt, qval->as_string());
        break;

      case type_array: {
        const auto& array = qval->as_array();
        fmt << '[';

        Xformat_array ctxa = { &array, array.begin() };
        if(ctxa.curp != array.end()) {
          // Open an array.
          indent.increment_level();
          indent.break_line(fmt);

          qval = &*(ctxa.curp);
          stack.emplace_back(::std::move(ctxa));
          goto format_next;
        }

        fmt << ']';
        break;
      }

      case type_object: {
        const auto& object = qval->as_object();
        fmt << '{';

        Xformat_object ctxo = { &object, object.begin() };
        if(do_find_uncensored(ctxo.curp, object)) {
          // Open an object.
          indent.increment_level();
          indent.break_line(fmt);
          do_format_object_key(fmt, json5, indent, ctxo.curp->first);

          qval = &(ctxo.curp->second);
          stack.emplace_back(::std::move(ctxo));
          goto format_next;
        }

        fmt << '}';
        break;
      }

      default:
        // Anything else is censored to `null`.
        fmt << "null";
        break;
    }

    while(stack.size()) {
      // Advance to the next element.
      auto& ctx = stack.mut_back();
      switch(ctx.index()) {
        case 0: {
          auto& ctxa = ctx.mut<0>();
          ++ ctxa.curp;
          if(ctxa.curp != ctxa.refa->end()) {
            fmt << ',';
            indent.break_line(fmt);

            // Format the next element.
            qval = &*(ctxa.curp);
            goto format_next;
          }

          // Close this array.
          if(json5 && indent.has_indention())
            fmt << ',';

          indent.decrement_level();
          indent.break_line(fmt);
          fmt << ']';
          break;
        }

        case 1: {
          auto& ctxo = ctx.mut<1>();
          if(do_find_uncensored(++(ctxo.curp), *(ctxo.refo))) {
            fmt << ',';
            indent.break_line(fmt);
            do_format_object_key(fmt, json5, indent, ctxo.curp->first);

            // Format the next value.
            qval = &(ctxo.curp->second);
            goto format_next;
          }

          // Close this object.
          if(json5 && indent.has_indention())
            fmt << ',';

          indent.decrement_level();
          indent.break_line(fmt);
          fmt << '}';
          break;
        }

        default:
          ROCKET_ASSERT(false);
      }

      stack.pop_back();
    }

    return fmt.extract_string();
  }

V_string
do_format_nonrecursive(const Value& value, bool json5, Indenter&& indent)
  {
    return do_format_nonrecursive(value, json5, indent);
  }

opt<Punctuator>
do_accept_punctuator_opt(Token_Stream& tstrm, initializer_list<Punctuator> accept)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      return nullopt;

    if(!qtok->is_punctuator())
      return nullopt;

    auto punct = qtok->as_punctuator();
    if(::rocket::is_none_of(punct, accept))
      return nullopt;

    tstrm.shift();
    return punct;
  }

struct Xparse_array
  {
    V_array arr;
  };

struct Xparse_object
  {
    V_object obj;
    phsh_string key;
    Source_Location key_sloc;
  };

using Xparse = ::rocket::variant<Xparse_array, Xparse_object>;

void
do_accept_object_key(Xparse_object& ctxo, Token_Stream& tstrm)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      throw Compiler_Error(Compiler_Error::M_status(),
                compiler_status_closing_brace_or_json5_key_expected, tstrm.next_sloc());

    switch(weaken_enum(qtok->index())) {
      case Token::index_identifier:
        ctxo.key = qtok->as_identifier();
        break;

      case Token::index_string_literal:
        ctxo.key = qtok->as_string_literal();
        break;

      default:
        throw Compiler_Error(Compiler_Error::M_status(),
                  compiler_status_closing_brace_or_json5_key_expected, tstrm.next_sloc());
    }

    ctxo.key_sloc = qtok->sloc();
    tstrm.shift();

    auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_colon });
    if(!kpunct)
      throw Compiler_Error(Compiler_Error::M_status(),
                compiler_status_colon_expected, tstrm.next_sloc());
  }

Value
do_parse_nonrecursive(Token_Stream& tstrm)
  {
    // Implement a non-recursive descent parser.
    Value value;
    cow_vector<Xparse> stack;

    // Accept a value. No other things such as closed brackets are allowed.
  parse_next:
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      throw Compiler_Error(Compiler_Error::M_format(),
                compiler_status_expression_expected, tstrm.next_sloc(),
                "Value expected");

    switch(weaken_enum(qtok->index())) {
      case Token::index_punctuator:
        // Accept an `[` or `{`.
        if(qtok->as_punctuator() == punctuator_bracket_op) {
          tstrm.shift();

          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
          if(!kpunct) {
            stack.emplace_back(Xparse_array());
            goto parse_next;
          }

          // Accept an empty array.
          value = V_array();
          break;
        }
        else if(qtok->as_punctuator() == punctuator_brace_op) {
          tstrm.shift();

          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
          if(!kpunct) {
            stack.emplace_back(Xparse_object());
            do_accept_object_key(stack.mut_back().mut<Xparse_object>(), tstrm);
            goto parse_next;
          }

          // Accept an empty object.
          value = V_object();
          break;
        }
        else
          throw Compiler_Error(Compiler_Error::M_format(),
                    compiler_status_expression_expected, tstrm.next_sloc(),
                    "Value expected");

      case Token::index_identifier:
        // Accept a literal.
        if(qtok->as_identifier() == "null") {
          tstrm.shift();
          value = nullopt;
          break;
        }
        else if(qtok->as_identifier() == "true") {
          tstrm.shift();
          value = true;
          break;
        }
        else if(qtok->as_identifier() == "false") {
          tstrm.shift();
          value = false;
          break;
        }
        else if(qtok->as_identifier() == "Infinity") {
          tstrm.shift();
          value = ::std::numeric_limits<double>::infinity();
          break;
        }
        else if(qtok->as_identifier() == "NaN") {
          tstrm.shift();
          value = ::std::numeric_limits<double>::quiet_NaN();
          break;
        }
        else
          throw Compiler_Error(Compiler_Error::M_format(),
                    compiler_status_expression_expected, tstrm.next_sloc(),
                    "Value expected");

      case Token::index_real_literal:
        // Accept a number.
        value = qtok->as_real_literal();
        tstrm.shift();
        break;

      case Token::index_string_literal:
        // Accept a UTF-8 string.
        value = qtok->as_string_literal();
        tstrm.shift();
        break;

      default:
        throw Compiler_Error(Compiler_Error::M_format(),
                  compiler_status_expression_expected, tstrm.next_sloc(),
                  "Value expected");
    }

    while(stack.size()) {
      // Advance to the next element.
      auto& ctx = stack.mut_back();
      switch(ctx.index()) {
        case 0: {
          auto& ctxa = ctx.mut<Xparse_array>();
          ctxa.arr.emplace_back(::std::move(value));

          // Look for the next element.
          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl, punctuator_comma });
          if(!kpunct)
            throw Compiler_Error(Compiler_Error::M_status(),
                      compiler_status_closing_bracket_or_comma_expected, tstrm.next_sloc());

          if(*kpunct == punctuator_comma) {
            // A closing bracket may still follow.
            kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
            if(!kpunct)
              goto parse_next;
          }

          // Close this array.
          value = ::std::move(ctxa.arr);
          break;
        }

        case 1: {
          auto& ctxo = ctx.mut<Xparse_object>();
          auto pair = ctxo.obj.try_emplace(::std::move(ctxo.key), ::std::move(value));
          if(!pair.second)
            throw Compiler_Error(Compiler_Error::M_status(),
                      compiler_status_duplicate_key_in_object, ctxo.key_sloc);

          // Look for the next element.
          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl, punctuator_comma });
          if(!kpunct)
            throw Compiler_Error(Compiler_Error::M_status(),
                      compiler_status_closing_brace_or_comma_expected, tstrm.next_sloc());

          if(*kpunct == punctuator_comma) {
            // A closing brace may still follow.
            kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
            if(!kpunct) {
              do_accept_object_key(stack.mut_back().mut<Xparse_object>(), tstrm);
              goto parse_next;
            }
          }

          // Close this object.
          value = ::std::move(ctxo.obj);
          break;
        }

        default:
          ROCKET_ASSERT(false);
      }

      stack.pop_back();
    }

    return value;
  }

Value
do_parse(tinybuf& cbuf)
  {
    // We reuse the lexer of Asteria here, allowing quite a few extensions e.g. binary numeric
    // literals and comments.
    Compiler_Options opts;
    opts.escapable_single_quotes = true;
    opts.keywords_as_identifiers = true;
    opts.integers_as_reals = true;

    Token_Stream tstrm(opts);
    tstrm.reload(sref("[JSON text]"), 1, ::std::move(cbuf));
    if(tstrm.empty())
      ASTERIA_THROW_RUNTIME_ERROR(("Empty JSON string"));

    // Parse a single value.
    auto value = do_parse_nonrecursive(tstrm);
    if(!tstrm.empty())
      ASTERIA_THROW_RUNTIME_ERROR(("Excess text at end of JSON string"));

    return value;
  }

}  // namespace

V_string
std_json_format(Value value, optV_string indent, optV_boolean json5)
  {
    // No line break is inserted if `indent` is null or empty.
    return (!indent || indent->empty())
        ? do_format_nonrecursive(value, json5 == true, Indenter_none())
        : do_format_nonrecursive(value, json5 == true, Indenter_string(*indent));
  }

V_string
std_json_format(Value value, V_integer indent, optV_boolean json5)
  {
    // No line break is inserted if `indent` is non-positive.
    return (indent <= 0)
        ? do_format_nonrecursive(value, json5 == true, Indenter_none())
        : do_format_nonrecursive(value, json5 == true, Indenter_spaces(indent));
  }

Value
std_json_parse(V_string text)
  {
    // Parse characters from the string.
    ::rocket::tinybuf_str cbuf;
    cbuf.set_string(text, tinybuf::open_read);
    return do_parse(cbuf);
  }

Value
std_json_parse_file(V_string path)
  {
    // Try opening the file.
    ::rocket::unique_posix_file fp(::fopen(path.safe_c_str(), "rb"));
    if(!fp)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Could not open file '$1'",
          "[`fopen()` failed: ${errno:full}]"),
          path);

    // Parse characters from the file.
    ::rocket::tinybuf_file cbuf(::std::move(fp));
    return do_parse(cbuf);
  }

void
create_bindings_json(V_object& result, API_Version /*version*/)
  {
    result.insert_or_assign(sref("format"),
      ASTERIA_BINDING(
        "std.json.format", "[value], [indent]",
        Argument_Reader&& reader)
      {
        Value value;
        optV_string sind;
        V_integer iind;
        optV_boolean json5;

        reader.start_overload();
        reader.optional(value);
        reader.save_state(0);
        reader.optional(sind);
        reader.optional(json5);
        if(reader.end_overload())
          return (Value) std_json_format(value, sind, json5);

        reader.load_state(0);
        reader.required(iind);
        reader.optional(json5);
        if(reader.end_overload())
          return (Value) std_json_format(value, iind, json5);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("parse"),
      ASTERIA_BINDING(
        "std.json.parse", "text",
        Argument_Reader&& reader)
      {
        V_string text;

        reader.start_overload();
        reader.required(text);
        if(reader.end_overload())
          return (Value) std_json_parse(text);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("parse_file"),
      ASTERIA_BINDING(
        "std.json.parse_file", "path",
        Argument_Reader&& reader)
      {
        V_string path;

        reader.start_overload();
        reader.required(path);
        if(reader.end_overload())
          return (Value) std_json_parse_file(path);

        reader.throw_no_matching_function_call();
      });
  }

}  // namespace asteria
