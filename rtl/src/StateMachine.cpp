/**
 * @file StateMachine.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief the state machine that process the trace data and reports on the data races 
 * @version alpha 0.0.1
 * @date 2023-10-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <scabbard/rtl/StateMachine.hpp>
#include <scabbard/rtl/calls.hpp>

#include <algorithm>

#define hipStreamLegacy_ull 1u
#ifndef hipStreamLegacy
#define hipStreamLegacy ((hipStream_t)hipStreamLegacy_ull)
#endif
#define hipStreamPerThread_ull 2u
#ifndef hipStreamPerThread
#define hipStreamPerThread ((hipStream_t)hipStreamPerThread_ull)
#endif

namespace scabbard {
namespace rtl {

/* set to move res if it needs to, might cause performance issues if there exists an existing entry */
inline StateMachine::Result::Status add_result(StateMachine::ResultList_t& results, StateMachine::Result&& res, std::size_t n=1u)
{
  StateMachine::Result::Status retVal = res.status;
  auto it = results.find(res);
  if (it == results.end()) // case not encountered yet
    results.emplace(std::move(res),std::move(n));
  else                     // case encountered before
    it->second += n;
  return retVal;
}

template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::HOST_CONTROL>(const StateMachine::DataPtr_t& td);
template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::DEVICE_CONTROL>(const StateMachine::DataPtr_t& td);


