/**
 * @file DeviceTracker.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief an object to be passed through the device calls to keep track of the data
 * @version alpha 0.0.1
 * @date 2024-02-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include <scabbard/TraceData.hpp>

#include <cstdint>
#include <hip/hip_runtime.h>
#include <hip/hip_runtime_api.h>

#define SCABBARD_DEVICE_TRACKER_BUFF_DEFAULT_SIZE (32768ull<<1ull)
// #define SCABBARD_DEVICE_TRACKER_BUFF_DEFAULT_SIZE (2048ul)


namespace scabbard {
namespace rtl {
namespace device {

  /// @brief A object to pass into a device kernel to bring in relevant information about 
  ///        the stream, job, and logical time.  As well as implement a cycle buffer to 
  ///        store the rtl data from the kernel job it belongs too. \n
  ///        It is expected to allocate the memory for this object using \c hipMallocManaged()
  ///        The size of the allocation should be retrieved using the static
  ///        fn \c DeviceTracker::getAllocSizeBytes() so that additional 
  ///        memory for the actual cycle buffer can be in the same allocation.
  ///        To construct a \c DeviceTracker object it is expected to use C++'s 
  ///        "in place" \c new interface with the memory allocated with \c hipMallocManaged()
  ///        as follows `new (managedPtr) DeviceTracker(...)`.
  ///        \c DeviceTracker 's should be free'ed using \c hipFree() .
  struct DeviceTracker {
    static std::size_t BUFFER_SIZE;
    const jobId_t JOB_ID;
    _Atomic(std::uint64_t) vClk;
    const std::size_t _BUF_SIZE;
    _Atomic(std::size_t) next = 0ul;
    std::size_t next_read = 0ul;
    _Atomic(bool) finished = false;
    /// @brief Pointer to the first address in the cycle buffer for the device tracker.
    ///        It should be set to address immediately word-size following 
    ///         this device tracker object in memory.
    ///        The size of the cycle buffer will be at least `sizeof(TraceData)*BUFFER_SIZE`
    TraceData* buffer;
    __host__ __device__
    DeviceTracker() = delete;
    __host__
    DeviceTracker(const jobId_t& JOB_ID_, std::uint64_t vClk_)
      : JOB_ID(JOB_ID_), vClk(vClk_), 
        _BUF_SIZE(DeviceTracker::BUFFER_SIZE),
        buffer(nullptr)
    {
      buffer = (TraceData*)(((std::uintptr_t)(&buffer))+sizeof(std::uintptr_t));
      // initialize the cycle buffer by zero-ing it out
      std::memset(buffer, 0u, getBuffAllocSizeBytes()+sizeof(TraceData)*2u);
    }
    __host__
    static inline size_t getAllocSizeBytes();
    __host__
    static inline size_t getBuffAllocSizeBytes();
    // __host__
    // DeviceTracker(const DeviceTracker& other)
    // {
    //   *this = other;
    // }
    // __host__
    // DeviceTracker& operator = (const DeviceTracker& other)
    // {
    //   std::uintptr_t tmp = (std::uintptr_t)&(this->buffer);
    //   std::memcpy(this, &other, sizeof(DeviceTracker));
    //   this->buffer = (TraceData*)(tmp+8u);
    // }
  };



} //?namespace device
} //?namespace rtl
} //?namespace scabbard