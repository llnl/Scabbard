/**
 * @file ReportWriter.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief output to desired location the final report of Scabbard's verification process
 * @version alpha 0.0.1
 * @date 2026-05-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace scabbard {
namespace rtl {

class ostream;

/// @brief Produce a human readable report of the results of Scabbard's verification step
///         to the desired location specified by providing the desired \c std::ostream
///         or \c scabbard::rtl::ostream to the constructor.
class ReportWriter {

  /// @brief Where to write the report to
  std::ostream& out;


public:
  
  ReportWriter(std::ostream& out_) : out(out_) {} 
  ReportWriter(scabbard::rtl::ostream& out_) : out(*out_.get()) {}


};


} //?namespace rtl
} //?namespace scabbard