void StateMachine::run(std::uint64_t remainder_quotient)
{
  std::size_t dbg_i = 0u; //DEBUG
  std::size_t dbg_j = 0u; //DEBUG
  std::size_t dbg_k = 0u; //DEBUG
  MemTable_t::iterator i = mem.end();
  const std::size_t GOAL_SIZE = trace.size() >> remainder_quotient;
  while (trace.size() > GOAL_SIZE) {
    const DataPtr_t& td = trace.top();
    if (/* td->time_stamp == 0u || */ td->data == InstrData::NEVER) dbg_j++; //DEBUG
    if (td->data & InstrData::ON_GPU) dbg_k++; //DEBUG
    i = mem.end();
    switch (td->data & FILTER)
    {
      // << ======= Driver Events ======= >> 

      case ON_HOST | SYNC_EVENT: 
        sync_to_zone<Zone_t::HOST_CONTROL>(td);
        break;

      case ON_HOST | LAUNCH_EVENT:
          sync_to_zone<Zone_t::DEVICE_CONTROL>(td);
        break;

      case ON_HOST | ALLOCATE:
        allocs[td->ptr] = td->_OPT_DATA;
        sync_to_zone<Zone_t::HOST_CONTROL>(td); // currently assuming all allocates and frees are sync events
        break;

      case ON_HOST | FREE: {
        auto r = allocs.find(td->ptr);
        if (r == allocs.end()) {
          add_result(results,{Result::INTERNAL_ERROR, td, nullptr, 
                              "Bad hipFree (could not find allocate/register call associated with this hipFree in trace history)"}); //DEBUG
          sync_to_zone<Zone_t::HOST_CONTROL>(td); // currently assuming all allocates and frees are sync events
          break;
          // return {{{INTERNAL_ERROR, nullptr, nullptr, "\n[scabbard.rtl:ERR] bad alloc data (could not find hipMalloc associated with hipFree in trace history)"}, 1ul}};
        }
        i = mem.erase(td->ptr, td->ptr+r->second);
        allocs.erase(r);
        sync_to_zone<Zone_t::HOST_CONTROL>(td); // currently assuming all allocates and frees are sync events
        break;
      }

      // << ===== Host Events ===== >> 

      case ON_HOST | READ | WRITE: // (DH)
      case ON_HOST | READ:
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_uninit = 0ull;
          const Data_t* last_occurrence_uninit;
          uintptr_t last_stop = td->ptr;
          for (const auto& _idh : mem.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (_idh->start > last_stop) {
              occurrences_uninit += _idh->start - last_stop;
              last_occurrence_uninit = _idh->val.unsafe_get();
            } else {
              check_race_HR(td, _idh->val);
            }
            last_stop = _idh->stop;
          }
          if (occurrences_uninit)
            add_result(results,{Result::READ_UNINIT_H, td, DataPtr_t::make(last_occurrence_uninit), 
                                "The CPU read from Uninitialized memory durring a Bulk-CPU-Read/Memcpy"},
                       occurrences_uninit);
          mem.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          i = mem.find(td->ptr);
          if (i == mem.end()) {// read with no preceding write
            if (mem.find(td->ptr) == mem.end()) // check to see if other mem table records an event
              add_result(results,{Result::READ_UNINIT_H, td, nullptr, "The CPU Read from Uninitialized Memory"},
                          ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u)); // read with no preceding write
            mem.insert(td->ptr, td);
          } else {
            check_race_HR(td, i->val);
            if (i->is_single()) i->val = td; else mem.insert(td->ptr, td);
          }
        }
          if (not (td->data & WRITE)) //allow atomic instructions to fall through.
            break;
        
      case ON_HOST | WRITE: // (HD)
        if (td->data & InstrData::_OPT_USED) {
          for (auto _ihd : mem.find_all(td->ptr, td->ptr+td->_OPT_DATA))
            check_race_HW(td, _ihd->val);
          mem.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else {
          i = mem.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
          // not first write of a pair (empty or just allocated)
          //                          AND the conditions with the last memory action checks out
          check_race_HW(td, i->val);
          mem.insert(td->ptr, td); // do nothing but insert into memory later.
        }
        break;


      // << ===== Device Events ===== >> 

      case ON_DEVICE | READ | WRITE: // (HD)
      case ON_DEVICE | READ:
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_uninit = 0ull;
          const Data_t* last_occurrence_uninit;
          uintptr_t last_stop = td->ptr;
          for (auto _ihd : mem.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (_ihd->start > last_stop && mem.find(last_stop) == mem.end()) {
              occurrences_uninit += _ihd->start - last_stop;
              last_occurrence_uninit = _ihd->val.get();
            } else {
              check_race_DR(td, _ihd->val);
            }
            last_stop = _ihd->stop;
          }
          if (occurrences_uninit)
            add_result(results,{Result::READ_UNINIT_D, td, DataPtr_t::make(last_occurrence_uninit), 
                                "The GPU Read from Uninitialized memory durring a Bulk-GPU-Read/Memcpy"},
                       occurrences_uninit);
          mem.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          i = mem.find(td->ptr);
          if (i == mem.end()) {// read with no preceding write
            if (mem.find(td->ptr) == mem.end()) // check to see if it was initalized in other mem table.
              add_result(results,{Result::READ_UNINIT_D, td, nullptr, "a GPU Read of Uninitialized Memory"},
                          ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u));
            mem.insert(td->ptr, td);
          } else {
            check_race_DR(td, i->val);
            if (i->is_single()) i->val = td; else mem.insert(td->ptr, td);
          }
        }
        if (not (td->data & WRITE)) //allow atomic instructions to fall through.
          break; 

      case ON_DEVICE | WRITE: // (DH)
        if (td->data & InstrData::_OPT_USED) {
          std::size_t occurrences_race = 0ull;
          const Data_t* last_occurrence_race;
          for (auto _idh : mem.find_all(td->ptr, td->ptr+td->_OPT_DATA))
            check_race_DW(td, _idh->val);
          mem.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else {
          i = mem.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
          // not first write of a pair (empty or just allocated)
          //                          AND the conditions with the last memory action checks out
          if (i != mem.end())
            check_race_DW(td, i->val);
          mem.insert(td->ptr, td); // do nothing but insert into memory later.
        }

      default:
        break;
    }
    dbg_i++; //DEBUG
    trace.pop();
  }
}


void StateMachine::reset()
{
  mem.clear();
  default_stream_zone = {INIT_ZONE, 0ull};
  per_thread_zones.clear();
  zones.clear();
}

// template<>
// inline void StateMachine::sync_to_zone<StateMachine::Zone_t::HOST_CONTROL>(const StateMachine::DataPtr_t& td)
// {
//   std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
//   switch (stream) {
//     case hipStreamLegacy_ull: {
//       default_stream_zone = {Zone_t::HOST_CONTROL, td->time_stamp};
//       auto sids = zones.allKeys2();
//       for (const StreamId sid : sids)
//         zones[td->threadId.host][sid] = {Zone_t::HOST_CONTROL, td->time_stamp};
//       break;
//     }
//     case hipStreamPerThread_ull: {
//       per_thread_zones[jobId_t::hash_stream_ptr(td->threadId.host)] = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
//       auto rows = zones.findByKey1(td->threadId.host);
//       for (auto& row : rows)
//         row.data = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
//       break;
//     }
//     default:
//       zones[td->threadId.host][jobId_t::hash_stream_ptr(stream)] = {Zone_t::HOST_CONTROL, td->time_stamp};
//       break;
//   }
// }
// template<>
// inline void StateMachine::sync_to_zone<StateMachine::Zone_t::DEVICE_CONTROL>(const StateMachine::DataPtr_t& td)
// {
//   per_thread_zones[jobId_t::hash_stream_ptr(td->threadId.host)] = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
//   std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
//   switch (stream) {
//     case hipStreamLegacy_ull:
//       default_stream_zone = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
//       for (auto& e : zones)
//         e.data = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
//       break;
//     case hipStreamPerThread_ull: {
//       auto rows = zones.findByKey1(td->threadId.host);
//       for (auto& row : rows)
//         row.data = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
//       // /* NOTE: will flow into default case to finish all updates; and some rows may be redundantly updated */
//       // stream = (std::uintptr_t) td->threadId.host;
      
