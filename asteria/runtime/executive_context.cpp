// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#include "../precompiled.ipp"
#include "executive_context.hpp"
#include "runtime_error.hpp"
#include "../runtime/runtime_error.hpp"
#include "ptc_arguments.hpp"
#include "enums.hpp"
#include "variable.hpp"
#include "../llds/avmc_queue.hpp"
#include "../llds/reference_stack.hpp"
#include "../utils.hpp"
namespace asteria {

ROCKET_FLATTEN
Executive_Context::
Executive_Context(M_function, Global_Context& global, Reference_Stack& stack,
                  Reference_Stack& alt_stack, const refcnt_ptr<Variadic_Arguer>& zvarg,
                  const cow_vector<phsh_string>& params, Reference&& self)
  :
    m_parent_opt(nullptr), m_global(&global), m_stack(&stack),
    m_alt_stack(&alt_stack), m_zvarg(zvarg)
  {
    if(self.is_temporary()) {
      // If the self reference is null, it is likely that `this` isn't ever
      // referenced in this function, so perform lazy initialization to avoid
      // this overhead.
      const auto& val = self.dereference_readonly();
      if(!val.is_null())
        this->do_mut_named_reference(nullptr, sref("__this")) = ::std::move(self);
    }
    else if(self.is_variable()) {
      // If the self reference points to a variable, copy it because it is
      // always an lvalue.
      this->do_mut_named_reference(nullptr, sref("__this")) = ::std::move(self);
    }
    else
      ASTERIA_THROW_RUNTIME_ERROR((
          "Invalid `this` reference passed to `$1`"),
          zvarg->func());

    // Set arguments. As arguments are evaluated from left to right, the
    // reference at the top is the last argument.
    size_t arg_counter = stack.size();
    bool has_ellipsis = false;

    for(const auto& name : params)
      if(name != sref("...")) {
        auto& param = this->do_mut_named_reference(nullptr, name);
        param.set_temporary(nullopt);

        // Try popping an argument and assign it to this parameter.
        if(arg_counter != 0)
          param = ::std::move(stack.mut_top(-- arg_counter));
      }
      else
        has_ellipsis = true;

    if(!has_ellipsis && (arg_counter != 0))
      ASTERIA_THROW_RUNTIME_ERROR((
          "Too many arguments passed to `$1`"),
          zvarg->func());

    // Move all arguments into the variadic argument getter.
    while(arg_counter != 0)
      this->m_lazy_args.emplace_back(::std::move(stack.mut_top(-- arg_counter)));
  }

Executive_Context::
~Executive_Context()
  {
  }

Reference*
Executive_Context::
do_create_lazy_reference_opt(Reference* hint_opt, phsh_stringR name) const
  {
    // Create pre-defined references as needed.
    // N.B. If you have ever changed these, remember to update
    // 'analytic_context.cpp' as well.
    if(name == sref("__func")) {
      auto& ref = this->do_mut_named_reference(hint_opt, name);

      // Note: This can only happen inside a function context.
      ref.set_temporary(this->m_zvarg->func());
      return &ref;
    }

    if(name == sref("__this")) {
      auto& ref = this->do_mut_named_reference(hint_opt, name);

      // Note: This can only happen inside a function context and the `this`
      // argument is null.
      ref.set_temporary(nullopt);
      return &ref;
    }

    if(name == sref("__varg")) {
      auto& ref = this->do_mut_named_reference(hint_opt, name);

      // Use the zero-ary argument getter if there is variadic argument.
      // Create a new one otherwise.
      auto varg = this->m_zvarg;
      if(!this->m_lazy_args.empty())
        varg = ::rocket::make_refcnt<Variadic_Arguer>(*varg, this->m_lazy_args);

      // Note: This can only happen inside a function context.
      ref.set_temporary(::std::move(varg));
      return &ref;
    }

    return nullptr;
  }

void
Executive_Context::
defer_expression(const Source_Location& sloc, AVMC_Queue&& queue)
  {
    this->m_defer.emplace_back(sloc, ::std::move(queue));
  }

void
Executive_Context::
do_on_scope_exit_normal_slow(AIR_Status status)
  {
    // Stash the result reference, if any.
    Reference self;
    if(status == air_status_return_ref) {
      // If a PTC wrapper was returned, append all deferred expressions to it.
      // These callbacks will be unpacked later, so we just return.
      if(this->m_stack->top().is_ptc()) {
        const auto ptc = this->m_stack->top().unphase_ptc_opt();
        ROCKET_ASSERT(ptc);
        ptc->defer().append(this->m_defer.move_begin(), this->m_defer.move_end());
        return;
      }

      self = ::std::move(this->m_stack->mut_top());
    }

    // Execute all deferred expressions backwards.
    while(!this->m_defer.empty()) {
      auto pair = ::std::move(this->m_defer.mut_back());
      this->m_defer.pop_back();

      // Execute it.
      // If an exception is thrown, append a frame and rethrow it.
      try {
        pair.second.execute(*this);
      }
      catch(Runtime_Error& except) {
        except.push_frame_defer(pair.first);
        this->do_on_scope_exit_exceptional_slow(except);
        throw;
      }
    }

    // Restore the result reference.
    if(!self.is_invalid())
      this->m_stack->push() = ::std::move(self);
  }

void
Executive_Context::
do_on_scope_exit_exceptional_slow(Runtime_Error& except)
  {
    // Execute all deferred expressions backwards.
    while(!this->m_defer.empty()) {
      auto pair = ::std::move(this->m_defer.mut_back());
      this->m_defer.pop_back();

      // Execute it.
      // If an exception is thrown, replace `except` with it.
      try {
        pair.second.execute(*this);
      }
      catch(Runtime_Error& nested) {
        except = nested;
        except.push_frame_defer(pair.first);
      }
    }
  }

}  // namespace asteria
