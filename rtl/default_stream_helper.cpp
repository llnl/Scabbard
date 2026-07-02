

#ifndef __SCABBARD_HIP_DEFAULT_STREAM__
#define __SCABBARD_HIP_DEFAULT_STREAM__ 1u
#endif

#include <cinttypes>

namespace scabbard {
namespace rtl{

  constexpr uintptr_t DEFAULT_STREAM_BEHAVIOR() asm ("scabbard.rtl.DEFAULT_STREAM_BEHAVIOR");

  constexpr uintptr_t DEFAULT_STREAM_BEHAVIOR() { return __SCABBARD_HIP_DEFAULT_STREAM__; }
} //?namespace rtl
} //?namespace scabbard