//     }
//     default: {
//       auto rows = zones.findByKey2(jobId_t::hash_stream_ptr(stream));
//       for (auto& row : rows)
//         row.data = {Zone_t::DEVICE_CONTROL, td->time_stamp};
//       break;
//     }
      
//   }
// }
template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::HOST_CONTROL>(const StateMachine::DataPtr_t& td)
{
  /* NOTE: this implementation does not support Non-BLocking Streams */
  std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
  switch (stream) {
    case hipStreamLegacy_ull:
      default_stream_zone = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
    case hipStreamPerThread_ull:
      per_thread_zones[jobId_t::hash_stream_ptr(td->threadId.host)] = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
      break;
    default:
      zones[td->threadId.host][jobId_t::hash_stream_ptr(stream)] = {Zone_t::HOST_CONTROL, td->time_stamp};
      break;
  }
}
template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::DEVICE_CONTROL>(const StateMachine::DataPtr_t& td)
{
  /* NOTE: this implementation does not support Non-BLocking Streams */
  /* NOTE: DEVICE_CONTROL should not update zone transition time if the zone was already in DEVICE_CONTROL */
  std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
  switch (stream) {
    case hipStreamLegacy_ull:
      if (default_stream_zone.state != Zone_t::DEVICE_CONTROL)
        default_stream_zone = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
    case hipStreamPerThread_ull: {
      auto _i = per_thread_zones.find(jobId_t::hash_stream_ptr(td->threadId.host));
      if (_i == per_thread_zones.end())
        per_thread_zones.emplace(jobId_t::hash_stream_ptr(td->threadId.host),
                                  Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp});
      else if (_i->second.state != Zone_t::DEVICE_CONTROL)
        _i->second = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
      break;  
    }
    default: {
      auto rows = zones.findByKey2(jobId_t::hash_stream_ptr(stream));
      for (auto& row : rows)
        if (row.data.state != Zone_t::DEVICE_CONTROL)
          row.data = {Zone_t::DEVICE_CONTROL, td->time_stamp};
      break;
    }
      
  }
}

inline const StateMachine::Zone_t& StateMachine::get_host_zone(const StateMachine::DataPtr_t& HE) const
{
  if (DEFAULT_STREAM_BEHAVIOR() == hipStreamLegacy_ull) {
    auto _ptz = per_thread_zones.find(jobId_t::hash_stream_ptr(HE->threadId.host));
    if (_ptz == per_thread_zones.end())
      return default_stream_zone;
    return ((default_stream_zone.trans_time > _ptz->second.trans_time)
            ? default_stream_zone
            : _ptz->second);
  }

  const auto& i = per_thread_zones.find(jobId_t::hash_stream_ptr(HE->threadId.host));
  if (i != per_thread_zones.end())
    return i->second;
  
  return Zone_t{Zone_t::INIT_ZONE, 0u};
}

inline const StateMachine::Zone_t& StateMachine::get_device_zone(const StateMachine::DataPtr_t& DE) const
{
  /* NOTE: current implementation has all stored device thread id's are a hashed versions of the
      calling host's threadId rather than 0 if hipStreamPerThread is enabled */
  const StreamId stream = DE->threadId.device.job.STREAM;
  if (stream == 0u)
    return default_stream_zone;
  
  auto _ptz = per_thread_zones.find(stream);
  if (_ptz != per_thread_zones.end())
    return _ptz->second; // case: default stream job

  // hard method: look for last transition time that fits this kernel launch
  Zone_t& most_recent = {Zone_t::INIT_ZONE, 0u};
  for (auto& row : zones.findByKey2(stream))
    if (row.data.state == Zone_t::DEVICE_CONTROL
         && row.data.trans_time > most_recent.trans_time)
      most_recent = row.data;
  return most_recent;

  // // lazy method: assume device was meant to have control over the data
  // return {Zone_t::DEVICE_CONTROL, DE->time_stamp};
}

