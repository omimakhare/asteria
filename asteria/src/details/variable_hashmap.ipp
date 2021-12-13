// This file is part of Asteria.
// Copyleft 2018 - 2021, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_LLDS_VARIABLE_HASHMAP_HPP_
#  error Please include <asteria/llds/variable_hashmap.hpp> instead.
#endif

namespace asteria {
namespace details_variable_hashmap {

struct Bucket
  {
    Bucket* next;  // the next bucket in the [non-circular] list;
                   // used for iteration
    Bucket* prev;  // the previous bucket in the [circular] list;
                   // used to mark whether this bucket is empty or not

    const void* key_p;
    union { rcptr<Variable> vstor[1];  };  // initialized iff `prev` is non-null

    Bucket() noexcept { }
    ~Bucket() noexcept { }

    explicit operator
    bool() const noexcept
      { return this->prev != nullptr;  }
  };

}  // namespace details_variable_hashmap
}  // namespace asteria
