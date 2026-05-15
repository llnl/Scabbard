/**
 * @file osteam.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Provide a simple singleton interface that allows replacing the value
 *        in global space after initialization without adding too much extra overhead. 
 * @version alpha 0.0.1
 * @date 2026-05-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <ostream>
#include <iostream>

namespace scabbard {
namespace rtl {

/// @brief Provide a simple singleton interface that allows replacing the value
///        in global space after initialization without adding too much extra overhead.
class ostream {

  std::ostream* out = nullptr;

public:

  ostream() = default;

  ostream(std::ostream& out_) : out(&out_) {}
  ostream(std::ostream* out_) : out( out_) {}

  ~ostream() {
    if (not out || is_stdio())
      return;
    delete out;
  }


  /// @brief returns \c true if the \c std::ostream* underpinning this object is
  ///        either \c std::cout or \c std::cerr .
  /// @return \c bool
  inline bool is_stdio() const {
    return out == &std::cout || out == &std::cerr;
  }

  /// @brief Replace whatever \c std::ostream* is currently underpinning this object, 
  ///        with \param new_out and return a pointer to the old one (may be \c nullptr ).
  ///        If the old \c std::ostream was \c std::cout or \c std::cerr it will return
  ///        \c nullptr .
  /// @param new_out the new ostream to replace the old one.
  /// @return \c std::ostream* - ptr to previous \c std::ostream* \param new_out is replacing.
  inline std::ostream* replace(std::ostream* new_out) {
    std::ostream* old_out = ((is_stdio()) ? nullptr : out);
    out = new_out;
    return old_out;
  }

  inline std::ostream* get() const { return out; }
  inline std::ostream& getRef() { return *out; }

  inline std::ostream& operator*() { return *out; }
  inline std::ostream* operator->() { return out; }

};

template<typename T>
inline std::ostream& operator << (ostream& out, const T& other) {
  return ((*out) << other);
} 

} //?namespace rtl
} //?namespace scabbard

