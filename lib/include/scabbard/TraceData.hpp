/**
 * @file TraceData.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief The data object that holds all of the data pertaining to a rtl entry 
 *        (plus some read and write operations)
 * @version alpha 0.0.1
 * @date 2023-06-02
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include "Enums.hpp"
#include "Metadata.hpp"

# include <hip/hip_ext.h>
# include <hip/hip_runtime_api.h>


#include <cstdint>
#include <cstring>
#include <thread>
#include <type_traits>
#include <new>
#include <utility>


namespace scabbard {
  // namespace rtl {

#pragma pack(1)
struct blockId_t {
  uint32_t x = 0u;
  uint16_t y = 0u;
  uint16_t z = 0u;
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__ 
  inline blockId_t(const dim3& blockId)
    : x(blockId.x),  
      y(blockId.y),
      z(blockId.z)
    {}
};
#pragma pack()
static_assert(sizeof(blockId_t) <= __WORDSIZE, "blockId_t is of the correct size");

#pragma pack(1)
struct threadId_t {
  uint16_t x;
  uint16_t y;
  uint8_t z;

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__ 
  inline threadId_t(const dim3& threadId)
    : x(threadId.x),  
      y(threadId.y),
      z(threadId.z)
    {}
};
#pragma pack()
static_assert(sizeof(threadId_t) <= __WORDSIZE, "threadId_t is of the correct size");

#pragma pack(1)
struct jobId_t {
  uint16_t JOB = 0u;
  uint16_t STREAM = 0u;

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__
  jobId_t(uint16_t JOB_, const hipStream_t STREAM_)
    : JOB(JOB_), STREAM(jobId_t::hash_stream_ptr(STREAM_))
  {}
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__
  static inline uint16_t hash_stream_ptr(const hipStream_t STREAM) 
  {
    if (not STREAM) return 0u;
    return (((std::uint64_t)STREAM) % (UINT16_MAX-1u)) + 1u;
  }
};
#pragma pack()

struct DeviceThreadId {
  jobId_t job;
  blockId_t block;
  threadId_t thread;
# ifdef __scabbard_hip_compile
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__ 
  inline DeviceThreadId(const jobId_t& jobId, const dim3& blockId, const dim3& threadId) 
    : job(jobId), block(blockId), thread(threadId) 
  {}
# else
  inline DeviceThreadId(const jobId_t& jobId, const blockId_t& b, const threadId_t& t)
    : job(jobId), block(b), thread(t)
  {}
  inline DeviceThreadId(const jobId_t& jobId, const scabbard::dim3& b, const scabbard::dim3& t)
    : job(jobId), block(b), thread(t)
  {}
  inline DeviceThreadId(const jobId_t& jobId, const threadId_t& t)
    : job(jobId), block({0u,0u,0u}), thread(t) 
  {}
  inline DeviceThreadId(const jobId_t& jobId, const scabbard::dim3& t)
    : job(jobId), block({0u,0u,0u}), thread(t)
  {}
  inline DeviceThreadId(uint32_t thread_x, uint32_t thread_y=0u, uint32_t thread_z=0u)
    : job((jobId_t){0u,0u}), block((scabbard::dim3){0u,0u,0u}), thread((scabbard::dim3){thread_x,thread_y,thread_z}) 
  {}
# endif
};
// static_assert(sizeof(DeviceThreadId) <= __WORDSIZE*2, "DeviceThreadID is of the correct size");

typedef std::thread::id HostThreadId;
static_assert(sizeof(HostThreadId) <= sizeof(DeviceThreadId), "HostThreadId is of the correct size");

union ThreadId {
  HostThreadId host;
  DeviceThreadId device;
  void* _NONE_;
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]]
  __device__ inline ThreadId(const jobId_t& job_, const dim3& blockId_, const dim3& threadId_) { device = DeviceThreadId(job_, blockId_, threadId_); }
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__
  ThreadId() { std::memset(this,0,sizeof(ThreadId)); this->host = ::std::this_thread::get_id(); }
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__
  ThreadId(void* _) { std::memset(this,0,sizeof(ThreadId)); }
  // [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  // __host__ __device__ bool ok() const { return this->_NONE_ != nullptr; } 
};
static_assert(sizeof(ThreadId) <= __WORDSIZE*2, "ThreadID is of the correct size");

template<typename T>
inline const T& reading_cast(const char* buffer, const std::size_t index, const std::size_t WORD_LEN)
{
  return *static_cast<T*>(buffer[index*(WORD_LEN/8)]);
}
// template<typename T>
// inline const T& reading_cast(const char* buffer, const std::size_t offset, const std::size_t size, const std::size_t WORD_LEN)
// {
//   return *static_cast<T*>(reinterpret_cast<oldT*>(buffer[offset]));
// }



struct TraceData {

  //DATA TYPE         NAME          DEFAULT VALUE          SIZE       W/PADDING (64b arch)
  std::size_t         time_stamp  = 0ull;               //  8B ( 64b)  8B ( 64b)
  InstrData           data        = InstrData::NEVER;   //  2B ( 16b)  8B ( 64b)
  ThreadId            threadId    = ((void*)nullptr);   // 24B (192b) 24B (192b)
  std::uintptr_t      ptr         = 0ull;               //  8B ( 64b)  8B ( 64b)
  const SrcMetadata*  metadata    = nullptr;            //  8B ( 64b)  8B ( 64b)
  std::size_t         _OPT_DATA   = 0ull;               //  8B ( 64b)  8B ( 64b)
  //                                               TOTALS: 58B (464b) 64B (512b)
  //                                           DATA OCCUPANCY: 90.06%
  // Constructors
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__ 
  TraceData() = default;

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__
  explicit TraceData(const TraceData& other) = default;
  // { std::memcpy(this, &other, sizeof(TraceData)); }

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__
  explicit TraceData(TraceData&& other) = default;
  //   : time_stamp(std::exchange(other.time_stamp, 0ull)), 
  //     data(std::exchange(other.data, InstrData::NONE)), 
  //     threadId(std::exchange(other.threadId, ((void*)nullptr))),
  //     ptr(std::exchange(other.ptr, 0ull)), 
  //     metadata(std::exchange(other.metadata, nullptr)), 
  //     _OPT_DATA(std::exchange(other._OPT_DATA, 0ull))
  // {}

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]]
  __device__ 
  TraceData(const std::size_t time_stamp_, InstrData data_, 
            const jobId_t& JOB_ID, const dim3& blockId_, const dim3 threadId_,
            const std::uintptr_t ptr_, const void*const metadata_, 
            const std::size_t size_=0ull)
    : time_stamp(time_stamp_), data(data_), threadId(JOB_ID, blockId_, threadId_),
      ptr(ptr_), metadata((SrcMetadata*)metadata_), _OPT_DATA(size_)
    {}

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]]
  __host__
  TraceData(const size_t time_stamp_, const InstrData data_, const ThreadId& threadId_,
                    const std::uintptr_t ptr_, const SrcMetadata*const metadata_, 
                    const std::size_t opt_data)
    : time_stamp(time_stamp_), data(data_), threadId(threadId_),
      ptr(ptr_), metadata(metadata_), _OPT_DATA(opt_data)
  {}

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__ __device__ 
  inline bool empty() const { return data == InstrData::NEVER; }

  // Explicit operators
  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __device__ __host__
  inline TraceData& operator = (const TraceData& other) = default;

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __device__ __host__
  inline TraceData& operator = (TraceData&& other) = default; 

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]] 
  __host__
  explicit operator bool () const { return data != InstrData::NEVER; }

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]]
  __host__
  inline bool operator < (const TraceData& other) const {
    if (time_stamp < other.time_stamp)
      return true;
    else if ((time_stamp == other.time_stamp) // if they are equal in time
              && ((data & scabbard::InstrData::ON_CPU) // only true if l is on cpu and r is on gpu
                  && (other.data & scabbard::InstrData::ON_GPU))) 
        return true;
    return false;
  }

  [[clang::disable_sanitizer_instrumentation, gnu::flatten, gnu::always_inline]]
  __host__
  inline friend bool operator < (const TraceData& l, const TraceData& r) {
    if (l.time_stamp < r.time_stamp)
      return true;
    else if ((l.time_stamp == r.time_stamp) // if they are equal in time
              && ((l.data & scabbard::InstrData::ON_CPU) // only true if l is on cpu and r is on gpu
                  && (r.data & scabbard::InstrData::ON_GPU))) 
        return true;
    return false;
  }
};


// } //?namespace rtl
} //?namespace scabbard
