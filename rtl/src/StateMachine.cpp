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
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::HOST_CONTROL>(const StateMachine::WDataPtr_t td);
template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::DEVICE_CONTROL>(const StateMachine::WDataPtr_t td);

StateMachine::Zone_t StateMachine::Zone_t::DEFAULT_ZONE = {StateMachine::Zone_t::INITIALIZATION_PHASE, 0ull};

void StateMachine::run(std::uint64_t remainder_quotient)
{
  std::size_t dbg_i = 0u; //DEBUG
  std::size_t dbg_j = 0u; //DEBUG
  std::size_t dbg_k = 0u; //DEBUG
  MemTable_t::iterator i = memH.end();
  const std::size_t GOAL_SIZE = (remainder_quotient) ? trace.size() >> remainder_quotient : 0u;
  while (trace.size() > GOAL_SIZE) {
    const DataPtr_t& td = trace.top();
    if (/* td->time_stamp == 0u || */ td->data == InstrData::NEVER) dbg_j++; //DEBUG
    if (td->data & InstrData::ON_GPU) dbg_k++; //DEBUG
    i = memH.end();
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
        allocs.emplace(td->ptr,td->_OPT_DATA);
        sync_to_zone<Zone_t::HOST_CONTROL>(td); // currently assuming all allocates and frees are sync events
        break;

      case ON_HOST | FREE: {
        auto r = allocs.find(td->ptr);
        if (r == allocs.end()) {
          add_result(results,{Result::INTERNAL_ERROR, td, nullptr, 
                              "Bad hipFree (could not find allocate/register call associated with this hipFree in trace history)"});
        } else {
          const std::uintptr_t end = td->ptr+r->second;
          memH.erase(td->ptr, end);
          memD.erase(td->ptr, end);
          // ownership.erase(td->ptr, td->ptr+r->second);
          allocs.erase(r);
        }
        sync_to_zone<Zone_t::HOST_CONTROL>(td); // currently assuming all allocates and frees are sync events
        break;
      }

      // << ===== Host Events ===== >> 

      case ON_HOST | READ | WRITE:
      case ON_HOST | READ: {
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_uninit = check_uninit_bulkR(td, memD, memH, 
                                                              [this](auto HR, auto DE) 
                                                              {return check_race_HR(HR,DE);});
          if (occurrences_uninit)
            add_result(results,{Result::READ_UNINIT_H, td, nullptr, 
                                "The Host Read from Uninitialized memory durring a Bulk-CPU-Read/Memcpy"},
                       occurrences_uninit);
          memH.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          i = memD.find(td->ptr);
          if (i == memD.end()) { // no device event occurred before this one
            if (memH.find(td->ptr) == memH.end()) // check to see if other mem table records an event
              add_result(results,{Result::READ_UNINIT_H, td, nullptr, "The Host Read from Uninitialized Memory"}); // read with no preceding write
            const Zone_t zone = get_host_zone(td);
            if (zone.state == Zone_t::DEVICE_CONTROL)
              add_result(results,{Result::UNPROTECTED_HR, td, nullptr, "WARN: Host Read from memory not sync protected on this Host thread!"});
          } else {
            check_race_HR(td, i->val);
          }
          memH.insert(td->ptr, td);
        }
      }
          if (not (td->data & WRITE)) //allow atomic instructions to fall through.
            break;
        
      case ON_HOST | WRITE: {
        if (td->data & InstrData::_OPT_USED) {
          const std::uintptr_t END_CAP = td->ptr+td->_OPT_DATA;
          auto DEvents = memD.find_all(td->ptr, END_CAP);
          if (DEvents.empty()) {
            const Zone_t zone = get_host_zone(td);
            if (zone.state == Zone_t::DEVICE_CONTROL)
            add_result(results, {Result::UNPROTECTED_HW,td,nullptr,"WARN: Host Wrote to memory not sync protected on this Host thread!"}, td->_OPT_DATA);
          }
          for (const auto _i : DEvents)
            check_race_HW(td, _i->val);
          memH.insert(td->ptr, END_CAP, td);
        } else {
          i = memD.find(td->ptr);
          if (i != memD.end())
            check_race_HW(td, i->val);
          else {
            const Zone_t zone = get_host_zone(td);
            if (zone.state == Zone_t::DEVICE_CONTROL)
              add_result(results,{Result::UNPROTECTED_HW, td, nullptr, "WARN: Host Wrote to memory not sync protected on this Host thread!"});
          }
          memH.insert(td->ptr, td);
        }
        break;
      }


      // << ===== Device Events ===== >> 

      case ON_DEVICE | READ | WRITE:
      case ON_DEVICE | READ:
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_uninit = check_uninit_bulkR(td, memH, memD,
                                                              [this](auto DR, auto HE) 
                                                              {return check_race_DR(DR,HE);});
          if (occurrences_uninit)
            add_result(results,{Result::READ_UNINIT_D, td, nullptr, //DataPtr_t::make(last_occurrence_uninit), 
                                "A Device Kernel Read from Uninitialized memory durring a Bulk-GPU-Read/Memcpy"},
                       occurrences_uninit);
          memD.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          i = memH.find(td->ptr);
          if (i == memH.end()) {// read with no preceding write
            if (memD.find(td->ptr) == memD.end()) // check to see if it was initalized in other mem table.
              add_result(results,{Result::READ_UNINIT_D, td, nullptr, "A Device Kernel Read from Uninitialized Memory"},
                          ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u));
          } else {
            check_race_DR(td, i->val);
          }
          memD.insert(td->ptr, td);
        }
        if (not (td->data & WRITE)) //allow atomic instructions to fall through.
          break; 

      case ON_DEVICE | WRITE:
        if (td->data & InstrData::_OPT_USED) {
          const std::uintptr_t END_CAP = td->ptr+td->_OPT_DATA;
          std::size_t occurrences_race = 0ull;
          const Data_t* last_occurrence_race;
          for (const auto _i : memH.find_all(td->ptr, END_CAP))
            check_race_DW(td, _i->val);
          memD.insert(td->ptr, END_CAP, td);
        } else {
          i = memH.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
          // not first write of a pair (empty or just allocated)
          //                          AND the conditions with the last memory action checks out
          if (i != memH.end())
            check_race_DW(td, i->val); 
          memD.insert(td->ptr, td);
        }
      break;

      default:
        add_result(results,{Result::INTERNAL_ERROR, td, nullptr, "[scabbard.rtl.sm:Err] failed to recognize/handle trace type"});
        break;
    }
    // if (td->data & ON_DEVICE) {
    //   if (td->data & _OPT_DATA_USED)
    //     ownership.insert(td->ptr, td->ptr+td->_OPT_DATA, td->threadId.device.job.STREAM);
    //   else
    //     ownership.insert(td->ptr, td->threadId.device.job.STREAM);
    // }
    dbg_i++; //DEBUG
    trace.pop();
  }
}


