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
    const char* const srcFile;
    const char* const fnName;
    const std::size_t line;
    const std::size_t col;

    struct Hash {
      inline std::uint64_t operator () (const SrcMetadata* const & data) const {
        return (*this)(data->srcFile,data->line,data->col);
      }
      template<typename StrTy>
      inline std::uint64_t operator () (const StrTy& FilePath, std::size_t line, std::size_t col) const {
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        uint64_t prime = 1099511628211ULL;       // FNV prime

        std::size_t i = 0ul;
        while (FilePath[i] != '\0') {
          hash ^= static_cast<uint64_t>(FilePath[i++]);
          hash *= prime;
        }
        hash ^= static_cast<uint64_t>(':');
        hash *= prime;
        for (const char& c : std::to_string(line)) {
          hash ^= static_cast<uint64_t>(c);
          hash *= prime;
        }
        hash ^= static_cast<uint64_t>(':');
        hash *= prime;
        for (const char& c : std::to_string(col)) {
          hash ^= static_cast<uint64_t>(c);
          hash *= prime;
        }
        return hash;
      }
    };
    struct Equal {
      inline bool operator () (const SrcMetadata* const & l, const SrcMetadata* const & r) const {
        return l->line == r->line && l->col == r->col && (std::strcmp(l->srcFile,r->srcFile)==0);
      }
    };
    using SeenList_t = std::unordered_map<const SrcMetadata*,const std::size_t,SrcMetadata::Hash,SrcMetadata::Equal>;
    using IDList_t = std::unordered_map<const SrcMetadata*,const std::size_t>;

    static std::size_t next_id;
    static SeenList_t seen_ids;
    static IDList_t ids;

    inline std::size_t get_id() const {
      if (lazy_id != 0u)
        return lazy_id;
      auto it = SrcMetadata::ids.find(this);
      if (it == SrcMetadata::ids.end()) {
        auto _it = SrcMetadata::seen_ids.find(this);
        if (_it == SrcMetadata::seen_ids.end()) {
          auto id = SrcMetadata::next_id++;
          SrcMetadata::seen_ids.emplace(this,id);
          return id;
        }
        SrcMetadata::ids.emplace(this,_it->second);
        return _it->second;
      }
      return it->second;
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

    SrcMetadata(const char* const srcFile_, const char* const fnName_, std::size_t line_, std::size_t col_)
      : lazy_id(next_id++), srcFile(srcFile_), fnName(fnName_), line(line_), col(col_)
    {}

#   ifdef SCABBARD_UNIT_TEST
    SrcMetadata(std::size_t Id, const char* const Src, const char* const Fn, std::size_t Line, std::size_t Col)
      : lazy_id(Id), srcFile(Src), fnName(Fn), line(Line), col(Col) {}
#   endif

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
