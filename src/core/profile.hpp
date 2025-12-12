#pragma once

#if defined(TRACY_ENABLE)
  #include <tracy/Tracy.hpp>
  #define CUBE_PROFILE_FRAME() FrameMark
  #define CUBE_PROFILE_SCOPE() ZoneScoped
  #define CUBE_PROFILE_SCOPE_N(name) ZoneScopedN(name)
  #define CUBE_PROFILE_ALLOC(ptr, size) TracyAlloc((ptr), (size))
  #define CUBE_PROFILE_FREE(ptr) TracyFree((ptr))
#else
  #define CUBE_PROFILE_FRAME() (void)0
  #define CUBE_PROFILE_SCOPE() (void)0
  #define CUBE_PROFILE_SCOPE_N(name) (void)0
  #define CUBE_PROFILE_ALLOC(ptr, size) (void)0
  #define CUBE_PROFILE_FREE(ptr) (void)0
#endif


