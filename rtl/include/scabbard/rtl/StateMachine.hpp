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
#include <scabbard/rtl/DualKeyTable.hpp>

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
    using StreamList_t = std::unordered_map<HostThreadId, std::unordered_set<std::uintptr_t>>;
    using ThreadList_t = std::unordered_map<std::uintptr_t, std::unordered_map<HostThreadId,LTime_t>>;
    

    struct Result {
      enum Status { 
        GOOD=0, 
        READ_UNINIT_D, READ_UNINIT_H,
        POS_RACE_DR_HW, POS_RACE_HR_DW,
        UNPROTECTED_HW, UNPROTECTED_HR,
        RACE_DR_HW, RACE_HR_DW,
        INTERNAL_ERROR=-1 
      };
      Status status;
      DataPtr_t first_td = nullptr; 
      DataPtr_t second_td = nullptr;
      std::string msg = "";
      friend inline bool operator == (const Result& L, const Result& R);
      friend inline bool operator < (const Result& L, const Result& R);
    };

    using ResultList_t = std::map<const StateMachine::Result, LTime_t>;

    struct Zone_t {
      enum State { INIT_ZONE, HOST_CONTROL, DEVICE_CONTROL };
      State state;
      LTime_t transition_time;
    };

    using ZoneTable_t = std::unordered_map<std::uintptr_t, Zone_t>;
    
  private:
    Trace_t trace;
    MemTable_t mem;
    AllocTable_t allocs;
    Zone_t default_stream_zone = {INIT_ZONE, 0ull};
    ZoneTable_t stream_zone;
    StreamList_t streams_per_thread;
    ThreadList_t threads_per_stream;
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

    inline void append(DataPtr_t&& __Ptr) { move_append(std::move(__Ptr)); }
    inline void append(const DataPtr_t& Ptr) { copy_append(Ptr); }

  private:

    const InstrData FILTER = (
            InstrData::ON_HOST | InstrData::ON_DEVICE
          | InstrData::SYNC_EVENT | InstrData::DESYNC_EVENT
          | InstrData::READ | InstrData::WRITE
          | InstrData::ALLOCATE | InstrData::FREE
        );

    Result::Status check_race_DR(const DataPtr_t& DR, const DataPtr_t& o);
    Result::Status check_race_HR(const DataPtr_t& HR, const DataPtr_t& o);
    Result::Status check_race_DW(const DataPtr_t& DW, const DataPtr_t& o);
    Result::Status check_race_HW(const DataPtr_t& HW, const DataPtr_t& o);

    /**
     * @brief modify the state to reflect that a sync/de-sync event occurs transfering control as indicated
     * @param td - the trace data indicating the sync/de-sync event
     * @param zone - where the control is being handed off to
     */
    template<Zone_t::State ZS>
    inline void sync_to_zone(const DataPtr_t& td);

    inline const Zone_t& get_host_zone(const DataPtr_t& td) const;
    inline const Zone_t& get_device_zone(const DataPtr_t& td) const;
    /**
     * @brief Get the host zone per stream object
     *        NOTE: assumes that you have called \c get_host_zone() this cycle already
     */
    inline const Zone_t& get_host_zone_per_stream(const DataPtr_t& H, const DataPtr_t& D) const;


    inline void move_append(DataPtr_t&& __Ptr) { trace.emplace(std::move(__Ptr)); }
    inline void copy_append(const DataPtr_t& Ptr) { trace.push(Ptr); }


    friend inline StateMachine& operator << (StateMachine& SM, const DataPtr_t& Ptr);
    friend inline StateMachine& operator << (StateMachine& SM, DataPtr_t&& __Ptr);

  };

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
//           ^ (((res.second) ? std::hash<scabbard::LocationMetadata>()(res.second->metadata) : 0ul) << 1u
//         )
//       );
//   }
// };

// } //?namespace std
