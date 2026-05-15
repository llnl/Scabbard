/**
 * @file StateMachine.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief the state machine that process the trace data and reports on the data races 
 * @version alpha 0.0.1
 * @date 2023-10-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <scabbard/TraceData.hpp>
#include <scabbard/rtl/GroupedPtr.hpp>

#include <unordered_set>
#include <set>
#include <map>
#include <tuple>

namespace scabbard {
namespace rtl {

  class StateMachine {

  public:
    using Trace_t = std::multiset<GroupedPtr<const TraceData>, GroupedPtr<const TraceData>::less>;
    using MemTable_t = std::map<std::uintptr_t, GroupedPtr<const TraceData>>;
    using AllocTable_t = std::map<std::uintptr_t, std::size_t>;
    using SyncTable_t = std::map<std::uintptr_t, std::size_t>;
    
  private:
    Trace_t& trace;
    MemTable_t mem;
    AllocTable_t allocs;
    size_t last_global_sync = __UINT64_MAX__;
    SyncTable_t last_stream_sync;

  public:
    StateMachine(const Trace_t& trace_);
    
    struct Result {
      enum Status { GOOD=0, ERROR=2, WARNING=1, INTERNAL_ERROR=-1 };
      Status status;
      GroupedPtr<const TraceData> read = nullptr; 
      GroupedPtr<const TraceData> write = nullptr;
      std::string err_msg = "";
      friend inline bool operator == (const Result& L, const Result& R);
      inline bool operator < (const Result& other) const;
    };

    using ResultList_t = std::map<StateMachine::Result, std::size_t>;

    ResultList_t run();

    void reset();

  private:

    /**
     * @brief check if a race has occurred if the current trace is a read event
     * @param r - the current trace data being processed that is known to be a read event
     * @param o  - the other trace data from the mem object (known to exist)
     * @return \c const ResultStatus - the resulting condition
     */
    const Result::Status check_race_read(const TraceData& r, const TraceData& o);

    // /**
    //  * @brief check if a race has occurred if the current trace is a write event
    //  * @param w - the current trace data being processed that is known to be a write event
    //  * @param o - the other trace data from the mem object (known to exist)
    //  * @return \c const ResultStatus - the resulting condition
    //  */
    // const ResultStatus check_race_write(const TraceData& w, const TraceData& o);

  };

  std::ostream& operator << (std::ostream& out, const StateMachine::ResultStatus& status);

} //?namespace rtl
} //?namespace scabbard


// namespace std {

// template<>
// struct hash<scabbard::rtl::StateMachine::Result> {
//   uint64_t operator () (const scabbard::rtl::StateMachine::Result& res) const
//   {
//     return ((
//             std::hash<int>()(res.status)
//             ^ (((res.read) ? std::hash<scabbard::LocationMetadata>()(res.read->metadata) : 0ul) << 1u) >> 1u)
//           ^ (((res.write) ? std::hash<scabbard::LocationMetadata>()(res.write->metadata) : 0ul) << 1u
//         )
//       );
//   }
// };

// } //?namespace std
