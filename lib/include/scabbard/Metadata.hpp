/**
 * @file Metadata.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Contents pertaining to Metadata structures scrapped by the scabbard instrument-er
 * @version alpha 0.0.1
 * @date 2023-06-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include "Enums.hpp"

// #include <nlohmann/json.hpp>  //TODO: remove
#include <cstdint>
#include <unordered_set>
#include <cstring>

namespace scabbard {
  
  
  struct SrcMetadata {
    char* const srcFile;
    char* const fnName;
    const std::size_t line;
    const std::size_t col;

    // inline bool operator < (const SrcMetadata& other) const {
    //   return srcID < other.srcID;
    // }

    // NLOHMANN_DEFINE_TYPE_INTRUSIVE(SrcMetadata, srcID, srcFile, line, col, modType) //TODO: remove
  };

  std::ostream& operator << (std::ostream& out, SrcMetadata& data) {
    return out << "[`" << data.fnName << "()`](\""<< data.srcFile << "\":" 
                << data.line << ':' << data.col << ')';
  }

  // std::string to_string(const SrcMetadata& meta);
  // std::string repr(const SrcMetadata& meta);

} //?namespace scabbard
