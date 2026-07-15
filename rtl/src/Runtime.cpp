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

#include <scabbard/rtl/Runtime.hpp>
#include <scabbard/rtl/globals.hpp>
#include <scabbard/rtl/ReportWriter.hpp>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>

#include <cstring>
#include <deque>
#include <thread>


namespace scabbard {
  namespace rtl {


    // << ========================================================================================== >> 
    // <<                                     MAIN ASYNC QUEUE                                       >> 
    // << ========================================================================================== >> 


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    Runtime::Runtime() {}

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    Runtime::~Runtime()
    {
      stop();
      for (auto dt : device_trackers)
        if (dt != nullptr)
          if (hipFree(dt) != hipSuccess)
            SCAB_SERR << "\n[scabbard.rtl.dtor:ERROR] could not deallocate a device side buffer!\n" 
                      << endl();
      if (SM) delete SM;
      if (GPF) delete GPF;
    }


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::start()
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
    void Runtime::stop()
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
    void Runtime::set_delay(const std::chrono::duration<Rep,Period>& delay_)
    {
      delay = delay_;
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::initialize(std::size_t mem_chunk_len)
    {
      if (SM != nullptr) { // case reinitialization
        SM->reset();
        if (GPF) delete GPF;
      } else
        SM = new StateMachine();
      GPF = new GroupedPtrFactory<const TraceData>(mem_chunk_len);
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::report()
    {
      print_report(SM->get_results());
    }


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    device::DeviceTracker* Runtime::add_job(const hipStream_t STREAM)
    {
      device::DeviceTracker* dt = nullptr;
      hipError_t hipRes = hipMallocManaged(&dt, 
                                          device::DeviceTracker::getAllocSizeBytes(),
                                          hipMemAttachGlobal);
      // hipError_t hipRes = hipHostMalloc(&dt, sizeof(device::DeviceTracker), hipHostMallocPortable);
      if (hipRes != hipSuccess) {
        SCAB_SERR << "\033[91m\n[scabbard.rtl:ERROR] failed to allocate managed memory before kernel launch!\033[00m\n" << endl();
        exit(EXIT_FAILURE);
      }
      auto it = stream_job_counters.find(STREAM);
      if (it == stream_job_counters.end())
        it = stream_job_counters.emplace(std::make_pair(STREAM,0u)).first;
      // in place construction into allocated managed memory
      new (dt) device::DeviceTracker(jobId_t(++it->second,STREAM), vClk);
      mx_device.lock();
      device_trackers.push_back(dt);
      mx_device.unlock();
      it->second++; 
      return dt;
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::append(TraceData&& tData)
    {
      mx_host.lock();
      hostQ.push(std::move(GPF->create(tData)));
      mx_host.unlock();
    }



    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::async_process()
    {
      static std::size_t cycle_count = 0ull;
      process_device();
      process_host();
      if (cycle_count++ % 8u == 0u) {
        mx_device.lock(); // NOTE: might disrupt validity to freeze host and device here.
        mx_host.lock();
        SM->run(2u);
        mx_host.unlock();
        mx_device.unlock();
      }
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::process_device()
    {
      using namespace scabbard::rtl::device;
      mx_device.lock();
      for (auto dt : device_trackers) {
        if (dt == nullptr) continue;
        const size_t NEXT = dt->next; // get copy of atomic value to skip atomic reads since the buffer is frozen
        const size_t TRUE_SPAN = NEXT - dt->next_read;
        const size_t SPAN = (TRUE_SPAN < DeviceTracker::BUFFER_SIZE) ? TRUE_SPAN : DeviceTracker::BUFFER_SIZE;
        const size_t MAX = dt->next_read + DeviceTracker::BUFFER_SIZE;
        for (size_t i = dt->next_read; i < MAX && i < NEXT; ++i)
          SM->append(std::move(GPF->create(dt->buffer[i%DeviceTracker::BUFFER_SIZE])));
        dt->next_read = NEXT;
        if (TRUE_SPAN)
          SCAB_SERR << "\n[scabbard.rtl:INFO] reading " << SPAN << '/' << TRUE_SPAN << " data points from GPU s:" << dt->JOB_ID.STREAM << " j:" << dt->JOB_ID.JOB << flush();
        if (TRUE_SPAN > SPAN)
          SCAB_SERR << "\n\033[33m[scabbard.rtl:WARN] " << TRUE_SPAN - SPAN << " data points lost from GPU s:" << dt->JOB_ID.STREAM << " j:" << dt->JOB_ID.JOB << "\033[00m" << flush();
        if (dt->finished) { // deal with a device tracker that is done with it's job
          auto hipRes = hipFree(dt);
          if (hipRes != hipSuccess)
            SCAB_SERR << "\n\033[91m[scabbard.rtl:ERROR] failed to free a device tracker from managed memory! (errcode: " << hipRes << ")\033[00m\n";
          dt = nullptr;
        }
      }
      mx_device.unlock();
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    __host__
    void Runtime::process_host()
    {
      mx_host.lock();
      while (not hostQ.empty()) {
        SM->append(hostQ.front());
        hostQ.pop();
      }
      mx_host.unlock();
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
  
  
  } //?namespace rtl
} //?namespace scabbard
