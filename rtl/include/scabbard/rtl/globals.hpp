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
#include <scabbard/rtl/StateMachine.hpp>
#include <scabbard/rtl/osteam.hpp>

namespace scabbard {
  namespace rtl {

    /// @brief Scabbard RTL's version of \c std::cout
    ///        Used so that env variables can redirect
    ///        the output to go to a separate place from the
    ///        output form the instrumented program--if desired.
    extern scabbard::rtl::ostream SCAB_SOUT;
    /// @brief Scabbard RTL's version of \c std::cerr .
    ///        Used so that env variables can redirect
    ///        the output to go to a separate place from the
    ///        output form the instrumented program--if desired.
    extern scabbard::rtl::ostream SCAB_SERR;
  
  } //?namespace rtl
} //?namespace scabbard
