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

#include <scabbard/trace/AsyncQueue.hpp>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>

#include <cstring>
#include <iostream>
#include <deque>
#include <thread>


namespace scabbard {
  namespace trace {


    // << ========================================================================================== >> 
    // <<                                     MAIN ASYNC QUEUE                                       >> 
    // << ========================================================================================== >> 


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    AsyncQueue::AsyncQueue() {}

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    AsyncQueue::~AsyncQueue()
    {
      stop();
      for (auto dt : device_trackers)
        if (dt != nullptr)
          if (hipFree(dt) != hipSuccess)
            std::cerr << "\n[scabbard.trace.dtor:ERROR] could not deallocate device side buffer!\n" 
                      << std::endl;
      if (tw != nullptr) {
        tw->finalize();
        tw->close();
        delete tw;
      }
    }


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::start()
    {
      stop();
      run_worker = true;
      worker_thread = new std::thread([&,this]() -> void {
                                        while (this->run_worker) {
                                          std::this_thread::sleep_for(this->delay);
                                          this->async_process();
                                        }
                                      });
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::stop()
    {
      if (worker_thread != nullptr) {
        run_worker = false;
        worker_thread->join();
        delete worker_thread;
      }
    }

    template<class Rep, class Period>
    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::set_delay(const std::chrono::duration<Rep,Period>& delay_)
    {
      delay = delay_;
    }


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::set_trace_writer(const std::string& file_path, const std::string& exe_path, std::time_t start_time)
    {
      if (tw != nullptr) {
        tw->finalize();
        tw->close();
        delete tw;
      }
      try {
        tw = new TraceWriter(file_path);
      } catch (std::exception ex) {
        std::cerr << "\n[scabbard.trace:ERR] Could not open trace file!"
                     "\n[scabbard.trace:ERR]          error: \"" << ex.what() << "\"" 
                     "\n[scabbard.trace:ERR]     trace file: \"" << file_path << "\"\n";
        exit(EXIT_FAILURE);
      } catch (...) {
        std::cerr << "\n[scabbard.trace:ERR] Could not open trace file!"
                     "\n[scabbard.trace:ERR]          error: \"<UNKNOWN_ERROR>\"" 
                     "\n[scabbard.trace:ERR]     trace file: \"" << file_path << "\"\n";
        exit(EXIT_FAILURE);
      }
      try {
        tw->init(exe_path, start_time);
      } catch (std::exception ex) {
        std::cerr << "\n[scabbard.trace:ERR] Could not write header for trace file!"
                     "\n[scabbard.trace:ERR]          error: \"" << ex.what() << "\"" 
                     "\n[scabbard.trace:ERR]     trace file: \"" << file_path << "\"\n";
        exit(EXIT_FAILURE);
      } catch (...) {
        std::cerr << "\n[scabbard.trace:ERR] Could not write header for trace file!"
                     "\n[scabbard.trace:ERR]          error: \"<UNKNOWN_ERROR>\"" 
                     "\n[scabbard.trace:ERR]     trace file: \"" << file_path << "\"\n";
        exit(EXIT_FAILURE);
      }
    }


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    device::DeviceTracker* AsyncQueue::add_job(const hipStream_t STREAM)
    {
      device::DeviceTracker* dt = nullptr;
      hipError_t hipRes = hipMallocManaged(&dt, 
                                          device::DeviceTracker::getAllocSizeBytes(),
                                          hipMemAttachGlobal);
      // hipError_t hipRes = hipHostMalloc(&dt, sizeof(device::DeviceTracker), hipHostMallocPortable);
      if (hipRes != hipSuccess) {
        std::cerr << "\n[scabbard.trace:ERROR] failed to allocate managed memory before kernel launch!\n" << std::endl;
        exit(EXIT_FAILURE);
      }
      mx_device.lock();
      auto tmp = stream_job_counters.find(STREAM);
      if (tmp == stream_job_counters.end())
        stream_job_counters.emplace(std::make_pair(STREAM,0u));
      // in place construction into allocated managed memory
      new (dt) device::DeviceTracker(jobId_t(tmp->second,STREAM), vClk);
      device_trackers.push_back(dt);
      stream_job_counters[STREAM] = tmp->second+1u; 
      mx_device.unlock();
      return dt;
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::append(TraceData tData)
    {
      mx_hostQ.lock();
      hostQ.push(tData);
      mx_hostQ.unlock();
    }



    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::async_process()
    {
      process_device(*tw);
      process_host(*tw);
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::process_device(TraceWriter& tw)
    {
      using namespace scabbard::trace::device;
      mx_device.lock();
      for (auto dt : device_trackers) {
        if (dt == nullptr) continue;
        const size_t NEXT = dt->next; // get copy of atomic value to skip atomic reads since the buffer is frozen
        const size_t TRUE_SPAN = NEXT - dt->next_read;
        const size_t SPAN = (TRUE_SPAN < DeviceTracker::BUFFER_SIZE) ? TRUE_SPAN : DeviceTracker::BUFFER_SIZE;
        const size_t MAX = dt->next_read + DeviceTracker::BUFFER_SIZE;
        for (size_t i = dt->next_read; i < MAX && i < NEXT; ++i)
          tw << dt->buffer[i%DeviceTracker::BUFFER_SIZE];
        dt->next_read = NEXT;
        if (TRUE_SPAN)
          std::cerr << "[scabbard.trace:INFO] reading " << SPAN << '/' << TRUE_SPAN << " data points from GPU s:" << dt->JOB_ID.STREAM << " j:" << dt->JOB_ID.JOB << std::endl;
        if (dt->finished) { // deal with a device tracker that is done with it's job
          auto hipRes = hipFree(dt);
          if (hipRes != hipSuccess)
            std::cerr << "\n[scabbard.trace:ERR] failed to free a device tracker from managed memory! (errcode: " << hipRes << ")\n";
          dt = nullptr;
        }
      }
      mx_device.unlock();
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void AsyncQueue::process_host(TraceWriter& tw)
    {
      mx_hostQ.lock();
      while (not hostQ.empty()) {
        tw << hostQ.front();
        hostQ.pop();
      }
      mx_hostQ.unlock();
    }


    // << ========================================================================================== >> 
    // <<                                    DEVICE ASYNC QUEUE                                      >> 
    // << ========================================================================================== >> 

    namespace device {
      std::size_t DeviceTracker::BUFFER_SIZE = SCABBARD_DEVICE_TRACKER_BUFF_DEFAULT_SIZE;

      __host__
      inline size_t DeviceTracker::getAllocSizeBytes()
      {
        return sizeof(DeviceTracker) + DeviceTracker::getBuffAllocSizeBytes();
      }

      __host__
      inline size_t DeviceTracker::getBuffAllocSizeBytes()
      {
        return (size_t)((sizeof(TraceData)*(DeviceTracker::BUFFER_SIZE+3ull))/((size_t)__WORDSIZE/8ull));
      }
    } //?namespace device
  
  
  } //?namespace trace
} //?namespace scabbard
