/**
 * @file globals.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Instrumentation globals extern definitions
 * @version alpha 0.0.1
 * @date 2023-05-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <scabbard/rtl/AsyncQueue.hpp>

namespace scabbard {
  namespace rtl {

    extern AsyncQueue TRACE_LOGGER;
    // __device__ DeviceAsyncQueue* DEVICE_TRACE_LOGGER = nullptr;
    // extern MetadataStore METADATA_STORE;
  
  } //?namespace rtl
} //?namespace scabbard
