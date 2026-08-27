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

// #include <nlohmann/json.hpp>  //TODO: remove
#include <cstdint>
#include <unordered_map>
#include <cstring>

namespace scabbard {
  
  
  struct SrcMetadata {
  private:
    const std::size_t lazy_id = 0ul;
  public:
    char* const srcFile;
    char* const fnName;
    const std::size_t line;
    const std::size_t col;

    struct Hash {
      inline std::uint64_t operator () (const SrcMetadata* data) {
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        uint64_t prime = 1099511628211ULL;       // FNV prime

        std::size_t i = 0ul;
        while (data->srcFile[i] != '\0') {
          hash ^= static_cast<uint64_t>(data->srcFile[i]);
          hash *= prime;
        }
        hash ^= static_cast<uint64_t>(':');
        hash *= prime;
        for (const char& c : std::to_string(data->line)) {
          hash ^= static_cast<uint64_t>(c);
          hash *= prime;
        }
        hash ^= static_cast<uint64_t>(':');
        hash *= prime;
        for (const char& c : std::to_string(data->col)) {
          hash ^= static_cast<uint64_t>(c);
          hash *= prime;
        }
        return hash;
      }
      inline bool operator () (const SrcMetadata* l, const SrcMetadata* r) {
        return l->line == r->line && l->col == r->col && std::strcmp(l->srcFile,r->srcFile);
      }
    };
    using SeenList_t = std::unordered_map<const SrcMetadata*,const std::size_t,SrcMetadata::Hash,SrcMetadata::Hash>;

    static std::size_t next_id;
    static SeenList_t seen;

    inline std::size_t get_id() const {
      if (lazy_id != 0u)
        return lazy_id;
      auto it = SrcMetadata::seen.find(this);
      if (it != SrcMetadata::seen.end()) {
        *const_cast<std::size_t*>(&lazy_id) = it->second;
        return it->second;
      }
      auto id = SrcMetadata::next_id++;
      auto res = SrcMetadata::seen.emplace(this,id);
      *const_cast<std::size_t*>(&lazy_id) = id;
      return id;
    }

    inline bool operator == (const SrcMetadata& other) const {
      return get_id() == other.get_id();
    }
    inline bool operator != (const SrcMetadata& other) const {
      return get_id() != other.get_id();
    }
    inline bool operator < (const SrcMetadata& other) const {
      return get_id() < other.get_id();
    }
    inline bool operator >= (const SrcMetadata& other) const {
      return get_id() >= other.get_id();
    }

    // NLOHMANN_DEFINE_TYPE_INTRUSIVE(SrcMetadata, srcID, srcFile, line, col, modType) //TODO: remove
  };

  std::ostream& operator << (std::ostream& out, SrcMetadata& data);
  // {
  //   return out << "[`" << data.fnName << "()`](\""<< data.srcFile << "\":" 
  //               << data.line << ':' << data.col << ')';
  // }

  // std::string to_string(const SrcMetadata& meta);
  // std::string repr(const SrcMetadata& meta);

} //?namespace scabbard
