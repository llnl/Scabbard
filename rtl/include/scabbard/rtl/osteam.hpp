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
  bool print_label = false;
  std::string label = "scabbard.rtl";
  std::string type = "INFO";
  std::size_t _indent = 0u;

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
    print_label = is_stdio() || old_out == &std::cerr;
    return old_out;
  }

  inline std::ostream* get() const { return out; }
  inline std::ostream& getRef() { return *out; }

  inline std::ostream& operator*() { return *out; }
  inline std::ostream* operator->() { return out; }

  inline ostream& indent(std::size_t n=2u) { _indent += n; return *this; }
  inline ostream& dedent(std::size_t n=2u;) { _indent = (n<_indent) ? _indent-n : 0u; return *this; }
  inline ostream& setIndent(std::size_t n=0) { _indent = n; return *this; }
  inline ostream& setLabel(std::string&& label_) { label = label_; return *this; }
  inline ostream& setLogType(std::string&& logType_) { type = logType_; return *this; }
  inline ostream& reset() {
    label = "scabbard.rtl"; type = "INFO"; _indent = 0u; 
    return *this;
  }

  friend class endl;
  friend class nl;
  friend class flush;
  friend class indent;
  friend class dedent;
  friend class SetLabel;
  friend class SetLogType;

  // template<typename _CharT, typename _Traits>
  // inline ostream& operator << (std::basic_ostream<_CharT, _Traits>& (*manipulator)(std::basic_ostream<_CharT, _Traits>&)) {
  //   manipulator(*out.out); return out;
  // }
  // template<typename _CharT, typename _Traits>
  // friend inline ostream& operator << (ostream& out, 
  //                                     std::basic_ostream<_CharT, _Traits>& (*manipulator)(std::basic_ostream<_CharT, _Traits>&)) {
  //   manipulator(*out.out); return out;
  // }
  template<typename T>
  inline ostream& operator << (const T& other) {
    (*out) << other; return *this;
  }
  template<>
  inline ostream& operator << (const nl&) {
    if (print_label)
      *out << "\n[" << label << ':' << type << "] " << std::string(' ', _indent);
    else
      *out << '\n' << std::string(' ', _indent);
    return *this;
  }
  template<>
  inline ostream& operator << (const endl&) {
    *out << '\n' << std::flush;
    label = "scabbard.rtl";
    type = "INFO";
    return *this;
  }
  template<>
  inline ostream& operator << (const flush&) {
    *out << std::flush; return *this;
  }
  inline ostream& operator << (const scabbard::rtl::indent& indent_) {
    return indent(indent_.n);
  }
  template<>
  inline ostream& operator << (const scabbard::rtl::dedent& dedent_) {
    return dedent(dedent_.n);
  }
  inline ostream& operator << (const scabbard::rtl::SetLabel& label_) {
    return this->setLabel(label_.x);
  }
  template<>
  inline ostream& operator << (const scabbard::rtl::SetLogType& logType_) {
    return setLogType(logType_.x);
  }
  // template<>
  // inline ostream& operator << (std::ostream& (*manipulator)(std::ostream&)) {
  //   manipulator(*out); return this;
  // }
  // template<>
  // inline ostream& operator << (std::function<std::ostream&(std::ostream&)>& manipulator) {
  //   manipulator(*out); return *this;
  // }
};

struct nl {};
struct endl {};
struct flush {};
struct indent { size_t n = 2u; indent(size_t n_) : n(n_) {} };
struct dedent { size_t n = 2u; dedent(size_t n_) : n(n_) {} };
struct SetLabel { 
  std::string x = "scabbard.rtl"; 
  SetLabel() = default;
  SetLabel(const std::string& x_) : x(x_) {}
  SetLabel(std::string&& x_) : x(std::exchange(x_,std::string())) {}
};
struct SetLogType { 
  std::string x = "INFO";
  SetLogType() = default;
  SetLogType(const std::string& x_) : x(x_) {}
  SetLogType(std::string&& x_) : x(std::exchange(x_,std::string())) {}
};

} //?namespace rtl
} //?namespace scabbard

namespace std {
  
// inline scabbard::rtl::ostream& nl(scabbard::rtl::ostream& out) {
//   return (out << scabbard::rtl::nl());
// }
// inline scabbard::rtl::ostream& endl(scabbard::rtl::ostream& out) {
//   return (out << scabbard::rtl::endl());
// }
// inline scabbard::rtl::ostream& flush(scabbard::rtl::ostream& out) {
//   return (out << scabbard::rtl::flush());
// }
// inline scabbard::rtl::ostream& indent(scabbard::rtl::ostream& out, size_t n=2u);
// inline scabbard::rtl::ostream& indent(scabbard::rtl::ostream& out, size_t n) {
//   return out.indent(n);
// }
// inline scabbard::rtl::ostream& dedent(scabbard::rtl::ostream& out, size_t n=2u);
// inline scabbard::rtl::ostream& dedent(scabbard::rtl::ostream& out, size_t n) {
//   return out.dedent(n);
// }


} //?namespace std