inline const StateMachine::Zone_t& StateMachine::get_zone(const StateMachine::DataPtr_t& HE,
                                                          const StateMachine::DataPtr_t& DE) const
{
  const auto i = zones.find(HE->threadId.host, DE->threadId.device.job.STREAM);
  if (i)
    return i->data;
  return get_host_zone(HE); // might need to get device zone separately then compare for most recent?
}


// << ------------------------------------------------------------------------------------------ >> 


StateMachine::Result::Status StateMachine::check_race_HR(const StateMachine::DataPtr_t& HR, 
                                                         const StateMachine::DataPtr_t& o) 
{
  // Zone_t zone = ((o->data & ON_HOST) 
  //                 ? get_host_zone(HR)
  //                 : get_zone(HR, o));

  if (o->data & ON_HOST) 
    return Result::GOOD;
  
  Zone_t zone = get_zone(HR, o);

  if (zone.state != Zone_t::DEVICE_CONTROL  // not a critical zone
      || zone.trans_time > o->time_stamp)   // OR the previous device event is stale/out-of-date
    return Result::GOOD;

  switch (o->data & FILTER) {
    case ON_DEVICE | READ:
      return add_result(results,{Result::UNPROTECTED_HR, HR, nullptr, "WARN: Host Read from memory currently controlled by a Device"});
  
    case ON_DEVICE | WRITE:
    case ON_DEVICE | READ | WRITE: //for atomicrmw instructions
      return add_result(results,{Result::POS_RACE_HR_DW,HR,o, "WARN: Host Read from memory still controlled by a Device (no protections from HR->DW Race)"});
      
    default:
      return add_result(results,{Result::INTERNAL_ERROR,HR,o,"[scabbard.rtl.sm.checkHR:ERR] unknown event data stored in state `mem` variable"});
  }

}


StateMachine::Result::Status StateMachine::check_race_HW(const StateMachine::DataPtr_t& HW, 
                                                         const StateMachine::DataPtr_t& o) 
{
  // Zone_t zone = ((o->data & ON_HOST) 
  //                 ? get_host_zone(HR)
  //                 : get_zone(HR, o));

  if (o->data & ON_HOST) 
    return Result::GOOD;
  
  Zone_t zone = get_zone(HR, o);

  if (zone.state != Zone_t::DEVICE_CONTROL  // not a critical zone
      || zone.trans_time > o->time_stamp)   // OR the previous device event is stale/out-of-date
    return Result::GOOD;

  switch (o->data & FILTER) {
    case ON_DEVICE | READ: 
    case ON_DEVICE | READ | WRITE: // for atomicrmw instructions
      return add_result(results,{Result::RACE_DR_HW, o, HW, "RACE FOUND: DR->HW - Host Wrote to a memory location currently controlled by a Device after the Device Read from it"});

    case ON_DEVICE | WRITE:
      return add_result(results,{Result::UNPROTECTED_HW, o, HW, "WARN: Host Wrote to memory still controlled by a Device"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR, o, HW,"[scabbard.rtl.sm.checkHW:ERR] unknown event data stored in state `mem` variable"});
  }
}



StateMachine::Result::Status StateMachine::check_race_DR(const StateMachine::DataPtr_t& DR, 
                                                         const StateMachine::DataPtr_t& o) 
{
  // Zone_t zone = ((o->data & ON_DEVICE) 
  //                 ? get_device_zone(DR)
  //                 : get_zone(o, DR));

  if (o->data & ON_DEVICE)
    return Result::GOOD;

  Zone_t zone = get_zone(o, DR);

  if (zone.state != Zone_t::DEVICE_CONTROL)
    return add_result(results,{Result::INTERNAL_ERROR,DR,o,"[scabbard.rtl.sm.checkDR:ERR] a Device kernel did not get ownership of its zone"});
  
  if (zone.trans_time > o->time_stamp)
    return Result::GOOD;

  switch (o->data & FILTER) 
  {
    case ON_HOST | READ: 
      return add_result(results,{Result::UNPROTECTED_HR,DR,o,"WARN: Host Read from memory currently controlled by a Device"});

    case ON_HOST | WRITE:
    case ON_HOST | READ | WRITE:
      return add_result(results,{Result::POS_RACE_DR_HW,DR,o,"WARN: a Device Read from the the same location the Host Wrote to unprotected (no protection from DR->HW Race)"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR,DR,o,"[scabbard.rtl.sm.checkDR:ERR] unknown event data stored in state `mem` variable"});
  }
}



