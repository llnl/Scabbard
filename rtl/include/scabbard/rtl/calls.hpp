/**
 * @file calls.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Where the instrumentation calls progenitors are defined
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <scabbard/Enums.hpp>
#include <scabbard/instr-names.def>

#include <hip/hip_runtime.h>
#include <hip/hip_runtime_api.h>


namespace scabbard {
  namespace rtl {
  
    /**
     * @brief setup the scabbard system.
     *        Should be called from main at the start of the program.
     *        Scabbard instrumentation should do this automatically.
     */
    __host__ 
    void scabbard_init() asm (SCABBARD_CALLBACK_INIT_NAME);

    /**
     * @brief finalize and finish up all scabbard operations for program ending.
     *        Should be called from somewhere at the end of the program. \n
     *        ( \em NOTE: this description of where it goes is purposefully unhelpful,
     *          because I don't currently use this feature.)
     */
    __host__ 
    void scabbard_close() asm (SCABBARD_CALLBACK_CLOSE_NAME);


    namespace device {
      
      __device__
      void trace_append$mem(void* deviceTracker, const InstrData data, const void*const PTR, const void*const SRC_ID) 
        asm (SCABBARD_DEVICE_CALLBACK_APPEND_MEM_NAME);
      __device__ 
      void trace_append$alloc(void* deviceTracker, const InstrData data, const void*const PTR, const void*const SRC_ID, const std::size_t SIZE) 
        asm (SCABBARD_DEVICE_CALLBACK_APPEND_ALLOC_NAME);


    } //?namespace device

    namespace host {

      __host__ 
      void trace_append$mem(const InstrData data, const void*const PTR, const void* const SRC_ID) 
        asm (SCABBARD_HOST_CALLBACK_APPEND_MEM_NAME);
      __host__ 
      void trace_append$mem$cond(InstrData data, const void*const PTR, const void* const SRC_ID) 
        asm (SCABBARD_HOST_CALLBACK_APPEND_MEM_COND_NAME);
      __host__ 
      void trace_append$alloc(const InstrData data, const void*const PTR, const void* const SRC_ID, const std::size_t SIZE) 
        asm (SCABBARD_HOST_CALLBACK_APPEND_ALLOC_NAME);
      __host__ 
      void trace_append$alloc$cond(const InstrData data, const void*const PTR, const void* const SRC_ID, const std::size_t SIZE) 
        asm (SCABBARD_HOST_CALLBACK_APPEND_ALLOC_COND_NAME);
      
    } // namespace host


    /**
     * @brief during runtime this call gets instrumented befor a kernel launch function
     *        it returns a pointer to a scabbard::rtl::DeviceTracker object in device memory.
     *        That will be passed into the kernel and through all user defined kernel functions.
     */
    __host__
    void* register_job(const hipStream_t STREAM)
      asm (SCABBARD_CALLBACK_REGISTER_JOB);
    
    
    /**
     * @brief Register a callback with the stream for the specific job just entered into queue
     *        it will mark the device tracker as finished so that it can be cleaned up,
     *        and rebalance the logical vector clock.
     */
    __host__
    void register_job_callback(void* dt, hipStream_t stream, const void*const SRC_ID)
      asm (SCABBARD_CALLBACK_REGISTER_JOB_CALLBACK);


    /**
     * @brief register a source file at the requested hash key 
     *        to store the metadata in a runtime friendly way
     */
    // __host__
    // std::uint64_t metadata_register$src(const char* src_file)
    //     asm (SCABBARD_CALLBACK_REGISTER_SRC_NAME);
    
    /**
     * @brief unregister a source file at the requested hash key 
     *        to store the metadata in a runtime friendly way
     */
    // __host__
    // void metadata_unregister$src(std::size_t src_key) 
    //       asm (SCABBARD_CALLBACK_UNREGISTER_SRC_NAME);

    // /**
    //  * @brief register a location in a source file at the requested hash key 
    //  *        to store the metadata in a runtime friendly way
    //  */
    // __host__
    // void metadata_register$loc(std::size_t src_key, std::size_t loc_key, std::size_t line, std::size_t col) 
    //       asm (SCABBARD_CALLBACK_REGISTER_LOC_NAME);

    // /**
    //  * @brief unregister a location in a source file at the requested hash key 
    //  *        to store the metadata in a runtime friendly way
    //  */
    // __host__
    // void metadata_unregister$loc(std::size_t src_key, std::size_t loc_key) 
    //       asm (SCABBARD_CALLBACK_UNREGISTER_LOC_NAME);
    



  } //?namespace rtl
} //?namespace scabbard
