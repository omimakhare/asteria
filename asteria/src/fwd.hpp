// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_FWD_HPP_
#define ASTERIA_FWD_HPP_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <type_traits> // so many...
#include <exception> // std::exception, std::terminate()
#include <utility> // std::move(), std::forward()
#include <cstddef> // std::nullptr_t
#include <cstdint> // std::int64_t, std::uint64_t
#include "rocket/preprocessor_utilities.h"
#include "rocket/assert.hpp"
#include "rocket/cow_string.hpp"
#include "rocket/cow_vector.hpp"
#include "rocket/cow_hashmap.hpp"
#include "rocket/static_vector.hpp"
#include "rocket/prehashed_string.hpp"
#include "rocket/variant.hpp"
#include "rocket/refcounted_ptr.hpp"

namespace Asteria {

// Utilities
class Formatter;
class Runtime_error;
class Exception;

// Syntax
class Source_location;
class Xpnode;
class Expression;
class Statement;
class Block;

// Runtime
class Value;
class Abstract_opaque;
class Shared_opaque_wrapper;
class Abstract_function;
class Shared_function_wrapper;
class Reference_root;
class Reference_modifier;
class Reference;
class Reference_stack;
class Reference_dictionary;
class Variable;
class Variable_hashset;
class Abstract_variable_callback;
class Collector;
class Abstract_context;
class Analytic_context;
class Executive_context;
class Global_context;
class Global_collector;
class Variadic_arguer;
class Instantiated_function;

// Compiler
class Parser_error;
class Token;
class Token_stream;
class Parser;
class Single_source_file;

// Fundamental Types
using D_null      = std::nullptr_t;
using D_boolean   = bool;
using D_integer   = std::int64_t;
using D_real      = double;
using D_string    = rocket::cow_string;
using D_opaque    = Shared_opaque_wrapper;
using D_function  = Shared_function_wrapper;
using D_array     = rocket::cow_vector<Value>;
using D_object    = rocket::cow_hashmap<rocket::prehashed_string, Value, rocket::prehashed_string::hash, rocket::prehashed_string::equal_to>;

}

#endif