void StateMachine::reset()
{
  memH.clear();
  memD.clear();
  // ownership.clear();
  default_stream_zone = {Zone_t::INITIALIZATION_PHASE, 0ull};
  per_thread_zones.clear();
  zones.clear();
  for (auto [k,v] : allocs) {
    allocs.emplace(v,k);
  }
  allocs.clear();
}

std::size_t StateMachine::check_uninit_bulkR(const StateMachine::WDataPtr_t td, 
                                            const StateMachine::MemTable_t& mem, 
                                            const StateMachine::MemTable_t& mem_same,
                                            const StateMachine::CheckRaceFn_t& check_race_fn,
                                            bool on_host)
{
  const std::uintptr_t END_CAP = td->ptr+td->_OPT_DATA;
  std::size_t occurrences_uninit = 0ull;
  // const Data_t* last_occurrence_uninit = nullptr;
  std::uintptr_t last_stop = td->ptr;
  const auto OEvents = mem.find_all(td->ptr, END_CAP);
  if (OEvents.empty() && on_host
      && get_host_zone(td).state == Zone_t::DEVICE_CONTROL) 
    add_result(results,{Result::UNPROTECTED_HR, td, nullptr, "WARN: Host Read from memory not Sync protected on this Host thread!"},
              td->_OPT_DATA);
  
  for (const auto _i : OEvents) {
    if (_i->start > last_stop) {
      std::uintptr_t _ls = last_stop;
      for (const auto _j : mem_same.find_all(last_stop,_i->start)) {
        if (_j->start > _ls)
          occurrences_uninit += _j->start - _ls;
        _ls = _j->stop;
      }
      if (_ls < END_CAP)
        occurrences_uninit += END_CAP-_ls;
    } //else
      check_race_fn(td, _i->val);
    last_stop = _i->stop;
  }
  if (last_stop < END_CAP) {
    std::uintptr_t _ls = last_stop;
    for (const auto _j : mem_same.find_all(last_stop,END_CAP)) {
      if (_j->start > _ls)
        occurrences_uninit += _j->start - _ls;
      _ls = _j->stop;
    }
    if (_ls < END_CAP)
      occurrences_uninit += END_CAP-_ls;
  }
  return occurrences_uninit;
}

