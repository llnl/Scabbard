/**
 * @file AsyncQueue.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief The cross host, device and thread lock-free queue
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once


#include "TraceWriter.hpp"
// #include "MetadataStrore.hpp"
#include "calls.hpp"
#include "DeviceTracker.hpp"

#include <scabbard/TraceData.hpp>

#include <hip/hip_ext.h>
#include <hip/hip_runtime.h>

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cinttypes>

// #define WARP_SIZE (size_t(32ul))

// #define SCABBARD_DEVICE_CYCLE_BUFFER_LANE_LENGTH ((size_t)64ul)
// #define SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT_MODIFIER ((size_t)2ul)
// #define SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT ((size_t)SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT_MODIFIER * WARP_SIZE)


namespace scabbard {
  namespace trace {

    // /**
    //  * @brief Just a lazy holder of reference pointers that will be handled with 
    //  *        external functions.
    //  */
    // struct DeviceAsyncQueue {
    //   struct Lane {
    //     _Atomic(size_t) next = 0ul;
    //     TraceData data[SCABBARD_DEVICE_CYCLE_BUFFER_LANE_LENGTH];
    //     __host__ Lane();
    //     // __device__ inline TraceData& operator [] (size_t j);
    //     __host__ inline const TraceData& operator [] (size_t j) const;
    //     friend class AsyncQueue;
    //   };
    //
    //   Lane data[SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT];
    //
    //   // __device__ inline void append(TraceData tData);
    //   __host__ DeviceAsyncQueue();
    //   // __host__ __device__ inline DeviceAsyncQueue& operator += (const TraceData& tData);
    //   // __device__ inline Lane& operator [] (size_t i);
    //   __host__ inline const Lane& operator [] (size_t i) const;
    // protected:
    //   // __device__ static inline size_t getLaneId(); // const;
    //   friend class AsyncQueue;
    //   friend __device__ void scabbard::trace::device::trace_append$mem(InstrData data, const void* PTR, const std::uint64_t* src_id, std::uint32_t line, std::uint32_t col);
    //   friend __device__ void scabbard::trace::device::trace_append$alloc(InstrData data, const void* PTR, const std::uint64_t* src_id, std::uint32_t line, std::uint32_t col, std::size_t size);
    // };
  

    /**
     * @brief A specialized AsyncQueue, that just collates the contents of 
     *        subsidiary queues that are running on separate threads and devices
     *        primarily between host and device for a CPU/GPU setup. \n
     *        NOTE: the subsidiary queues are not updated to reflect changes
     *        provided by other threads, it's a one way relationship.
     *        WARNING: this class is designed to only have one instantiation in an entire program.
     */
    class AsyncQueue {

    public:
      /// @brief a logical clock used to determine when things occur
      _Atomic(size_t) vClk = 0u;
    
    protected:
      // /// @brief host side storage of the device ptr where the device side.
      // ///        NOTE: this is a device ptr and is set during scabbard_init()
      // DeviceAsyncQueue* deviceQ = nullptr;        
      //
      // /// @brief array of the last place we read from when processing the device side async queue
      // size_t device_last_read[SCABBARD_DEVICE_CYCLE_BUFFER_LANE_COUNT];
      //
      // /// @brief aka \c _device_buffer - local place to store the device side async queue during processing
      // DeviceAsyncQueue _db;

      /// @brief the owning list of device trackers
      std::vector<device::DeviceTracker*> device_trackers;
      
      // typedef std::vector<DeviceTracker*>::iterator DTRef;

      /// @brief a map connecting counters to each streams jobs
      std::map<hipStream_t,uint16_t> stream_job_counters;

      // /// @brief a map connecting device trackers to their GPU device (non-owning)
      // ///        the result is the index in the device_tracker vector that owns the pointer
      // std::multimap<int,DTRef> dts_by_device;

      /// @brief the mutex protecting access to the device side buffer objects
      std::mutex mx_device;

      /// @brief the host buffer
      std::queue<TraceData> hostQ;

      /// @brief the mutex protecting the host side buffer (if I can find an easily imported lock free queue this will replace it)
      std::mutex mx_hostQ;

      /// @brief the output of the async queue (has ownership of the pointer)
      TraceWriter* tw = nullptr;

      /// @brief the worker thread for the async queue
      std::thread* worker_thread = nullptr;

      /// @brief way to get worker to stop so it can be joined latter
      bool run_worker = false;

      /// @brief delay between processes of the various buffers
      std::chrono::high_resolution_clock::duration delay = std::chrono::milliseconds(2);

      /// @brief where we store an manage metadata about where data comes from
      // MetadataStore metadata;
      
      
    public:

      __host__ AsyncQueue();
      __host__ ~AsyncQueue();

      /**
       * @brief start the async process
       */
      __host__ void start();

      /**
       * @brief stop the async process
       */
      __host__ void stop();


      /**
       * @brief how the host will append its traces data to the trace.
       * @param tData the trace data to append
       */
      __host__ void append(TraceData tData);

      /**
       * @brief Set the delay between processing(s) of the buffers
       * @tparam Rep - an arithmetic type representing the number of ticks
       * @tparam Period - a std::ratio representing the tick period 
       *                  (i.e. the number of second's fractions per tick)
       * @param delay_ - the new delay value
       */
      template<class Rep, class Period = std::ratio<1>>
      __host__ void set_delay(const std::chrono::duration<Rep,Period>& delay_);

      // /**
      //  * @brief Set the device queue object (takes ownership of the pointer)
      //  * @param dq_ pointer to a valid DeviceAsyncQueue located in device memory (shared mem or the device heap)
      //  */
      // __host__ void set_device_queue(DeviceAsyncQueue* dq_);

      /**
       * @brief register a job to the async queue to monitor it
       * @param DEVICE the device id associated with the job launch
       * @param STREAM pointer to the stream object associated with the job launch
       * @return \c DeviceTracker* - pointer to the device side object the kernel will work with
       */
      __host__ device::DeviceTracker* add_job(const hipStream_t STREAM);
      
      /**
       * @brief Set the trace writer object
       * @param file_path the filepath of where the tracefile will be setup
       * @param exe_path path to the instrumented executable generating the trace
       * @param start_time unix timestamp of \em roughly when this executable started running
       */
      __host__ void set_trace_writer(const std::string& file_path, const std::string& exe_path, const std::time_t start_time);

      /**
       * @brief how to trigger a single round of processing of the trace data buffered from 
       *        the device then from the host out to the TraceWriter set with \c set_trace_writer() method.
       */
      __host__ void async_process();

      /**
       * @brief called from instrumented module scabbard_ctor's to register new src files 
       * @param src - the src file being registered
       * @return \c std::uint64_t - the id that this src file should be registered with
       */
      // __host__ std::uint64_t register_src(const char* src);


    protected:

      __host__ void process_device(TraceWriter& tw);
      __host__ void process_host(TraceWriter& tw);
      

    };

    

    // class HostAsyncQueue {

    // };
  
    
  } //?namespace trace
} //?namespace scabbard
