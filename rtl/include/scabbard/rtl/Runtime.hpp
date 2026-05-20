/**
 * @file Runtime.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief The cross host, device and thread lock-free queue
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once


#include "calls.hpp"
#include "DeviceTracker.hpp"

#include <scabbard/TraceData.hpp>
#include <scabbard/rtl/GroupedPtr.hpp>
#include <scabbard/rtl/StateMachine.hpp>

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



namespace scabbard {
  namespace rtl {

  

    /**
     * @brief A specialized Runtime, that just collates the contents of 
     *        subsidiary queues that are running on separate threads and devices
     *        primarily between host and device for a CPU/GPU setup. \n
     *        NOTE: the subsidiary queues are not updated to reflect changes
     *        provided by other threads, it's a one way relationship.
     *        WARNING: this class is designed to only have one instantiation in an entire program.
     */
    class Runtime {

    public:
      /// @brief a logical clock used to determine when things occur
      std::atomic<size_t> vClk = 0u;
    
    protected:
      
      /// @brief Pointer to the state machine that validates the process.
      StateMachine* SM = nullptr;

      /// @brief owner of all \c GroupedPtrs in the Runtime.
      GroupedPtrFactory<const TraceData>* GPF = nullptr;

      /// @brief the owning list of device trackers
      std::vector<device::DeviceTracker*> device_trackers;

      /// @brief a map connecting counters to each stream's jobs
      std::map<hipStream_t,uint16_t> stream_job_counters;

      /// @brief the mutex protecting access to the device side volatiles
      std::mutex mx_device;

      /// @brief the host buffer
      std::queue<GroupedPtr<const TraceData>> hostQ;

      /// @brief the mutex protecting the host side volatiles
      std::mutex mx_host;

      /// @brief the worker thread for the async queue
      std::thread* worker_thread = nullptr;

      /// @brief way to get worker to stop so it can be joined latter
      std::atomic<bool> run_worker = false;

      /// @brief delay between processes of the various buffers
      std::chrono::high_resolution_clock::duration delay = std::chrono::milliseconds(2);

      /// @brief where we store an manage metadata about where data comes from
      // MetadataStore metadata;
      
      
    public:

      __host__ Runtime();
      __host__ ~Runtime();

      /**
       * @brief start the runtime
       */
      __host__ void start();

      /**
       * @brief stop the runtime
       */
      __host__ void stop();


      /**
       * @brief how the host will append its traces data to the rtl.
       * @param tData the rtl data to append (moved not copied)
       */
      __host__ void append(TraceData&& tData);

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
       * @brief initialize the runtime
       * @param mem_chunk_len size to set chunk size to in the GPF
       */
      __host__
      void initialize(std::size_t mem_chunk_len);

      /**
       * @brief Produce a report to the global \c SCAB_SOUT with the results produced by the 
       *        \c StateMachine so far.
       */
      __host__
      void report();

      /**
       * @brief Perform any final actions necessary before a deconstructor is called for final cleanup.
       */
      __host__
      void finalize() {}

      /**
       * @brief how to trigger a single round of processing of the rtl data buffered from 
       *        the device then from the host out to the TraceWriter set with \c initialize() method.
       */
      __host__ void async_process();


    protected:

      __host__ void process_device();
      __host__ void process_host();
      

    };

    

    // class HostAsyncQueue {

    // };
  
    
  } //?namespace rtl
} //?namespace scabbard