template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::HOST_CONTROL>(const StateMachine::WDataPtr_t td)
{
  /* NOTE: this implementation does not support Non-BLocking Streams */
  std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
  switch (stream) {
    case hipStreamLegacy_ull:
      default_stream_zone = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
      per_thread_zones[jobId_t::hash_stream_ptr(td->threadId.host)] = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
      break;
    case hipStreamPerThread_ull: {
      per_thread_zones[jobId_t::hash_stream_ptr(td->threadId.host)] = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
      auto rows = zones.findByKey1(td->threadId.host);
      for (auto& row : rows)
        row.data = Zone_t{Zone_t::HOST_CONTROL, td->time_stamp};
      break;
    }
    default:
      zones[td->threadId.host][jobId_t::hash_stream_ptr(stream)] = {Zone_t::HOST_CONTROL, td->time_stamp};
      break;
  }
}
template<>
inline void StateMachine::sync_to_zone<StateMachine::Zone_t::DEVICE_CONTROL>(const StateMachine::WDataPtr_t td)
{
  /* NOTE: this implementation does not support Non-BLocking Streams */
  /* NOTE: DEVICE_CONTROL should not update zone transition time if the zone was already in DEVICE_CONTROL */
  std::uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
  switch (stream) {
    case hipStreamLegacy_ull:
      // if (default_stream_zone.state != Zone_t::DEVICE_CONTROL)
        default_stream_zone = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
    case hipStreamPerThread_ull: {
      auto _i = per_thread_zones.find(jobId_t::hash_stream_ptr(td->threadId.host));
      if (_i == per_thread_zones.end())
        per_thread_zones.emplace(jobId_t::hash_stream_ptr(td->threadId.host),
                                  Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp});
      else if (_i->second.state != Zone_t::DEVICE_CONTROL)
        _i->second = Zone_t{Zone_t::DEVICE_CONTROL, td->time_stamp};
      zones[td->threadId.host][jobId_t::hash_stream_ptr(stream)] = {Zone_t::DEVICE_CONTROL, td->time_stamp};
      break;  
    }
    default: {
      auto rows = zones.findByKey2(jobId_t::hash_stream_ptr(stream));
      if (rows.size() == 0u)
        zones[td->threadId.host][jobId_t::hash_stream_ptr(stream)] = {Zone_t::DEVICE_CONTROL, td->time_stamp};
      else
        for (auto& row : rows)
          if (row.data.state != Zone_t::DEVICE_CONTROL)
            row.data = {Zone_t::DEVICE_CONTROL, td->time_stamp};
      break;
    }
      
  }
}

inline const StateMachine::Zone_t StateMachine::get_host_zone(const StateMachine::WDataPtr_t HE) const
{
  // const Zone_t* zone = &Zone_t::DEFAULT_ZONE;
  // if (hipStreamLegacy_ull == DEFAULT_STREAM_BEHAVIOR()) {
    const auto _ptz = per_thread_zones.find(jobId_t::hash_stream_ptr(HE->threadId.host));
    if (_ptz != per_thread_zones.end() 
        /* && _ptz->second.trans_time >= zone->trans_time */)
      return _ptz->second;
  // } 
  // const auto _po = ownership.find(HE->ptr);
  // if (_po != ownership.end()) {
  //   const auto _z = zones.find(HE->threadId.host, _po->val);
  //   if (_z
  //       && _z->data.trans_time >= zone->trans_time)
  //     zone = &_z->data;
  // }
  return Zone_t::DEFAULT_ZONE;
}

inline const StateMachine::Zone_t StateMachine::get_device_zone(const StateMachine::WDataPtr_t DE) const
{
  /* NOTE: current implementation has all stored device thread id's are a hashed versions of the
      calling host's threadId rather than 0 if hipStreamPerThread is enabled */
  const StreamId stream = DE->threadId.device.job.STREAM;
  if (stream == 0u) // can only refer to the legacy default stream
    return default_stream_zone;
  
  auto _ptz = per_thread_zones.find(stream);
  if (_ptz != per_thread_zones.end())
    return _ptz->second; // case: default stream job (hipStreamPerThread)

  // hard method: look for last transition time that fits this kernel launch
  const Zone_t* most_recent = &Zone_t::DEFAULT_ZONE;
  for (auto& row : zones.findByKey2(stream))
    if (row.data.state == Zone_t::DEVICE_CONTROL
         && row.data.trans_time > most_recent->trans_time)
      most_recent = &row.data;
  return *most_recent;

  // // lazy method: assume device was meant to have control over the data
  // return {Zone_t::DEVICE_CONTROL, DE->time_stamp};
}

