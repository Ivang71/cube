#include "profile.hpp"

#if defined(TRACY_ENABLE)

#include <cstdlib>
#include <new>

void* operator new(std::size_t size) {
    if (void* p = std::malloc(size)) { CUBE_PROFILE_ALLOC(p, size); return p; }
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept {
    if (!p) return;
    CUBE_PROFILE_FREE(p);
    std::free(p);
}

void* operator new[](std::size_t size) {
    if (void* p = std::malloc(size)) { CUBE_PROFILE_ALLOC(p, size); return p; }
    throw std::bad_alloc();
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    CUBE_PROFILE_FREE(p);
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

#endif


