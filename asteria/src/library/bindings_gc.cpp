// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "bindings_gc.hpp"
#include "argument_reader.hpp"
#include "simple_binding_wrapper.hpp"
#include "../runtime/global_context.hpp"
#include "../runtime/collector.hpp"
#include "../utilities.hpp"

namespace Asteria {

Opt<D_integer> std_gc_get_threshold(const Global_Context& global, const D_integer& generation)
  {
    if((generation < 0) || (generation > 2)) {
      return rocket::nullopt;
    }
    auto qcoll = global.get_collector_opt(generation & 0xFF);
    if(!qcoll) {
      return rocket::nullopt;
    }
    // Get the current threshold.
    auto thres = qcoll->get_threshold();
    return D_integer(thres);
  }

Opt<D_integer> std_gc_set_threshold(const Global_Context& global, const D_integer& generation, const D_integer& threshold)
  {
    if((generation < 0) || (generation > 2)) {
      return rocket::nullopt;
    }
    auto qcoll = global.get_collector_opt(generation & 0xFF);
    if(!qcoll) {
      return rocket::nullopt;
    }
    // Get the current threshold.
    auto thres = qcoll->get_threshold();
    // Set a new threshold.
    qcoll->set_threshold(static_cast<std::size_t>(rocket::clamp(threshold, 0, PTRDIFF_MAX)));
    return D_integer(thres);
  }

D_integer std_gc_collect(const Global_Context& global, const Opt<D_integer>& generation_limit)
  {
    // Perform full garbage collection.
    auto nvars = global.collect_variables(static_cast<unsigned>(rocket::clamp(generation_limit.value_or(INT_MAX), 0, INT_MAX)));
    return D_integer(nvars);
  }

void create_bindings_gc(D_object& result, API_Version /*version*/)
  {
    //===================================================================
    // `std.gc.get_threshold()`
    //===================================================================
    result.insert_or_assign(rocket::sref("get_threshold"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.gc.get_threshold(generation)`\n"
                     "  * Gets the threshold of the collector for `generation`. Valid\n"
                     "    values for `generation` are `0`, `1` and `2`.\n"
                     "  * Returns the threshold. If `generation` is not valid, `null` is\n"
                     "    returned.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& global, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.gc.get_threshold"), args);
            // Parse arguments.
            D_integer generation;
            if(reader.start().g(generation).finish()) {
              // Call the binding function.
              auto qthres = std_gc_get_threshold(global, generation);
              if(!qthres) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { *qthres };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.gc.set_threshold()`
    //===================================================================
    result.insert_or_assign(rocket::sref("set_threshold"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.gc.set_threshold(generation, threshold)`\n"
                     "  * Sets the threshold of the collector for `generation` to\n"
                     "    `threshold`. Valid values for `generation` are `0`, `1` and\n"
                     "    `2`. Valid values for `threshould` range from `0` to an\n"
                     "    unspecified positive `integer`; overlarge values are capped\n"
                     "    silently without failure. A larger `threshold` makes garbage\n"
                     "    collection run less often but slower. Setting `threshold` to\n"
                     "    `0` ensures all unreachable variables be collected immediately.\n"
                     "  * Returns the threshold before the call. If `generation` is not\n"
                     "    valid, `null` is returned.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& global, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.gc.set_threshold"), args);
            // Parse arguments.
            D_integer generation;
            D_integer threshold;
            if(reader.start().g(generation).g(threshold).finish()) {
              // Call the binding function.
              auto qoldthres = std_gc_set_threshold(global, generation, threshold);
              if(!qoldthres) {
                return Reference_Root::S_null();
              }
              Reference_Root::S_temporary xref = { *qoldthres };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // `std.gc.collect()`
    //===================================================================
    result.insert_or_assign(rocket::sref("collect"),
      D_function(make_simple_binding(
        // Description
        rocket::sref("`std.gc.collect([generation_limit])`\n"
                     "  * Performs garbage collection on all generations including and\n"
                     "    up to `generation_limit`. If it is absent, all generations are\n"
                     "    collected.\n"
                     "  * Returns the number of variables that have been collected in\n"
                     "    total.\n"),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& global, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.gc.collect"), args);
            // Parse arguments.
            Opt<D_integer> generation_limit;
            if(reader.start().g(generation_limit).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_gc_collect(global, generation_limit) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          },
        // Opaque parameter
        D_null()
      )));
    //===================================================================
    // End of `std.gc`
    //===================================================================
  }

}  // namespace Asteria
