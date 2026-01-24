#pragma once
// Defines STL derived types

#include <cstddef>
#include <new>
#include <type_traits>
#include <vector>
#include <string>
#include "core/dataChunker.h"

namespace KorkApi
{

class Vm;
class VmInternal;

namespace VmAllocTLS
{
   KorkApi::VmInternal* get();
   void         set(KorkApi::VmInternal* vm);

   struct Scope
   {
      KorkApi::VmInternal* prev;
      Scope(KorkApi::VmInternal* vm) : prev(get()) { set(vm); }
      ~Scope() { set(prev); }
   };
}

namespace VMem
{
   void* allocBytes(std::size_t n);
   void  freeBytes(void* p);

   template<class T, class... A>
   T* New(A&&... a)
   {
      void* mem = allocBytes(sizeof(T));
      return new (mem) T(std::forward<A>(a)...);
   }

   template<class T>
   void Delete(T* p)
   {
      if (!p) return;
      p->~T();
      freeBytes(p);
   }

   template<class T>
   void DeleteArray(T* p)
   {
      if (!p) return;
      freeBytes(p);
   }

   template<class T>
   T* NewArray(size_t n)
   {
      // If you only use this for trivially constructible types, keep it simple.
      // Otherwise add header + element ctors/dtors.
      return static_cast<T*>(allocBytes(sizeof(T) * n));
   }

}

template <class T>
struct TlsVmAllocator
{
   using value_type = T;
   using is_always_equal = std::true_type; // allocator has no state
   using propagate_on_container_move_assignment = std::true_type;

   TlsVmAllocator() noexcept = default;
   template<class U> TlsVmAllocator(const TlsVmAllocator<U>&) noexcept {}

   [[nodiscard]] T* allocate(std::size_t n)
   {
      if (n > (std::size_t(-1) / sizeof(T))) throw std::bad_alloc();
      return static_cast<T*>(KorkApi::VMem::allocBytes(n * sizeof(T)));
   }

   void deallocate(T* p, std::size_t) noexcept
   {
      KorkApi::VMem::freeBytes(p);
   }

   template<class U>
   bool operator==(const TlsVmAllocator<U>&) const noexcept { return true; }
   template<class U>
   bool operator!=(const TlsVmAllocator<U>&) const noexcept { return false; }
};

using String = std::basic_string<char, std::char_traits<char>, TlsVmAllocator<char>>;
template<class T>
using Vector = std::vector<T, TlsVmAllocator<T>>;
using VMChunker = DataChunker<TlsVmAllocator<char>>;

}

// Set lexer and parser types

namespace SimpleParser
{
	using String = KorkApi::String;
   template<class T>
	using Vector = KorkApi::Vector<T>;
};

namespace SimpleLexer
{
	using String = KorkApi::String;
   template<class T>
   using Vector = KorkApi::Vector<T>;
}
