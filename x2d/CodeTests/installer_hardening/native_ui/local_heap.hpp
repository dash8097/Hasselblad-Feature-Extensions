#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>
namespace x2dui {
// Own controllers never override the process-wide C++ new/delete symbols.
struct LocalHeap {
 static void*operator new(std::size_t n,const std::nothrow_t&)noexcept{return std::malloc(n);}
 static void operator delete(void*p)noexcept{std::free(p);}
};
}
