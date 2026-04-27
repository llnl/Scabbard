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



// for getting the name of the executing program
// #ifdef __cpp_lib_filesystem
// # include <filesystem>
//   // using filesystem = std::filesystem;
//   using namespace std::filesystem;
// #else
// # include <experimental/filesystem>
//   // namespace std { namespace filesystem = std::experimental::filesystem; } //?namespace std
//   // using filesystem = std::experimental::filesystem;
//   using namespace std::experimental::filesystem;
// #endif
// // namespace { std::filesystem::path __getExePath(); } //?namespace
// // namespace { filesystem::path __getExePath(); } //?namespace
// namespace { path __getExePath(); } //?namespace


namespace scabbard {
  namespace trace {


    // << ========================================================================================== >> 
    // <<                                          GLOBALS                                           >> 
    // << ========================================================================================== >> 

    AsyncQueue TRACE_LOGGER; // created in scabbard init
    uint64_t scabbard_src_id = 0;
    

    // __device__ DeviceAsyncQueue* DEVICE_TRACE_LOGGER;



    // << ========================================================================================== >> 
    // <<                                           CALLS                                            >> 
    // << ========================================================================================== >> 

    [[clang::disable_sanitizer_instrumentation]] 
    __host__
    static inline bool ends_with(const std::string& str, const char* suffix, unsigned suffixLen)
    {
      return str.size() >= suffixLen && str.compare(str.size()-suffixLen, suffixLen, suffix, suffixLen) == 0;
    }

