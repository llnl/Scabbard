/**
 * @file device-defs.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief device defs for side wise compilation
 * @version alpha 0.0.1
 * @date 2023-06-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <scabbard/rtl/calls.hpp>
#include <scabbard/rtl/globals.hpp>
#include <scabbard/rtl/DeviceTracker.hpp>
#include <scabbard/instr-names.def>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>


namespace scabbard {
  namespace rtl {


    // << ========================================================================================== >> 
    // <<                                          GLOBALS                                           >> 
    // << ========================================================================================== >> 


    
    
    // << ======================================== Device ========================================== >> 
    namespace device {

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
      __device__
      void dbg_print(std::uint64_t value)
      {
        if (blockIdx.x != 0 && threadIdx.x != 0) return;
        const void* kernarg_ptr = __builtin_amdgcn_kernarg_segment_ptr();
        const auto* raw_args = static_cast<const uint8_t*>(kernarg_ptr);
        auto a0 = *reinterpret_cast<std::uintptr_t*>(raw_args+0);
        auto a1 = *reinterpret_cast<std::int32_t*>(raw_args+8);
        auto a2 = *reinterpret_cast<std::int32_t*>(raw_args+12);
        auto a3 = *reinterpret_cast<std::int32_t*>(raw_args+16);
        auto a4 = *reinterpret_cast<std::uintptr_t*>(raw_args+20);
        auto a5 = *reinterpret_cast<std::uintptr_t*>(raw_args+24);
        printf("\n[scabbard.rtl.device:DBG] %%0: %lu, %%1: %d, %%2: %lu, %%3: %lu, %%4: %lu, %%5: %lu",
                a0, a1, a2, a3, a4, a5);
      }
      
      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
      __device__
      void trace_append$mem(std::uintptr_t deviceTracker, const InstrData DATA, const void*const PTR, const void*const SRC_ID)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        const std::size_t BUF_SIZE = DT._BUF_SIZE;
        DT.buffer[(DT.next++) % BUF_SIZE] = TraceData(DT.vClk++, DATA, DT.JOB_ID,
                                                      blockIdx, threadIdx,
                                                      (std::uintptr_t)PTR, SRC_ID,  0ull);
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
      __device__
      void trace_append$alloc(std::uintptr_t deviceTracker, const InstrData DATA, const void*const PTR, 
                              const void*const SRC_ID, const std::size_t SIZE)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        const std::size_t BUF_SIZE = DT._BUF_SIZE;
        DT.buffer[(DT.next++) % BUF_SIZE] = TraceData(DT.vClk++, (InstrData)(DATA | InstrData::_OPT_USED),
                                                      DT.JOB_ID, blockIdx, threadIdx,
                                                      (std::uintptr_t)PTR,
                                                      SRC_ID, SIZE);
      }


    } //?namespace device
  
  
  } //?namespace rtl
} //?namespace scabbard



