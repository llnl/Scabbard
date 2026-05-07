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

#include <scabbard/trace/calls.hpp>
#include <scabbard/trace/globals.hpp>
#include <scabbard/trace/DeviceTracker.hpp>
#include <scabbard/instr-names.def>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>


namespace scabbard {
  namespace trace {


    // << ========================================================================================== >> 
    // <<                                          GLOBALS                                           >> 
    // << ========================================================================================== >> 

    // AsyncQueue TRACE_LOGGER; // created in scabbard init
    // __device__ DeviceAsyncQueue* DEVICE_TRACE_LOGGER;

    
    
    // << ======================================== Device ========================================== >> 
    namespace device {

      // NOT needed, since each job will have it's own buffer.
      // __device__ inline 
      // size_t getLaneId() // const
      // {
      //   return (size_t)(((blockDim.x*blockIdx.x) + (blockDim.y*blockIdx.y) + (blockDim.z*blockIdx.z)
      //           + threadIdx.x + threadIdx.y + threadIdx.z) % SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT);
      // }
      
      // __device__ __noinline__
      [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline, gnu::used, gnu::retain]] 
      __device__
      void trace_append$mem(void* deviceTracker, const InstrData data, const void*const PTR, const void*const SRC_ID)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        DT.buffer[(DT.next++) % SCABBARD_DEVICE_TRACKER_BUFF_LENGTH] = TraceData(DT.vClk++, data,
                                                                                  DT.JOB_ID, blockIdx, threadIdx,
                                                                                  (std::uint64_t)PTR, 
                                                                                  SRC_ID, 
                                                                                  0ull);
      }

      // __device__ __noinline__
      [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline, gnu::used, gnu::retain]] 
      __device__
      void trace_append$alloc(void* deviceTracker, const InstrData data, const void*const PTR, 
                              const void*const SRC_ID, const std::size_t size)
      {
        DeviceTracker& DT = *((DeviceTracker*) deviceTracker);
        DT.buffer[(DT.next++) % SCABBARD_DEVICE_TRACKER_BUFF_LENGTH] = TraceData(DT.vClk++, (InstrData)(data | InstrData::_OPT_USED),
                                                                                  DT.JOB_ID, blockIdx, threadIdx,
                                                                                  (std::uint64_t)PTR, 
                                                                                  SRC_ID, 
                                                                                  size);
      }


    } //?namespace device



    // << ========================================================================================== >> 
    // <<                                        GLOBAL TAGS                                         >> 
    // << ========================================================================================== >> 

    namespace {

      /**
       * @brief waisted space used to that the device side function does not optimized out
       *        from not being called from inside the llvm device module pre-instrumentation
       */
      // __global__ 
      // void call_for_looks(void* dt, int32_t intTy, float fTy, InstrData tmp, void* ptr, void* meta, const std::uint64_t* src_id) asm (SCABBARD_DEVICE_DUMMY_FUNC_NAME);

      // __global__ 
      // void call_for_looks(void* dt, int32_t intTy, float fTy, InstrData tmp, void* ptr, void* meta, const std::uint64_t* src_id)
      // {
      //   fTy *= intTy;
      //   *((float*) dt) = fTy;
      //   ::scabbard::trace::device::trace_append$mem(dt,tmp,ptr,101ul);
      //   ::scabbard::trace::device::trace_append$mem(dt,tmp,ptr,102ul);
      //   ::scabbard::trace::device::trace_append$mem(dt,tmp,ptr,103ul);
      //   ::scabbard::trace::device::trace_append$alloc(dt,tmp,ptr,104ul,7ul);
      //   ::scabbard::trace::device::trace_append$alloc(dt,tmp,ptr,105ul,23ul);
      //   ::scabbard::trace::device::trace_append$alloc(dt,tmp,ptr,106ul,230ul);
      // }

      // __global__ void call_for_differentiation(InstrData tmp, void* ptr, void* meta)
      // {
      //   ::scabbard::trace::device::trace_append$mem((InstrData)((InstrData)tmp | InstrData::FREE),ptr,meta);
      // }

    }
  
  
  } //?namespace trace
} //?namespace scabbard