StateMachine::Result::Status StateMachine::check_race_DW(const StateMachine::DataPtr_t& DW, 
                                                         const StateMachine::DataPtr_t& o) 
{
  // Zone_t zone = ((o->data & ON_DEVICE) 
  //                 ? get_device_zone(DW)
  //                 : get_zone(o, DW));

  if (o->data & ON_DEVICE)
    return Result::GOOD;

  Zone_t zone = get_zone(o, DW);

  if (zone.state == Zone_t::HOST_CONTROL)
    return add_result(results,{Result::INTERNAL_ERROR,DW,o,"[scabbard.rtl.sm.checkDW:ERR] a Device kernel did not get ownership of its zone"});
  
  if (zone.trans_time > o->time_stamp)
    return Result::GOOD;
    
  switch (o->data & FILTER) 
  {
    case ON_HOST | READ:
    case ON_HOST | READ | WRITE:  // treating atomicrmw as reads in this case
      return add_result(results,{Result::RACE_HR_DW,o,DW,"RACE FOUND: HR->DW - The Host Read from memory controlled by a Device before it Wrote to it"});
    
    case ON_HOST | WRITE:
      return add_result(results,{Result::UNPROTECTED_HW,o,DW,"WARN: the Host Wrote to memory controlled by a Device"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR,o,DW,"[scabbard.rtl.sm.checkDW:ERR] unknown event data stored in state `mem` variable"});
  }
}


// << ------------------------------------------------------------------------------------------ >> 


inline std::ostream& operator << (std::ostream& out, const StateMachine::Result::Status& status)
{
  switch (status)
  {
    case StateMachine::Result::Status::RACE_DR_HW:
    case StateMachine::Result::Status::RACE_HR_DW:
      return (out << "DATA RACE FOUND");
      break;
    case StateMachine::Result::Status::POS_RACE_DR_HW:
    case StateMachine::Result::Status::POS_RACE_HR_DW:
    case: StateMachine::Result::Status::UNPROTECTED_HR:
    case: StateMachine::Result::Status::UNPROTECTED_HW:
      return (out << "POSSIBLE Data Race Found");
    case StateMachine::Result::Status::GOOD:
      return (out << "NO data races detected");
    case StateMachine::Result::Status::INTERNAL_ERROR:
      return (out << "Internal ERROR occurred in Scabbard RTL");
    default:
      return (out << "<UNKNOWN_STATUS>");
  }
}

inline bool operator == (const StateMachine::Result& L, const StateMachine::Result& R)
{
  return ( (L.status == R.status)
      && ((L.first_td && R.first_td) ? *L.first_td->metadata == *R.first_td->metadata : L.first_td == R.first_td)
      && ((L.second_td && R.second_td) ? *L.second_td->metadata == *R.second_td->metadata : L.second_td == R.second_td)
    );
}

inline bool operator < (const StateMachine::Result& l, const StateMachine::Result& r)
{
  //TODO: figure out what orderings we want for this
  if (l.status > r.status)
    return false;
  return ( (l.status < r.status)
      || ((l.first_td && r.first_td) && *l.first_td->metadata < *r.first_td->metadata)
      || ((l.second_td && r.second_td) && *l.second_td->metadata < *r.second_td->metadata)
    );
}

// inline void StateMachine::move_append(StateMachine::DataPtr_t&& __Ptr) 
// {
//   trace.emplace(std::move(__Ptr));
// }
// inline void StateMachine::copy_append(const StateMachine::DataPtr_t& Ptr)
// {
//   trace.push(Ptr);
// }

inline StateMachine& operator << (StateMachine& SM, StateMachine::DataPtr_t&& __Ptr)
{
  SM.trace.emplace(std::move(__Ptr));
  return SM;
}

inline StateMachine& operator << (StateMachine& SM, const StateMachine::DataPtr_t& Ptr)
{
  SM.trace.push(Ptr);
  return SM;
}

} //?namespace rtl
} //?namespace scabbard
