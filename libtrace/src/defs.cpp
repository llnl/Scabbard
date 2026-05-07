/**
 * @file calls.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief implementation of the scabbard/trace/calls.hpp 
 *        and the scabbard/trace/globals.hpp include files
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <scabbard/trace/calls.hpp>
#include <scabbard/trace/globals.hpp>
#include <scabbard/trace/AsyncQueue.hpp>
#include <scabbard/trace/TraceWriter.hpp>
#include <scabbard/Metadata.hpp>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>

#include <thread>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <iostream>
#include <string>



namespace scabbard {
  namespace trace {


    // << ========================================================================================== >> 
    // <<                                          GLOBALS                                           >> 
    // << ========================================================================================== >> 

    AsyncQueue TRACE_LOGGER; // initialized in scabbard init



    // << ========================================================================================== >> 
    // <<                                           CALLS                                            >> 
    // << ========================================================================================== >> 

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_init()
    {
      const char* _EXE_NAME = std::getenv("SCABBARD_INSTRUMENTED_EXE_NAME");
      const std::string EXE_NAME = ((_EXE_NAME)
                                    ? std::string(_EXE_NAME)
                                    : "unknown_exe");
      const char* _DEVICE_BUFFER_SIZE = std::getenv("SCABBARD_DEVICE_BUFFER_SIZE");
      if (_DEVICE_BUFFER_SIZE) {
        try {
          device::DeviceTracker::BUFFER_SIZE = std::stoull(_DEVICE_BUFFER_SIZE);
        } catch (std::invalid_argument& ia) {
          std::cerr << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_DEVICE_BUFFER_SIZE` was not a positive integer!" << std::endl;
          std::exit(EXIT_FAILURE);
        } catch (std::out_of_range& oor) {
          std::cerr << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_DEVICE_BUFFER_SIZE` was too large (or small)!" << std::endl;
          std::exit(EXIT_FAILURE);
        } catch (std::exception& ex) {
          std::cerr << "\n[scabbard.rtl.init:ERROR] Unknown error parsing the value of `$SCABBARD_DEVICE_BUFFER_SIZE`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << "\""
                       "\n[scabbard.rtl.init:ERROR]   Error Message: \"" << ex.what() << '\"' << std::endl;
          std::exit(EXIT_FAILURE);
        } catch (...) {
          std::cerr << "\n[scabbard.rtl.init:ERROR] Unknown error parsing the value of `$SCABBARD_DEVICE_BUFFER_SIZE`!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << '"'
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
      }

      TRACE_LOGGER.set_trace_writer(TRACE_FILE, EXE_NAME, 
                                    std::chrono::system_clock::now().time_since_epoch().count());
      TRACE_LOGGER.start();
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_close()
    {
      //TODO if any cleanup needs to be added put it here.
    }



    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void* register_job(const hipStream_t STREAM)
    {
      return ((void*) TRACE_LOGGER.add_job(STREAM));
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_stream_callback(hipStream_t stream, hipError_t status, void* dt_)
    {
      // mark the device tracker as finished for the TRACE_LOGGER to take care of during next device upkeep cycle
      device::DeviceTracker* dt = (device::DeviceTracker*) dt_;
      dt->finished = true;
      // rebalance logical vClk
      const size_t dvClk = dt->vClk;
      if (dvClk > TRACE_LOGGER.vClk)
        TRACE_LOGGER.vClk = dvClk;
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void register_job_callback(void* dt_, hipStream_t stream, const void* const SRC_ID)
    {
      device::DeviceTracker* dt = (device::DeviceTracker*) dt_;
      auto hipRes = hipStreamAddCallback(stream, scabbard_stream_callback, dt, 0);
      if (hipRes != hipSuccess) {
        std::cerr << "\n[scabbard.trace:ERROR] failed to register callback on "
                     "{stream: "<< dt->JOB_ID.STREAM<< ", "
                      "job: " << dt->JOB_ID.JOB  << "}\n" << std::endl;
      }
      host::trace_append$alloc(
          (InstrData)(LAUNCH_EVENT | ON_HOST | _OPT_USED),
          stream,
          SRC_ID,
          dt->JOB_ID.JOB
        );
    }

    
    
    // << ======================================== Device ========================================== >> 

    namespace device {
      //NOTE: moved to device-defs.cpp for crosswise compilation
    } //?namespace device


    // << ========================================= Host =========================================== >> 
    namespace host {

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]]
      __host__
      void trace_append$mem(const InstrData data, const void*const PTR, const void* const SRC_ID)
      {
        TRACE_LOGGER.append(
            TraceData(
                TRACE_LOGGER.vClk++,
                data,
                ThreadId(), 
                (std::uintptr_t)PTR,
                (SrcMetadata*)SRC_ID,
                0ull
              )
          );
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
      __host__
      void trace_append$mem$cond(InstrData data, const void*const PTR, const void* const SRC_ID)
      {
        hipPointerAttribute_t attrs;
        const auto status = hipPointerGetAttributes(&attrs,PTR);
        if (status == hipSuccess) {
          switch (attrs.type) {
            case hipMemoryTypeArray:
              std::cerr << "\n[scabbard.rtl.cond:WARNING] Scabbard does not support array memory results may be invalid!"
                           "\n[scabbard.rtl.cond:WARNING]   location: " << *((SrcMetadata*) SRC_ID) << std::endl;
            case hipMemoryTypeUnregistered:
            return;
            case hipMemoryTypeHost:
            case hipMemoryTypeDevice:
            case hipMemoryTypeManaged:
            case hipMemoryTypeUnified:
              trace_append$mem(data, PTR, SRC_ID);
              return;
            }
        } else {
          std::cerr << "\n[scabbard.rtl.cond:ERROR] Could not get the properties of a pointer with `hipPointerGetAttributes()`!"
                       "\n[scabbard.rtl.cond:ERROR]   location: " << *((SrcMetadata*) SRC_ID) << std::endl;
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]]
      __host__
      void trace_append$alloc(const InstrData data, const void*const PTR, const void* const SRC_ID, const std::size_t SIZE)
      {
        TRACE_LOGGER.append(
            TraceData(
                TRACE_LOGGER.vClk++,
                (data | InstrData::_OPT_USED),
                ThreadId(),
                (std::uintptr_t)PTR,
                (SrcMetadata*)SRC_ID,
                SIZE
              )
          );
      }

      void trace_append$alloc$cond(const InstrData data, const void*const PTR, const void* const SRC_ID, const std::size_t SIZE)
      {
        hipPointerAttribute_t attrs;
        const auto status = hipPointerGetAttributes(&attrs,PTR);
        if (status == hipSuccess) {
          switch (attrs.type) {
            case hipMemoryTypeArray:
              std::cerr << "\n[scabbard.rtl.cond:WARNING] Scabbard does not support array memory results may be invalid!"
                           "\n[scabbard.rtl.cond:WARNING]   location: " << *((SrcMetadata*) SRC_ID) << std::endl;
            case hipMemoryTypeUnregistered:
              return;
            case hipMemoryTypeHost:
            case hipMemoryTypeDevice:
            case hipMemoryTypeManaged:
            case hipMemoryTypeUnified:
              trace_append$alloc(data, PTR, SRC_ID, SIZE);
              return;
            }
        } else {
          std::cerr << "\n[scabbard.rtl.cond:ERROR] Could not get the properties of a pointer with `hipPointerGetAttributes()`!"
                       "\n[scabbard.rtl.cond:ERROR]   location: " << *((SrcMetadata*) SRC_ID) << std::endl;
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }
      
    } // namespace host


  } //?namespace trace
} //?namespace scabbard
