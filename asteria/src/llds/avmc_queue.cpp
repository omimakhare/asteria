// This file is part of Asteria.
// Copyleft 2018 - 2022, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "avmc_queue.hpp"
#include "../runtime/air_node.hpp"
#include "../runtime/runtime_error.hpp"
#include "../runtime/enums.hpp"
#include "../utils.hpp"
#include <sys/mman.h>

namespace asteria {
namespace {

details_avmc_queue::Header*
do_vm_allocate_headers(uint32_t nhdrs)
  {
    const size_t msize = nhdrs * sizeof(details_avmc_queue::Header);
    void* mptr = ::mmap(nullptr, msize, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(mptr == MAP_FAILED)
      ASTERIA_THROW_RUNTIME_ERROR(
          "could not allocate $2 bytes of virtual memory"
          "[`mmap()` failed: $1]",
          format_errno(), msize);

    ::std::memset(mptr, 0xCC, msize);
    return static_cast<details_avmc_queue::Header*>(mptr);
  }

void
do_vm_free_headers(details_avmc_queue::Header* bptr, uint32_t nhdrs)
  {
    const size_t msize = nhdrs * sizeof(details_avmc_queue::Header);
    int err = ::munmap(bptr, msize);
    ROCKET_ASSERT(err == 0);
  }

void
do_vm_protect_headers(details_avmc_queue::Header* bptr, uint32_t nhdrs, int prot)
  {
    const size_t msize = nhdrs * sizeof(details_avmc_queue::Header);
    int err = ::mprotect(bptr, msize, PROT_READ | prot);
    ROCKET_ASSERT(err == 0);
  }

extern "C" AIR_Status
do_call_jit_code(Executive_Context& ctx, const uint8_t* jit_code);

__asm__ (
"""""""""""""""""""""""""""""""""""""""" R"'''''''''''''''(
do_call_jit_code:

    pushq %rbx           # push rbx
    movq %rdi, %rbx      # rbx := ctx
    jmpq *%rsi           # rsi := jit_code

)'''''''''''''''" """""""""""""""""""""""""""""""""""""""");

}  // namespace

void
AVMC_Queue::
do_destroy_nodes(bool xfree) noexcept
  {
    do_vm_protect_headers(this->m_bptr, this->m_estor, PROT_WRITE);

    auto next = this->m_bptr;
    const auto eptr = this->m_bptr + this->m_used;
    while(ROCKET_EXPECT(next != eptr)) {
      auto qnode = next;
      next += UINT32_C(1) + qnode->nheaders;

      // Destroy `sparam`, if any.
      if(qnode->meta_ver && qnode->pv_meta->dtor_opt)
        qnode->pv_meta->dtor_opt(qnode);

      // Deallocate the vtable and symbols.
      if(qnode->meta_ver)
        delete qnode->pv_meta;
    }

#ifdef ROCKET_DEBUG
    ::std::memset(this->m_bptr, 0xE6, this->m_estor * sizeof(Header));
#endif

    this->m_used = 0xDEADBEEF;
    if(!xfree)
      return;

    // Deallocate the old table.
    auto bold = ::std::exchange(this->m_bptr, (Header*)0xDEADBEEF);
    auto esold = ::std::exchange(this->m_estor, (size_t)0xBEEFDEAD);
    do_vm_free_headers(bold, esold);
  }

void
AVMC_Queue::
do_reallocate(uint32_t nadd)
  {
    // Allocate a new table.
    constexpr size_t nheaders_max = UINT32_MAX / sizeof(Header);
    if(nheaders_max - this->m_used < nadd)
      throw ::std::bad_alloc();

    // Allocate non-executable memory here. A call to `finalize()` is necessary
    // for making this piece of memory executable.
    uint32_t estor = this->m_used + nadd;
    auto bptr = do_vm_allocate_headers(estor);

    // Perform a bitwise copy of all contents of the old block.
    // This copies all existent headers and trivial data.
    // Note that the size is unchanged.
    auto bold = ::std::exchange(this->m_bptr, bptr);
    ::std::memcpy(bptr, bold, this->m_used * sizeof(Header));
    auto esold = ::std::exchange(this->m_estor, estor);
    if(ROCKET_EXPECT(!bold))
      return;

    // Move old non-trivial nodes if any.
    // Warning: no exception shall be thrown from the code below.
    uint32_t offset = 0;
    while(ROCKET_EXPECT(offset != this->m_used)) {
      auto qnode = bptr + offset;
      auto qfrom = bold + offset;
      offset += UINT32_C(1) + qnode->nheaders;

      // Relocate `sparam`, if any.
      if(qnode->meta_ver && qnode->pv_meta->reloc_opt)
        qnode->pv_meta->reloc_opt(qnode, qfrom);
    }

    // Deallocate the old table.
    do_vm_free_headers(bold, esold);
  }

details_avmc_queue::Header*
AVMC_Queue::
do_reserve_one(Uparam uparam, size_t size)
  {
    constexpr size_t size_max = UINT8_MAX * sizeof(Header) - 1;
    if(size > size_max)
      ASTERIA_THROW("invalid AVMC node size (`$1` > `$2`)", size, size_max);

    // Round the size up to the nearest number of headers.
    // This shall not result in overflows.
    size_t nheaders_p1 = (sizeof(Header) * 2 - 1 + size) / sizeof(Header);

    // Allocate a new memory block as needed.
    if(ROCKET_UNEXPECT(this->m_estor - this->m_used < nheaders_p1)) {
      size_t nadd = nheaders_p1;
#ifndef ROCKET_DEBUG
      // Reserve more space for non-debug builds.
      nadd |= this->m_used * 4;
#endif
      this->do_reallocate(static_cast<uint32_t>(nadd));
    }
    ROCKET_ASSERT(this->m_estor - this->m_used >= nheaders_p1);

    // Append a new node.
    // `uparam` is overlapped with `nheaders` so it must be assigned first.
    // The others can occur in any order.
    auto qnode = this->m_bptr + this->m_used;
    qnode->uparam = uparam;
    qnode->nheaders = static_cast<uint8_t>(nheaders_p1 - UINT32_C(1));
    qnode->meta_ver = 0;
    return qnode;
  }

AVMC_Queue&
AVMC_Queue::
do_append_trivial(Uparam uparam, Executor* exec, size_t size, const void* data_opt)
  {
    auto qnode = this->do_reserve_one(uparam, size);

    // Copy source data if `data_opt` is non-null. Fill zeroes otherwise.
    // This operation will not throw exceptions.
    if(data_opt)
      ::std::memcpy(qnode->sparam, data_opt, size);
    else if(size)
      ::std::memset(qnode->sparam, 0, size);

    // Accept this node.
    qnode->pv_exec = exec;
    this->m_used += UINT32_C(1) + qnode->nheaders;
    return *this;
  }

AVMC_Queue&
AVMC_Queue::
do_append_nontrivial(Uparam uparam, Executor* exec, const Source_Location* sloc_opt,
                     Var_Getter* vget_opt, Relocator* reloc_opt, Destructor* dtor_opt,
                     size_t size, Constructor* ctor_opt, intptr_t ctor_arg)
  {
    auto qnode = this->do_reserve_one(uparam, size);

    // Allocate metadata for this node.
    auto meta = ::rocket::make_unique<details_avmc_queue::Metadata>();
    uint8_t meta_ver = 1;

    meta->reloc_opt = reloc_opt;
    meta->dtor_opt = dtor_opt;
    meta->vget_opt = vget_opt;
    meta->exec = exec;

    if(sloc_opt) {
      meta->syms = *sloc_opt;
      meta_ver = 2;
    }

    // Invoke the constructor if `ctor_opt` is non-null. Fill zeroes otherwise.
    // If an exception is thrown, there is no effect.
    if(ctor_opt)
      ctor_opt(qnode, ctor_arg);
    else if(size)
      ::std::memset(qnode->sparam, 0, size);

    // Accept this node.
    qnode->pv_meta = meta.release();
    qnode->meta_ver = meta_ver;
    this->m_used += UINT32_C(1) + qnode->nheaders;
    return *this;
  }

AVMC_Queue&
AVMC_Queue::
finalize()
  {
    if(!this->m_bptr)
      return *this;

    // Fill in JIT machine code.
    // At the moment JIT is available on x86_64 only.
    auto next = this->m_bptr;
    const auto eptr = this->m_bptr + this->m_used;
    while(ROCKET_EXPECT(next != eptr)) {
      auto qnode = next;
      next += UINT32_C(1) + qnode->nheaders;

      // mov rdi, rbx
      qnode->jit_code[ 0] = 0x48;
      qnode->jit_code[ 1] = 0x89;
      qnode->jit_code[ 2] = 0xDF;

      // lea rsi, [rip - 26]
      qnode->jit_code[ 3] = 0x48;
      qnode->jit_code[ 4] = 0x8D;
      qnode->jit_code[ 5] = 0x35;
      qnode->jit_code[ 6] = 0xE6;
      qnode->jit_code[ 7] = 0xFF;
      qnode->jit_code[ 8] = 0xFF;
      qnode->jit_code[ 9] = 0xFF;

      // movabs rax, EXEC
      uintptr_t word;
      if(qnode->meta_ver == 0)
        word = reinterpret_cast<uintptr_t>(qnode->pv_exec);
      else
        word = reinterpret_cast<uintptr_t>(qnode->pv_meta->exec);

      qnode->jit_code[10] = 0x48;
      qnode->jit_code[11] = 0xB8;
      qnode->jit_code[12] = (uint8_t)(word >>  0);
      qnode->jit_code[13] = (uint8_t)(word >>  8);
      qnode->jit_code[14] = (uint8_t)(word >> 16);
      qnode->jit_code[15] = (uint8_t)(word >> 24);
      qnode->jit_code[16] = (uint8_t)(word >> 32);
      qnode->jit_code[17] = (uint8_t)(word >> 40);
      qnode->jit_code[18] = (uint8_t)(word >> 48);
      qnode->jit_code[19] = (uint8_t)(word >> 56);

      // call rax
      qnode->jit_code[20] = 0xFF;
      qnode->jit_code[21] = 0xD0;

      if(next == eptr) {
        // pop rbx
        qnode->jit_code[22] = 0x5B;
        // ret
        qnode->jit_code[23] = 0xC3;
        continue;
      }

      // test al, al
      qnode->jit_code[22] = 0x84;
      qnode->jit_code[23] = 0xC0;

      // jz NEXT
      word = 18 + qnode->nheaders * sizeof(Header);

      qnode->jit_code[24] = 0x0F;
      qnode->jit_code[25] = 0x84;
      qnode->jit_code[26] = (uint8_t)(word >>  0);
      qnode->jit_code[27] = (uint8_t)(word >>  8);
      qnode->jit_code[28] = (uint8_t)(word >> 16);
      qnode->jit_code[29] = (uint8_t)(word >> 24);

      // pop rbx
      qnode->jit_code[30] = 0x5B;
      // ret
      qnode->jit_code[31] = 0xC3;
    }

    do_vm_protect_headers(this->m_bptr, this->m_estor, PROT_EXEC);
    return *this;
  }

AIR_Status
AVMC_Queue::
execute(Executive_Context& ctx) const
  {
    if(!this->m_bptr)
      return air_status_next;

    // Use JIT code.
    return do_call_jit_code(ctx, this->m_bptr->jit_code);
  }

void
AVMC_Queue::
get_variables(Variable_HashMap& staged, Variable_HashMap& temp) const
  {
    auto next = this->m_bptr;
    const auto eptr = this->m_bptr + this->m_used;
    while(ROCKET_EXPECT(next != eptr)) {
      auto qnode = next;
      next += UINT32_C(1) + qnode->nheaders;

      // Get all variables from this node.
      if((qnode->meta_ver >= 1) && qnode->pv_meta->vget_opt)
        qnode->pv_meta->vget_opt(staged, temp, qnode);
    }
  }

}  // namespace asteria
