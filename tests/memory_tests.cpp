#include "memory/linear_allocator.hpp"
#include "memory/pool_allocator.hpp"
#include "memory/stack_allocator.hpp"

#include <cstdio>
#include <cstddef>
#include <array>

static int mfail(int code, const char* what) {
    std::fprintf(stderr, "cube_tests: FAIL(%d): %s\n", code, what);
    return code;
}

int run_memory_tests() {
    {
        std::array<std::byte, 128> buf{};
        cube::mem::LinearAllocator a(buf.data(), buf.size());
        void* p1 = a.alloc(16, 8);
        void* p2 = a.alloc(32, 16);
        if (!p1 || !p2) return mfail(201, "LinearAllocator alloc");
        if (a.used() == 0) return mfail(202, "LinearAllocator used");
        a.reset();
        if (a.used() != 0) return mfail(203, "LinearAllocator reset");
    }

    {
        std::array<std::byte, 128> buf{};
        cube::mem::StackAllocator a(buf.data(), buf.size());
        void* p1 = a.alloc(8, 8);
        void* p2 = a.alloc(16, 8);
        if (!p1 || !p2) return mfail(211, "StackAllocator alloc");
        auto m = a.mark();
        void* p3 = a.alloc(8, 8);
        if (!p3) return mfail(212, "StackAllocator alloc after mark");
        a.pop(m);
        void* p4 = a.alloc(8, 8);
        if (!p4) return mfail(213, "StackAllocator pop(marker)");
        a.free(p4);
        a.free(p2);
        void* p5 = a.alloc(16, 8);
        if (!p5) return mfail(214, "StackAllocator free LIFO");
    }

    {
        cube::mem::PoolAllocator p;
        if (!p.init(32, 4)) return mfail(221, "PoolAllocator init");
        void* a = p.alloc(8);
        void* b = p.alloc(8);
        void* c = p.alloc(8);
        void* d = p.alloc(8);
        void* e = p.alloc(8);
        if (!a || !b || !c || !d) return mfail(222, "PoolAllocator alloc 4");
        if (e) return mfail(223, "PoolAllocator overflow");
        p.free(b);
        void* f = p.alloc(8);
        if (!f) return mfail(224, "PoolAllocator reuse freed");
    }

    return 0;
}

