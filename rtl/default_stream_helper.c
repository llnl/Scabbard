
#include <hip/hip_runtime.h>

#ifndef __SCABBARD_HIP_DEFAULT_STREAM__
# ifdef HIP_API_PER_THREAD_DEFAULT_STREAM
#   define __SCABBARD_HIP_DEFAULT_STREAM__ 2u
# else
#   define __SCABBARD_HIP_DEFAULT_STREAM__ 1u
# endif
#endif

#include <inttypes.h>



uintptr_t DEFAULT_STREAM_BEHAVIOR() asm ("scabbard.rtl.DEFAULT_STREAM_BEHAVIOR");

uintptr_t DEFAULT_STREAM_BEHAVIOR() { return __SCABBARD_HIP_DEFAULT_STREAM__; }