    [[clang::disable_sanitizer_instrumentation]] 
    __host__
    static inline bool ends_with(const std::string& str, const char* suffix)
    {
      return ends_with(str, suffix, std::string::traits_type::length(suffix));
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_init()
    {
      // if (__hip_gpubin_handle == nullptr)
      //   __hip_gpubin_handle = __hipRegisterFatBinary(__hip_fatbin_wrapper);
      // scabbard_src_id = metadata_register$src("<scabbard.trace>");
      const char* _EXE_NAME = std::getenv("SCABBARD_INSTRUMENTED_EXE_NAME");
      const std::string EXE_NAME = ((_EXE_NAME)
                                    ? std::string(_EXE_NAME)
                                    : "unknown_exe");
      const char* _TRACE_FILE = std::getenv("SCABBARD_TRACE_FILE");
      std::string trace_file = ((_TRACE_FILE) 
                                  ? std::string(_TRACE_FILE) 
                                  : "./" + EXE_NAME + ".scabbard.trace");
      const char* _IS_MPI = std::getenv("SCABBARD_IS_MULTI_NODE");
//       if (_IS_MPI)
//         trace_file += ".p" + std::to_string(getpid());

// #     ifdef SCABBARD_USE_COMPRESSION
//       if (not ends_with(trace_file,".gz")) // mark as a compressed trace file if necessary
//         trace_file += ".gz";
// #     endif
      const std::string TRACE_FILE = trace_file;
      // std::cerr << "\n[scabbard.trace:DBG] initiating scabbard trace system with details as follows"    //DEBUG
      //              "\n[scabbard.trace:DBG]   'SCABBARD_INSTRUMENTED_EXE_NAME': `" << EXE_NAME << "`"    //DEBUG
      //              "\n[scabbard.trace:DBG]              'SCABBARD_TRACE_FILE': `" << TRACE_FILE << "`"; //DEBUG
      TRACE_LOGGER.set_trace_writer(TRACE_FILE, EXE_NAME, 
                                    std::chrono::system_clock::now().time_since_epoch().count());
      // hipError_t hipRes;
      // DeviceAsyncQueue* tmp1;
      // if ((hipRes = hipMalloc(&tmp1, sizeof(DeviceAsyncQueue))) != hipSuccess) {
      //   std::cerr << "\n[scabbard::trace::init::ERROR] could not allocate space for the device side async queue! [hipError_t: " << hipRes << "]\n" 
      //             << std::endl;
      //   int device_count = 0;
      //   if (hipRes == hipErrorInvalidDevice) {
      //     if ((hipRes = hipGetDeviceCount(&device_count)) == hipSuccess)
      //       std::cerr << "\n[scabbard::trace::init::INFO] api reports " << device_count << " compatible devices on this system\n" 
      //                 << std::endl;
      //     else
      //       std::cerr << "\n[scabbard::trace::init::ERROR] could not get the number of compatible devices! [hipError_t: " << hipRes << "]\n" 
      //                 << std::endl;
      //   }
      //   exit(EXIT_FAILURE);
      // }
      // DeviceAsyncQueue tmp2; // build default config of Device async queue
      // if (hipMemcpy(tmp1, &tmp2, sizeof(DeviceAsyncQueue), hipMemcpyHostToDevice)
      //       != hipSuccess) {
      //   std::cerr << "\n[scabbard::trace::init::ERROR] failed to copy the initial device side async queue to the device(s)! [hipError_t: " << hipRes << "]\n" 
      //             << std::endl;
      //   exit(EXIT_FAILURE);
      // }
      // DEVICE_TRACE_LOGGER = tmp1;
      // TRACE_LOGGER.set_device_queue(tmp1);
      //TODO setup basics for scabbard trace
      TRACE_LOGGER.start();
    }

    [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
    __host__
    void scabbard_close()
    {
      //TODO
    }


    // [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain]] 
    // __host__
    // std::uint64_t metadata_register$src(const char* src_file) 
    // {
    //   return TRACE_LOGGER.register_src(src_file);
    // }


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
    void register_job_callback(void* dt_, hipStream_t stream, const std::uint64_t SRC_ID)
    {
      //TODO amend this to pass in instrumented src id, line and col metadata
      device::DeviceTracker* dt = (device::DeviceTracker*) dt_;
      auto hipRes = hipStreamAddCallback(stream, scabbard_stream_callback, dt, 0);
      if (hipRes != hipSuccess) {
        std::cerr << "\n[scabbard.trace:ERROR] failed to register callback on "
                     "{stream: "<< dt->JOB_ID.STREAM<< ", "
                      "job: " << dt->JOB_ID.JOB  << "}\n" << std::endl;
      }
      host::trace_append$alloc(
          (InstrData)(InstrData::LAUNCH_EVENT | InstrData::ON_HOST | InstrData::_OPT_USED),
          stream,
          SRC_ID,
          dt->JOB_ID.JOB
        );
    }

    
    
    // << ======================================== Device ========================================== >> 

    namespace device {
      //NOTE: moved to device-defs.cpp for crosswise compilation
      // __global__ 
      // void call_stub(int* data_stub)
      // {
      //   size_t i = threadIdx.x;
      //   data_stub[i*2] += data_stub[i*2+1]; 
      // }
    } //?namespace device


    // << ========================================= Host =========================================== >> 
    namespace host {

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]]
      __host__
      void trace_append$mem(const InstrData data, const void* PTR, const std::uint64_t SRC_ID)
      {
        TRACE_LOGGER.append(
            TraceData(
                TRACE_LOGGER.vClk++,
                data,
                ThreadId(), 
                PTR, 
                SRC_ID,
                0ul
              )
          );
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]] 
      __host__
      void trace_append$mem$cond(InstrData data, const void* PTR, const std::uint64_t SRC_ID)
      {
        hipPointerAttribute_t attrs;
        const auto status = hipPointerGetAttributes(&attrs,PTR);
        InstrData _data = (InstrData) (data & ((~InstrData::NONE) ^ InstrData::UNKNOWN_HEAP));
        if (status == hipSuccess) {
          if (attrs.isManaged) {
            _data |= InstrData::MANAGED_MEM;
          } else if (attrs.devicePointer == nullptr) {
            _data |= InstrData::HOST_HEAP;
          } else {
            _data |= InstrData::DEVICE_HEAP;
          }
          trace_append$mem(_data, PTR, SRC_ID);
        } else {
          std::cerr << "\n[scabbard::trace::cond::ERROR] could not get the properties of a pointer with `hipPointerGetAttributes()`!\n"
                    << std::endl;
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }

      [[clang::disable_sanitizer_instrumentation, gnu::used, gnu::retain, gnu::noinline]]
      __host__
      void trace_append$alloc(const InstrData data, const void* PTR, const std::uint64_t SRC_ID, const std::size_t SIZE)
      {
        TRACE_LOGGER.append(
            TraceData(
                TRACE_LOGGER.vClk++,
                // std::chrono::high_resolution_clock::now().time_since_epoch().count(),
                (data | InstrData::_OPT_USED),
                ThreadId(),
                PTR,
                SRC_ID,
                SIZE
              )
          );
      }

      void trace_append$alloc$cond(const InstrData data, const void* PTR, const std::uint64_t SRC_ID, const std::size_t SIZE)
      {
        hipPointerAttribute_t attrs;
        const auto status = hipPointerGetAttributes(&attrs,PTR);
        InstrData _data = (InstrData)(data & ((~InstrData::NONE) ^ InstrData::UNKNOWN_HEAP));
        if (status == hipSuccess) {
          if (attrs.isManaged) {
            _data |= InstrData::MANAGED_MEM;
          } else if (attrs.devicePointer == nullptr) {
            _data |= InstrData::HOST_HEAP;
          } else {
            _data |= InstrData::DEVICE_HEAP;
          }
          trace_append$alloc(_data, PTR, SRC_ID, SIZE);
        } else {
          std::cerr << "\n[scabbard::trace::cond::ERROR] could not get the properties of a pointer with `hipPointerGetAttributes()`!\n"
                    << std::endl;
#         ifdef DEBUG
            exit(EXIT_FAILURE);
#         endif
        }
      }
      
    } // namespace host


  } //?namespace trace
} //?namespace scabbard


// // for getting the name of the executing program
// #ifdef _WIN32
// # include <windows.h>    //GetModuleFileNameW
// #else
// # include <limits.h>
// # include <unistd.h>     //readlink
// #endif

// // for getting the name of the executing program
// namespace { 
//   path __getExePath()
//   {
// #   ifdef _WIN32
//       wchar_t _path[MAX_PATH] = { 0 };
//       GetModuleFileNameW(NULL, _path, MAX_PATH);
//       return path(_path);
// #   else
//       char result[PATH_MAX];
//       ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
//       return path(std::string(result, (count > 0) ? count : 0));
//     #endif
//   }

// } //?namespace