inline const StateMachine::Zone_t StateMachine::get_zone(const StateMachine::WDataPtr_t HE,
                                                          const StateMachine::WDataPtr_t DE) const
{
  const StreamId stream = DE->threadId.device.job.STREAM;
  const HostThreadId thread = HE->threadId.host;
  const auto i = zones.find(thread, stream);
  if (i)
    return i->data;
  
  if (HE->time_stamp <= DE->time_stamp) // looking in from a check DW/DR fn
    return get_device_zone(DE);

  if (hipStreamLegacy_ull == DEFAULT_STREAM_BEHAVIOR()) {
      if (stream != 0u)  // case: not legacy default stream, but no zone was found for this thread
        return Zone_t::DEFAULT_ZONE;
  } else {
    if (jobId_t::hash_stream_ptr(thread) != stream) // case: not per-thread default stream, but no zone was found
      return Zone_t::DEFAULT_ZONE;
  }
  return get_host_zone(HE);

  // const auto& zone = Zone_t::DEFAULT_ZONE;
  // if (stream == 0u) { // case: must be legacy default stream
  //   auto _pt = per_thread_zones.find(jobId_t::hash_stream_ptr(thread));
  //   if (_pt == per_thread_zones.end()) // case this thread has no clear zone to work from.
  //     return zone;
  //    _pt->second; //case: there is a ho
  // }

}


// << ------------------------------------------------------------------------------------------ >> 


StateMachine::Result::Status StateMachine::check_race_HR(const StateMachine::WDataPtr_t HR, 
                                                         const StateMachine::WDataPtr_t DE) 
{
  Zone_t zone = get_zone(HR, DE);

  if (zone.state == Zone_t::HOST_CONTROL  // not a critical zone
      || zone.trans_time > DE->time_stamp)   // OR the previous device event is stale/out-of-date
    return Result::GOOD;

  switch (DE->data & FILTER) {
    case ON_DEVICE | READ:
      return add_result(results,{Result::UNPROTECTED_HR,HR,DE,"WARN: Host Read from memory currently controlled by a Device"});
  
    case ON_DEVICE | WRITE:
    case ON_DEVICE | READ | WRITE: //for atomicrmw instructions
      if (HR->time_stamp == DE->time_stamp)
        return add_result(results,{Result::RACE_HR_DW,HR,DE,"RACE FOUND: HR->DW - The Host Read from memory controlled by a Device before it Wrote to it"});
      return add_result(results,{Result::POS_RACE_HR_DW,HR,DE,"WARN: Host Read from memory still controlled by a Device (no protections from HR->DW Race)"});
      
    default:
      return add_result(results,{Result::INTERNAL_ERROR,HR,DE,"[scabbard.rtl.sm.checkHR:ERR] unknown event data stored in state `memD` variable"});
  }
}


StateMachine::Result::Status StateMachine::check_race_HW(const StateMachine::WDataPtr_t HW, 
                                                         const StateMachine::WDataPtr_t DE) 
{
  Zone_t zone = get_zone(HW, DE);

  if (zone.state == Zone_t::HOST_CONTROL  // not a critical zone
      || zone.trans_time > DE->time_stamp)   // OR the previous device event is stale/out-of-date
    return Result::GOOD;

  switch (DE->data & FILTER) {
    case ON_DEVICE | READ: 
    case ON_DEVICE | READ | WRITE: // for atomicrmw instructions
      return add_result(results,{Result::RACE_DR_HW, DE, HW, "RACE FOUND: DR->HW - Host Wrote to a memory location currently controlled by a Device without a Sync."});

    case ON_DEVICE | WRITE:
      return add_result(results,{Result::UNPROTECTED_HW, DE, HW, "WARN: Host Wrote to memory still controlled by a Device"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR, DE, HW,"[scabbard.rtl.sm.checkHW:ERR] unknown event data stored in state `memD` variable"});
  }
}



