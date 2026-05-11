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

      
      [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline, gnu::used, gnu::retain]] 
      __device__
      void trace_append$mem(void* deviceTracker, const InstrData DATA, const void*const PTR, const void*const SRC_ID)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        const std::size_t BUF_SIZE = DT._BUF_SIZE;
        DT.buffer[(DT.next++) % BUF_SIZE] = TraceData(DT.vClk++, DATA, DT.JOB_ID,
                                                      blockIdx, threadIdx,
                                                      (std::uint64_t)PTR, SRC_ID,  0ull);
      }

      [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline, gnu::used, gnu::retain]] 
      __device__
      void trace_append$alloc(void* deviceTracker, const InstrData DATA, const void*const PTR, 
                              const void*const SRC_ID, const std::size_t SIZE)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        const std::size_t BUF_SIZE = DT._BUF_SIZE;
        DT.buffer[(DT.next++) % BUF_SIZE] = TraceData(DT.vClk++, (InstrData)(DATA | InstrData::_OPT_USED),
                                                      DT.JOB_ID, blockIdx, threadIdx,
                                                      (std::uint64_t)PTR, 
                                                      SRC_ID, SIZE);
      }


    } //?namespace device
  
  
  } //?namespace rtl
} //?namespace scabbard



