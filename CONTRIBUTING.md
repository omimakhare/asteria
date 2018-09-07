# Coding Standard (C++)

### Abstract

This document describes the coding style requirements of **Asteria**.

These rules cover everything in the `asteria/src` directory, excluding those in `asteria/src/rocket`.

### Standard Conformance

1. Code shall conform to _C++11_. _C++14_ or above is permitted with proper checks of [feature test macros](https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations).

2. These specifications are mandated and can be assumed to be always true:

    1. `char` has _exactly_ 8 bits; `int` has _at least_ 32 bits.
    2. All integral types are represented in [two's complement](https://en.wikipedia.org/wiki/Two%27s_complement); `int(8|16|32|64|ptr)_t` and their unsigned counterparts are always available.
    3. `ptrdiff_t` and `size_t` only differ in signness.
    4. `float` and `double` conform to the _single precision_ and _double precision_ formats in [IEEE 754-1985](https://en.wikipedia.org/wiki/IEEE_754-1985).

3. `_Names_that_begin_with_an_underscore_followed_by_an_uppercase_letter` and `__those_contain_consecutive_underscores` are reserved by the implementation and shall not be defined.

### Naming

1. A name denoting a _namespace_ or _type_ shall `Begin_with_an_uppercase_letter`, and otherwise shall `begin_with_a_lowercase_letter`. For categorization purposes, _class templates_, _function templates_ and _variable templates_ are treated as _types_, _functions_ and _variables_, respectively.

2. A name denoting a macro shall be in `ALL_CAPITAL_LETTERS`. The name of a macro in a public header, including the header guard, shall begin with `THE_PROJECT_NAME_`. The header guard of a `.hpp` file shall end with `_HPP_`.

### Use of Namespaces

```c++
namespace Root {
namespace Submodule {

namespace details_foo {

  extern void helper_function(int type);

}

inline void function_one()
  {
    return details_foo::helper_function(1);
  }

inline void function_two()
  {
    return details_foo::helper_function(2);
  }

}
}
```

1. Each file is associated with a _module namespace_. A module namespace may be exactly the root namespace of the project or a nested one in it. If it is a nested one then each level of nesting namespaces shall start and end on a separated line.

2. Non-public entities in a `.hpp` file shall be put in a nested _implementation namespace_. Usually this namespace shall have a name that begins with `details_`.

3. _Functions_, _classes_ and _variables_ defined in a `.cpp` file that are only ever used in the same file shall be put in an _unnamed namespace_, which is also an implementation namespace. Note that _type aliases_ and _enumerations_ are exempted from this rule.

4. There shall be a blank line right after the initiation of a module namespace or implementation namespace, as well as right before the termination.

5. The `{` that begins the body of a namespace shall follow the name of that namespace, separated by a space. There shall be no line break in between.

6. Members of a module namespace are not indented. Members of an implementation namespace are indented by one level.

### Formatting of Braces

```c++
class Some_class : public Some_base
  {
  private:
    unsigned m_index;
  
  public:
    Some_class()
      : Some_base(),
        m_index(5)
      {
      }

  public:
    int look_up() const
      {
        static constexpr char table[] =
          { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l' };
        const auto i = this->m_index;
        if(i >= sizeof(table)) {
          return -1;
        }
        const auto c = static_cast<unsigned char>(table[i]);
        return c;
      }
  };
```

1. An `{` that initiates the body of a `class`, `struct`, `enum`, `union` or function (this includes function templates, constructors and destructors) shall be preceded by a line break and indented by one level. Otherwise a link break in either front or back is discouraged unless the matching `}` cannot be folded onto the same line.

2. If an `{` follows an `(` or `{`, there shall be no space in between. If an `{` follows something other than a space character, an `(` or `{`, there shall be one space between them. Note that the latter case includes a `)` or `}`.

3. A `}` shall be indent to the same level as the line containing the matching `{` if it follows a line break.