StateMachine::Result::Status StateMachine::check_race_DR(const StateMachine::WDataPtr_t DR, 
                                                         const StateMachine::WDataPtr_t HE) 
{
  Zone_t zone = get_zone(HE, DR);

  if (zone.state != Zone_t::DEVICE_CONTROL)
    return add_result(results,{Result::INTERNAL_ERROR,DR,HE,"[scabbard.rtl.sm.checkDR:ERR] a Device kernel did not get ownership of its zone"});
  
  if (zone.trans_time > HE->time_stamp)
    return Result::GOOD;

  switch (HE->data & FILTER) 
  {
    case ON_HOST | READ: 
      return add_result(results,{Result::UNPROTECTED_HR,DR,HE,"WARN: Host Read from memory currently controlled by a Device"});

    case ON_HOST | WRITE:
    case ON_HOST | READ | WRITE:
      if (DR->time_stamp == HE->time_stamp)
        return add_result(results,{Result::RACE_DR_HW,DR,HE,"RACE FOUND: DR->HW - Host Wrote to a memory location currently controlled by a Device without a Sync."});
      return add_result(results,{Result::POS_RACE_DR_HW,DR,HE,"WARN: a Device Read from the the same location the Host Wrote to unprotected (no protection from DR->HW Race)"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR,DR,HE,"[scabbard.rtl.sm.checkDR:ERR] unknown event data stored in state `memH` variable"});
  }
}



StateMachine::Result::Status StateMachine::check_race_DW(const StateMachine::WDataPtr_t DW, 
                                                         const StateMachine::WDataPtr_t HE) 
{
  Zone_t zone = get_zone(HE, DW);

  if (zone.state == Zone_t::HOST_CONTROL)
    return add_result(results,{Result::INTERNAL_ERROR,DW,HE,"[scabbard.rtl.sm.checkDW:ERR] a Device kernel did not get ownership of its zone"});
  
  if (zone.trans_time > HE->time_stamp)
    return Result::GOOD;
    
  switch (HE->data & FILTER) 
  {
    case ON_HOST | READ:
    case ON_HOST | READ | WRITE:  // treating atomicrmw as reads in this case
      return add_result(results,{Result::RACE_HR_DW,HE,DW,"RACE FOUND: HR->DW - The Host Read from memory controlled by a Device before it Wrote to it"});
    
    case ON_HOST | WRITE:
      return add_result(results,{Result::UNPROTECTED_HW,HE,DW,"WARN: the Host Wrote to memory controlled by a Device"});

    default:
      return add_result(results,{Result::INTERNAL_ERROR,HE,DW,"[scabbard.rtl.sm.checkDW:ERR] unknown event data stored in state `memH` variable"});
  }
}


// << ------------------------------------------------------------------------------------------ >> 


inline std::ostream& operator << (std::ostream& out, const StateMachine::Result::Status& status)
{
  switch (status)
  {
    case StateMachine::Result::Status::READ_UNINIT_D: 
    case StateMachine::Result::Status::READ_UNINIT_H:
      return (out << "READ FROM UNINITIALIZED MEMORY");
    case StateMachine::Result::Status::RACE_DR_HW:
    case StateMachine::Result::Status::RACE_HR_DW:
      return (out << "DATA RACE FOUND");
    case StateMachine::Result::Status::POS_RACE_DR_HW:
    case StateMachine::Result::Status::POS_RACE_HR_DW:
    case StateMachine::Result::Status::UNPROTECTED_HR:
    case StateMachine::Result::Status::UNPROTECTED_HW:
      return (out << "POSSIBLE Data Race Found");
    case StateMachine::Result::Status::GOOD:
      return (out << "NO issues detected :)");
    case StateMachine::Result::Status::INTERNAL_ERROR:
      return (out << "Internal ERROR occurred in Scabbard RTL");
    default:
      return (out << "<UNKNOWN_STATUS>");
  }
}

inline bool operator == (const StateMachine::Result& L, const StateMachine::Result& R)
{
  return ( (L.status == R.status)
      && ((L.first_td && R.first_td) 
            ? *(L.first_td->metadata) == *(R.first_td->metadata) 
            : L.first_td.get() == R.first_td.get())
      && ((L.second_td && R.second_td) 
            ? *(L.second_td->metadata) == *(R.second_td->metadata) 
            : L.second_td.get() == R.second_td.get())
    );
}

inline bool operator < (const StateMachine::Result& l, const StateMachine::Result& r)
{
  //TODO: figure out what orderings we want for this
  if (l.status < r.status)
    return true;
  if (l.status > r.status)
    return false;
  //else l.status == r.status -> compare trace data's metadata
  return ((*(l.first_td->metadata) < *(r.first_td->metadata))
            || ((l.second_td && r.second_td) 
                && *(l.second_td->metadata) < *(r.second_td->metadata))
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
