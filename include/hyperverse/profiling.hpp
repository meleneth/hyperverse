#pragma once

#if defined(HYPERVERSE_ENABLE_TRACY)
#include <tracy/Tracy.hpp>
#define HYPERVERSE_PROFILE_ZONE(name) ZoneScopedN(name)
#define HYPERVERSE_PROFILE_FRAME() FrameMark
#else
#define HYPERVERSE_PROFILE_ZONE(name) ((void)0)
#define HYPERVERSE_PROFILE_FRAME() ((void)0)
#endif
