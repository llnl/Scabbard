/**
 * @file calls.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief implementation of the scabbard/rtl/calls.hpp 
 *        and the scabbard/rtl/globals.hpp include files
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <scabbard/rtl/calls.hpp>
#include <scabbard/rtl/globals.hpp>
#include <scabbard/rtl/Runtime.hpp>
#include <scabbard/rtl/StateMachine.hpp>
#include <scabbard/rtl/ReportWriter.hpp>
#include <scabbard/Metadata.hpp>

#include <hip/hip_ext.h>
#include <hip/hip_runtime_api.h>

#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>



namespace scabbard {
  namespace rtl {


    // << ========================================================================================== >> 
    // <<                                          GLOBALS                                           >> 
    // << ========================================================================================== >> 

    Runtime SCAB_RUNTIME; // initialized in scabbard init
    scabbard::rtl::ostream SCAB_SOUT(std::cout);
    scabbard::rtl::ostream SCAB_SERR(std::cerr);



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
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_DEVICE_BUFFER_SIZE` was not a positive integer!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        } catch (std::out_of_range& oor) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_DEVICE_BUFFER_SIZE` was too large (or small)!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        } catch (std::exception& ex) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Exception while parsing the value of `$SCABBARD_DEVICE_BUFFER_SIZE`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << "\""
                       "\n[scabbard.rtl.init:ERROR]   Error Message: \"" << ex.what() << '\"' << endl();
          std::exit(EXIT_FAILURE);
        } catch (...) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Unknown Error parsing the value of `$SCABBARD_DEVICE_BUFFER_SIZE`!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        }
        if (device::DeviceTracker::BUFFER_SIZE < 4096ull 
            || device::DeviceTracker::BUFFER_SIZE > 4294967296ull) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_DEVICE_BUFFER_SIZE` value not in range (min:4096,max:4294967296)"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_DEVICE_BUFFER_SIZE: \""<< _DEVICE_BUFFER_SIZE << "\""
                       "\n[scabbard.rtl.init:ERROR]                     Evaluated To:  "
                    << device::DeviceTracker::BUFFER_SIZE << endl();
          std::exit(EXIT_FAILURE);
        }
      }
      const char* _MEM_CHUNK_SIZE = std::getenv("SCABBARD_RTL_MEM_CHUNK_LEN");
      std::size_t MEM_CHUNK_SIZE = 64ull;
      if (_MEM_CHUNK_SIZE) {
        try {
          MEM_CHUNK_SIZE = std::stoull(_MEM_CHUNK_SIZE);
        } catch (std::invalid_argument& ia) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_RTL_MEM_CHUNK_LEN` was not a positive integer!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_RTL_MEM_CHUNK_LEN: \""<< _MEM_CHUNK_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        } catch (std::out_of_range& oor) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_RTL_MEM_CHUNK_LEN` was too large (or small)!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_RTL_MEM_CHUNK_LEN: \""<< _MEM_CHUNK_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        } catch (std::exception& ex) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Exception occurred while parsing the value of `$SCABBARD_RTL_MEM_CHUNK_LEN`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_RTL_MEM_CHUNK_LEN: \""<< _MEM_CHUNK_SIZE << "\""
                       "\n[scabbard.rtl.init:ERROR]   Error Message: \"" << ex.what() << '\"' << endl();
          std::exit(EXIT_FAILURE);
        } catch (...) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Unknown Error parsing the value of `$SCABBARD_RTL_MEM_CHUNK_LEN`!"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_RTL_MEM_CHUNK_LEN: \""<< _MEM_CHUNK_SIZE << '"'
                    << endl();
          std::exit(EXIT_FAILURE);
        }
        if (MEM_CHUNK_SIZE < 8ull || MEM_CHUNK_SIZE > 4096ull<<1ull) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_RTL_MEM_CHUNK_LEN` value not in range (min: 8, max: 4096)"
                       "\n[scabbard.rtl.init:ERROR]     $SCABBARD_RTL_MEM_CHUNK_LEN: \""<< _MEM_CHUNK_SIZE << "\""
                       "\n[scabbard.rtl.init:ERROR]                    Evaluated To:  "<< MEM_CHUNK_SIZE << endl();
          std::exit(EXIT_FAILURE);
        }
      }

      const char* _STDOUT = std::getenv("SCABBARD_RTL_STDOUT");
      std::ofstream* std_out = nullptr;
      if (_STDOUT) {
        try {
          std_out = new std::ofstream(_STDOUT);
        } catch (std::exception ex) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Error opening file for `$SCABBARD_RTL_STDOUT`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_RTL_STDOUT: \""<< _STDOUT << "\""
                       "\n[scabbard.rtl.init:ERROR]   Error Message: \"" << ex.what() << '\"' << endl();
          if (std_out) { delete std_out; std_out = nullptr; }
          std::exit(EXIT_FAILURE);
        } catch (...) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] UNKOWN Error opening file for `$SCABBARD_RTL_STDOUT`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_RTL_STDOUT: \""<< _STDOUT << "\""
                    << endl();
          if (std_out) { delete std_out; std_out = nullptr; }
          std::exit(EXIT_FAILURE);
        }
      }
      const char* _STDERR = std::getenv("SCABBARD_RTL_STDERR");
      std::ofstream* std_err = nullptr;
      if (_STDERR) {
        try {
          std_err = new std::ofstream(_STDERR);
        } catch (std::exception ex) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] Error opening file for `$SCABBARD_RTL_STDERR`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_RTL_STDERR: \""<< _STDERR << "\""
                       "\n[scabbard.rtl.init:ERROR]   Error Message: \"" << ex.what() << '\"' << endl();
          if (std_err) { delete std_err; std_err = nullptr; }
          std::exit(EXIT_FAILURE);
        } catch (...) {
          SCAB_SERR << "\n[scabbard.rtl.init:ERROR] UNKOWN Error opening file for `$SCABBARD_RTL_STDERR`!"
                       "\n[scabbard.rtl.init:ERROR]   $SCABBARD_RTL_STDERR: \""<< _STDERR << "\""
                    << endl();
          if (std_err) { delete std_err; std_err = nullptr; }
          std::exit(EXIT_FAILURE);
        }
      }
      // if both SCAB_SOUT and SCAB_SERR are redirected; ensure they don't redirect to the same place.
      if (std_out && std_err  
          && std::filesystem::equivalent(std::filesystem::absolute(std::filesystem::path(_STDOUT)),
                                         std::filesystem::absolute(std::filesystem::path(_STDERR)))) {
        SCAB_SERR << "\n[scabbard.rtl.init:ERROR] `$SCABBARD_RTL_STDOUT` and `$SCABBARD_RTL_STDERR` can NOT redirect to the same file!"
                  << endl();
        if (std_out->is_open()) std_out->close(); delete std_out;
        if (std_err->is_open()) std_err->close(); delete std_err;
        std::exit(EXIT_FAILURE);
      }

      SCAB_RUNTIME.initialize(MEM_CHUNK_SIZE);

      if (std_out) SCAB_SOUT.replace(std_out);
      if (std_err) SCAB_SERR.replace(std_err);

      SCAB_RUNTIME.start();
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_close()
    {
      SCAB_RUNTIME.stop();
      SCAB_RUNTIME.report();
      SCAB_RUNTIME.finalize();
    }

#   ifndef hipStreamLegacy
#   define hipStreamLegacy ((hipStream_t)1u)
#   endif
#   ifndef hipStreamPerThread
#   define hipStreamPerThread ((hipStream_t)2u)
#   endif


    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void* register_job(hipStream_t STREAM)
    {
      if (STREAM == nullptr)
        STREAM = (hipStream_t) DEFAULT_STREAM_BEHAVIOR();
      if (STREAM == hipStreamLegacy) //TODO: double check this still works later
        return ((void*) SCAB_RUNTIME.add_job(nullptr));
      if (STREAM == hipStreamPerThread)
        return ((void*) SCAB_RUNTIME.add_job((hipStream_t)std::hash<std::thread::id>()(std::this_thread::get_id())));
      return ((void*) SCAB_RUNTIME.add_job(STREAM));
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_stream_callback(hipStream_t stream, hipError_t status, void* dt_)
    {
      // mark the device tracker as finished for the SCAB_RUNTIME to take care of during next device upkeep cycle
      device::DeviceTracker* dt = (device::DeviceTracker*) dt_;
      dt->finished = true;
      // rebalance logical vClk
      const size_t dvClk = dt->vClk;
      if (dvClk > SCAB_RUNTIME.vClk)
        SCAB_RUNTIME.vClk = dvClk;
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void register_job_callback(void* dt_, hipStream_t stream, const void* const SRC_ID)
    {
      device::DeviceTracker* dt = (device::DeviceTracker*) dt_;
      auto hipRes = hipStreamAddCallback(stream, scabbard_stream_callback, dt, 0u);
      if (hipRes != hipSuccess) {
        SCAB_SERR << "\n[scabbard.rtl:ERROR] failed to register callback on "
                     "{stream: "<< jobId_t::hash_stream_ptr(stream) << ", "
                      "job: " << dt->JOB_ID.JOB  << "}\n" << endl();
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
        SCAB_RUNTIME.append(
            TraceData(
                SCAB_RUNTIME.vClk++,
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
              SCAB_SERR << "\n[scabbard.rtl.cond:WARNING] Scabbard does not support array memory results may be invalid!"
                           "\n[scabbard.rtl.cond:WARNING]   location: " << *((SrcMetadata*) SRC_ID) << endl();
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
          SCAB_SERR << "\n[scabbard.rtl.cond:ERROR] Could not get the properties of a pointer with `hipPointerGetAttributes()`!"
                       "\n[scabbard.rtl.cond:ERROR]   location: " << *((SrcMetadata*) SRC_ID) << endl();
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]]
      __host__
      void trace_append$alloc(const InstrData data, const void*const PTR, const void* const SRC_ID, const std::size_t SIZE)
      {
        SCAB_RUNTIME.append(
            TraceData(
                SCAB_RUNTIME.vClk++,
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
              SCAB_SERR << "\n[scabbard.rtl.cond:WARNING] Scabbard does not support array memory results may be invalid!"
                           "\n[scabbard.rtl.cond:WARNING]   location: " << *((SrcMetadata*) SRC_ID) << endl();
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
          SCAB_SERR << "\n[scabbard.rtl.cond:ERROR] Could not get the properties of a pointer with `hipPointerGetAttributes()`!"
                       "\n[scabbard.rtl.cond:ERROR]   location: " << *((SrcMetadata*) SRC_ID) << endl();
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }
      
    } // namespace host


  } //?namespace rtl
} //?namespace scabbard
