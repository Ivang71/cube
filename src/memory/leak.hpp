#pragma once

#include <cstddef>

namespace cube::mem {

using LeakBytesFn = std::size_t(*)(void*);

void register_leak_check(const char* name, void* ctx, LeakBytesFn fn);
void report_leaks();

} // namespace cube::mem

