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
#include <scabbard/rtl/IntervalMap.hpp>

// #include <llvm/ADT/IntervalMap.h>

#include <unordered_set>
#include <queue>
#include <unordered_map>
#include <tuple>

namespace scabbard {
namespace rtl {

  class StateMachine {

  public:
    using Data_t = TraceData;
    using DataPtr_t = GroupedPtr<const Data_t>;
    using Trace_t = std::priority_queue<DataPtr_t, 
                                        std::vector<DataPtr_t>,
                                        DataPtr_t::priority_less>;
    using MemTable_t = IntervalMap<std::uintptr_t, DataPtr_t>;
    using AllocTable_t = std::unordered_map<std::uintptr_t, std::size_t>;
    using SyncTable_t = std::unordered_map<std::uintptr_t, std::size_t>;

    struct Result {
      enum Status { 
        GOOD=0, 
        READ_UNINIT_D=1, READ_UNINIT_H=2,
        POS_RACE_DH=3, POS_RACE_HD=4,
        RACE_DH=6, RACE_HD=5,
        INTERNAL_ERROR=-1 
      };
      Status status;
      DataPtr_t read = nullptr; 
      DataPtr_t write = nullptr;
      std::string msg = "";
      friend inline bool operator == (const Result& L, const Result& R);
      friend inline bool operator < (const Result& L, const Result& R);
    };

    using ResultList_t = std::map<const StateMachine::Result, std::size_t>;
    
  private:
    Trace_t trace;
    MemTable_t mem_dh;
    MemTable_t mem_hd;
    AllocTable_t allocs;
    size_t last_global_sync = __UINT64_MAX__;
    SyncTable_t last_stream_sync;
    size_t last_global_launch = __UINT64_MAX__;
    SyncTable_t last_stream_launch;
    ResultList_t results;

  public:
    StateMachine() = default;


    /**
     * @brief Run the StateMachine on the trace data.
     * @param remainder_proportion how much of the trace to leave unprocessed,
     *                             so that timings left in the buffers can be sorted appropriately. \n 
     *                             Value is expressed in a left bit-shift format ( \c >> ),
     *                             Such that \c 0 will process all of the current trace;
     *                             \c 1 will leave 1/2 of the current trace un-processed;
     *                             \c 2 will leave 1/4 of the current trace un-processed;
     *                             and so on with the form (1/(x+1)). 
     */
    void run(std::uint64_t remainder_proportion=0);

    void reset();

    inline const ResultList_t& get_results() const { return results; }

  private:

    /**
     * @brief check if a race has occurred if the current trace is a READ event (for d->h races)
     * @param r - the current trace data being processed that is known to be a read event
     * @param o  - the other trace data from the mem_dh object (known to exist)
     * @return \c const ResultStatus - the resulting condition
     */
    Result::Status check_race_read_dh(const DataPtr_t& r, const DataPtr_t& o);

    /**
     * @brief check if a race has occurred if the current trace is a READ event (for h->d races)
     * @param r - the current trace data being processed that is known to be a read event
     * @param o  - the other trace data from the mem_dh object (known to exist)
     * @return \c const ResultStatus - the resulting condition
     */
    Result::Status check_race_read_hd(const DataPtr_t& r, const DataPtr_t& o);

    /**
     * @brief check if a race has occurred if the current trace is a WRITE event (for d->h races)
     * @param r - the current trace data being processed that is known to be a read event
     * @param o  - the other trace data from the mem_dh object (known to exist)
     * @return \c const ResultStatus - the resulting condition
     */
    Result::Status check_race_write_dh(const DataPtr_t& w, const DataPtr_t& o);

    /**
     * @brief check if a race has occurred if the current trace is a WRITE event (for h->d races)
     * @param r - the current trace data being processed that is known to be a read event
     * @param o  - the other trace data from the mem_dh object (known to exist)
     * @return \c const ResultStatus - the resulting condition
     */
    Result::Status check_race_write_hd(const DataPtr_t& w, const DataPtr_t& o);


    friend inline StateMachine& operator << (StateMachine& SM, DataPtr_t& Ptr);
    friend inline StateMachine& operator << (StateMachine& SM, DataPtr_t&& __Ptr);

  };

  inline std::ostream& operator << (std::ostream& out, const StateMachine::Result::Status& status);

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
