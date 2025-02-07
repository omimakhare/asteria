// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#include "../precompiled.ipp"
#include "analytic_context.hpp"
#include "../utils.hpp"
namespace asteria {

Analytic_Context::
Analytic_Context(M_function, Abstract_Context* parent_opt,
                 const cow_vector<phsh_string>& params)
  :
    m_parent_opt(parent_opt)
  {
    // Set parameters, which are local references.
    for(const auto& name : params)
      if(name != sref("..."))
        this->do_mut_named_reference(nullptr, name);

    // Set pre-defined references.
    // N.B. If you have ever changed these, remember to update
    // 'executive_context.cpp' as well.
    this->do_mut_named_reference(nullptr, sref("__varg"));
    this->do_mut_named_reference(nullptr, sref("__this"));
    this->do_mut_named_reference(nullptr, sref("__func"));
  }

Analytic_Context::
~Analytic_Context()
  {
  }

}  // namespace asteria
