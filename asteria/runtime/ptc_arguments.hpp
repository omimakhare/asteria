// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_RUNTIME_PTC_ARGUMENTS_
#define ASTERIA_RUNTIME_PTC_ARGUMENTS_

#include "../fwd.hpp"
#include "../llds/reference_stack.hpp"
#include "../source_location.hpp"
namespace asteria {

class PTC_Arguments final
  :
    public rcfwd<PTC_Arguments>
  {
  private:
    // These describe characteristics of the function call.
    Source_Location m_sloc;
    PTC_Aware m_ptc;

    // These are the target function and its arguments.
    cow_function m_target;
    Reference_Stack m_stack;

    // These are captured data.
    rcfwd_ptr<const Variadic_Arguer> m_caller_opt;
    cow_bivector<Source_Location, AVMC_Queue> m_defer;

  public:
    explicit
    PTC_Arguments(const Source_Location& sloc, PTC_Aware ptc,
                  const cow_function& target, Reference_Stack&& stack)
      :
        m_sloc(sloc), m_ptc(ptc),
        m_target(target), m_stack(::std::move(stack))  { }

  public:
    ASTERIA_NONCOPYABLE_DESTRUCTOR(PTC_Arguments);

    const Source_Location&
    sloc() const noexcept
      { return this->m_sloc;  }

    PTC_Aware
    ptc_aware() const noexcept
      { return this->m_ptc;  }

    const cow_function&
    target() const noexcept
      { return this->m_target;  }

    const Reference_Stack&
    stack() const noexcept
      { return this->m_stack;  }

    Reference_Stack&
    stack() noexcept
      { return this->m_stack;  }

    ASTERIA_INCOMPLET(Variadic_Arguer)
    refcnt_ptr<const Variadic_Arguer>
    caller_opt() const noexcept
      { return unerase_pointer_cast<const Variadic_Arguer>(this->m_caller_opt);  }

    template<typename ArguerT>
    void
    set_caller(const refcnt_ptr<ArguerT>& caller) noexcept
      { this->m_caller_opt = ::std::move(caller);  }

    const cow_bivector<Source_Location, AVMC_Queue>&
    defer() const noexcept
      { return this->m_defer;  }

    cow_bivector<Source_Location, AVMC_Queue>&
    defer() noexcept
      { return this->m_defer;  }
  };

}  // namespace asteria
#endif
