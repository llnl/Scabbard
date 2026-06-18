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

#ifndef hipStreamLegacy
#define hipStreamLegacy ((hipStream_t)1u)
#endif
#ifndef hipStreamPerThread
#define hipStreamPerThread ((hipStream_t)2u)
#endif

namespace scabbard {
namespace rtl {


inline void add_result(StateMachine::ResultList_t& results, const StateMachine::Result& res, std::size_t n=1u)
{
  auto it = results.find(res);
  if (it == results.end()) // case not encountered yet
    results.insert(std::make_pair(res,n));
  else                     // case encountered before
    it->second += n;
}


void StateMachine::run(std::uint64_t remainder_quotient)
{
  const InstrData FILTER = (
      InstrData::ON_HOST | InstrData::ON_DEVICE
      | InstrData::SYNC_EVENT | InstrData::DESYNC_EVENT
      | InstrData::READ | InstrData::WRITE
      | InstrData::ALLOCATE | InstrData::FREE
    );
  std::size_t dbg_i = 0u; //DEBUG
  std::size_t dbg_j = 0u; //DEBUG
  std::size_t dbg_k = 0u; //DEBUG
  MemTable_t::iterator idh = mem_dh.end();
  MemTable_t::iterator ihd = mem_hd.end();
  const std::size_t GOAL_SIZE = trace.size() >> remainder_quotient;
  while (trace.size() > GOAL_SIZE) {
    const DataPtr_t& td = trace.top();
    if (/* td->time_stamp == 0u || */ td->data == InstrData::NEVER) dbg_j++; //DEBUG
    if (td->data & InstrData::ON_GPU) dbg_k++; //DEBUG
    idh = mem_dh.end();
    ihd = mem_hd.end();
    switch (td->data & FILTER)
    {
      // << ======= Driver Events ======= >> 

      case ON_HOST | SYNC_EVENT: { 
          uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
          if (stream == (uintptr_t)hipStreamLegacy) {
            last_global_launch = UINT64_MAX;
            last_global_sync = td->time_stamp;
            last_stream_sync.clear();
          } else if (stream == (uintptr_t)hipStreamPerThread) {
            last_stream_sync[jobId_t::hash_stream_ptr(td->threadId.host)] = td->time_stamp; 
            last_stream_launch.erase(jobId_t::hash_stream_ptr(td->threadId.host));
          } else {
            last_stream_sync[jobId_t::hash_stream_ptr(td->ptr)] = td->time_stamp; 
            last_stream_launch.erase(jobId_t::hash_stream_ptr(td->ptr));
          }
        }
        break;

      case ON_HOST | LAUNCH_EVENT: {
          uintptr_t stream = (td->ptr) ? td->ptr : DEFAULT_STREAM_BEHAVIOR();
          if (stream == (uintptr_t)hipStreamLegacy) {
            last_global_sync = UINT64_MAX;
            last_global_launch = td->time_stamp;
            last_stream_launch.clear(); //NOTE: should this form also be copied???
          } else if (stream == (uintptr_t)hipStreamPerThread) {
            last_stream_sync.erase(jobId_t::hash_stream_ptr(td->threadId.host));
            last_stream_launch[jobId_t::hash_stream_ptr(td->threadId.host)] = td->time_stamp;
          } else {
            last_stream_sync.erase(jobId_t::hash_stream_ptr(td->ptr));
            last_stream_launch[jobId_t::hash_stream_ptr(td->ptr)] = td->time_stamp;  
          }
        }
        break;

      case ON_HOST | ALLOCATE:
        allocs[td->ptr] = td->_OPT_DATA;
        last_global_sync = td->time_stamp;   // currently assuming all allocate and free are synchonous
        break;

      case ON_HOST | FREE: {
        auto r = allocs.find(td->ptr);
        if (r == allocs.end()) {
          add_result(results,{Result::INTERNAL_ERROR, td, nullptr, 
                            "Bad alloc data (could not find hipMalloc associated with this hipFree in trace history)"}); //DEBUG
          last_global_sync = td->time_stamp;
          break;
          // return {{{INTERNAL_ERROR, nullptr, nullptr, "\n[scabbard.rtl:ERR] bad alloc data (could not find hipMalloc associated with hipFree in trace history)"}, 1ul}};
        }
        idh = mem_dh.erase(td->ptr, td->ptr+r->second);
        ihd = mem_hd.erase(td->ptr, td->ptr+r->second);
        allocs.erase(r);
        last_global_sync = td->time_stamp;   // currently assuming all allocate and free are synchonous
        break;
      }

      // << ===== Host Events ===== >> 

      case ON_HOST | READ | WRITE: // (DH)
      case ON_HOST | READ:
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_pos_race = 0ull;
          const Data_t* last_occurrence_pos_race;
          std::size_t occurrences_uninit = 0ull;
          const Data_t* last_occurrence_uninit;
          uintptr_t last_stop = td->ptr;
          for (const auto& _idh : mem_dh.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (_idh->start > last_stop && mem_hd.find(last_stop) == mem_hd.end()) {
              occurrences_uninit += _idh->start - last_stop;
              last_occurrence_uninit = _idh->val.unsafe_get();
            } else if (check_race_read_dh(td, _idh->val) != Result::GOOD) {
              occurrences_pos_race += _idh->stop - _idh->start;
              last_occurrence_pos_race = idh->val.unsafe_get();
            }
            last_stop = _idh->stop;
          }
          if (occurrences_pos_race)
            add_result(results,{Result::POS_RACE_DH, td, DataPtr_t::make(last_occurrence_pos_race), 
                                "Bulk CPU Read/MemcpyAsync occurs before any identifiably relevant Sync event"},
                       occurrences_pos_race);
          if (occurrences_uninit)
            add_result(results,{Result::POS_RACE_DH, td, DataPtr_t::make(last_occurrence_uninit), 
                                "The CPU read from Uninitialized memory durring a Bulk-CPU-Read/Memcpy"},
                       occurrences_uninit);
          mem_dh.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          idh = mem_dh.find(td->ptr);
        if (idh == mem_dh.end()) {// read with no preceding write
          if (mem_hd.find(td->ptr) == mem_hd.end()) // check to see if other mem table records an event
            add_result(results,{Result::READ_UNINIT_H, td, nullptr, "The CPU Read from Uninitialized Memory"},
                        ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u)); // read with no preceding write
          mem_dh.insert(td->ptr, td);
        } else {
            auto res = check_race_read_dh(td, idh->val);
            if (res != Result::GOOD) {
              add_result(results,{res, td, idh->val, "CPU Read occurs before any identifiably relevant Sync event"});
              if (idh->is_single()) idh->val = td; else mem_dh.insert(td->ptr, td);
            }
          }
        }
        if (not (td->data & WRITE)) //allow atomic instructions to fall through.
          break;
        
      case ON_HOST | WRITE: // (HD)
        if (td->data & InstrData::_OPT_USED) {
          std::size_t occurrences_race = 0ull;
          const Data_t* last_occurrence_race;
          for (auto _ihd : mem_hd.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (check_race_write_hd(td, _ihd->val) != Result::GOOD) {
              occurrences_race++;
              last_occurrence_race = _ihd->val.unsafe_get();
            }
          }
          if (occurrences_race)
            add_result(results,{Result::RACE_HD, DataPtr_t::make(last_occurrence_race), td,
                                "GPU Read from memory before a CPU Bulk-Memory-Write/Memcpy operation unbounded by a Sync. (DR->HW w/no Sync)"});
          mem_hd.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else {
          ihd = mem_hd.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
          // not first write of a pair (empty or just allocated)
          //                          AND the conditions with the last memory action checks out
          if (ihd != mem_hd.end() && check_race_write_hd(td, ihd->val) != Result::GOOD) 
            add_result(results,{Result::RACE_HD, ihd->val, td, 
                                "GPU Read from memory before a CPU Write unbounded by a Sync. (DR->HW w/no Sync)"});
          mem_hd.insert(td->ptr, td); // do nothing but insert into memory later.
        }
        break;


      // << ===== Device Events ===== >> 

      case ON_DEVICE | READ | WRITE: // (HD)
      case ON_DEVICE | READ:
        if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          std::size_t occurrences_pos_race = 0ull;
          const Data_t* last_occurrence_pos_race;
          std::size_t occurrences_uninit = 0ull;
          const Data_t* last_occurrence_uninit;
          uintptr_t last_stop = td->ptr;
          for (auto _ihd : mem_hd.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (_ihd->start > last_stop && mem_dh.find(last_stop) == mem_dh.end()) {
              occurrences_uninit += _ihd->start - last_stop;
              last_occurrence_uninit = _ihd->val.get();
            } else if (check_race_read_hd(td, _ihd->val) != Result::GOOD) {
              occurrences_pos_race += _ihd->stop - _ihd->start;
              last_occurrence_pos_race = ihd->val.get();
            }
            last_stop = _ihd->stop;
          }
          if (occurrences_pos_race)
            add_result(results,{Result::POS_RACE_HD, td, DataPtr_t::make(last_occurrence_pos_race), 
                                "Bulk-GPU-Read/Memcpy occurs before any identifiably relevant Sync event"},
                       occurrences_pos_race);
          if (occurrences_uninit)
            add_result(results,{Result::POS_RACE_HD, td, DataPtr_t::make(last_occurrence_uninit), 
                                "The GPU Read from Uninitialized memory durring a Bulk-GPU-Read/Memcpy"},
                       occurrences_uninit);
          mem_hd.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else { // single read
          ihd = mem_hd.find(td->ptr);
          if (ihd == mem_hd.end()) {// read with no preceding write
            if (mem_dh.find(td->ptr) == mem_dh.end()) // check to see if it was initalized in other mem table.
              add_result(results,{Result::READ_UNINIT_D, td, nullptr, "a GPU Read of Uninitialized Memory"},
                          ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u)); 
            mem_hd.insert(td->ptr, td);
          } else {
            auto res = check_race_read_hd(td, ihd->val);
            if (res != Result::GOOD) {
              add_result(results,{res, td, ihd->val, "GPU Read occurs before any identifiably relevant Sync event"});
              if (ihd->is_single()) ihd->val = td; else mem_hd.insert(td->ptr, td);
            }
          }
        }
        if (not (td->data & WRITE)) //allow atomic instructions to fall through.
          break; 

      case ON_DEVICE | WRITE: // (DH)
        if (td->data & InstrData::_OPT_USED) {
          std::size_t occurrences_race = 0ull;
          const Data_t* last_occurrence_race;
          for (auto _idh : mem_dh.find_all(td->ptr, td->ptr+td->_OPT_DATA)) {
            if (check_race_write_dh(td, _idh->val) != Result::GOOD) {
              occurrences_race++;
              last_occurrence_race = _idh->val.unsafe_get();
            }
          }
          if (occurrences_race)
            add_result(results,{Result::RACE_DH, DataPtr_t::make(last_occurrence_race), td,
                                "GPU Read from memory before a CPU Bulk-Memory-Write/Memcpy operation unbounded by a Sync. (DR->HW w/no Sync)"});
          mem_dh.insert(td->ptr, td->ptr+td->_OPT_DATA, td);
        } else {
          idh = mem_dh.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
          // not first write of a pair (empty or just allocated)
          //                          AND the conditions with the last memory action checks out
          if (idh != mem_dh.end() && check_race_write_dh(td, idh->val) != Result::GOOD) 
            add_result(results,{Result::RACE_DH, idh->val, td, 
                                "GPU Read from memory before a CPU Write unbounded by a Sync. (DR->HW w/no Sync)"});
          mem_dh.insert(td->ptr, td); // do nothing but insert into memory later.
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
  mem_dh.clear();
  last_global_sync = __UINT64_MAX__;
  last_stream_sync.clear();
}




StateMachine::Result::Status StateMachine::check_race_read_dh(const StateMachine::DataPtr_t& r, 
                                                              const StateMachine::DataPtr_t& w) 
{
  // if mem_dh stores a device write event
  if ((w->data & (ON_DEVICE | WRITE)) == (ON_DEVICE | WRITE)) { 
    // the device write happened after the last global sync event
    if (last_global_sync < w->time_stamp /* || last_global_sync > r->time_stamp */) 
        return Result::Status::POS_RACE_DH; // return a pos race warn
    auto res = last_stream_sync.find(w->threadId.device.job.STREAM);
    // the device write happened after the last global sync event or the read occurred after the last global sync event
    if (res != last_stream_sync.end() && (res->second < w->time_stamp /* || res->second >= r->time_stamp */ )) 
      return Result::Status::POS_RACE_DH; // return a poss race warn
  } // else    // if a read event we don't care yet (could be a double read)
    return Result::Status::GOOD;
}

StateMachine::Result::Status StateMachine::check_race_read_hd(const StateMachine::DataPtr_t& r, 
                                                              const StateMachine::DataPtr_t& w) 
{
  // if mem_dh stores a host write event 
  if ((w->data & (ON_HOST | WRITE)) == (ON_HOST | WRITE)) { 
    // the host write happened before the last global launch event which happened before this read
    if (last_global_launch < w->time_stamp /*|| last_global_launch > r->time_stamp*/ ) 
        return Result::Status::POS_RACE_HD; // return a poss race warn
    auto res = last_stream_launch.find(r->threadId.device.job.STREAM);
    // the host write happened after the last stream launch event or the read occurred after the last global launch event
    if (res != last_stream_launch.end() && (res->second < w->time_stamp /*|| res->second >= r->time_stamp*/ )) 
      return Result::Status::POS_RACE_HD; // return a poss race warn
  } // else    // if a write event we don't care yet (could be a double write)
    return Result::Status::GOOD;
}

StateMachine::Result::Status StateMachine::check_race_write_dh(const StateMachine::DataPtr_t& w, 
                                                               const StateMachine::DataPtr_t& r) 
{
  // if mem_dh stores a host read event
  if ((r->data & (ON_HOST | READ)) == (ON_HOST | READ)) { 
    // the last global sync event must have occurred (at all and) before the associated host read (else race)
    if (last_global_sync < UINT64_MAX && last_global_sync > r->time_stamp) 
        return Result::Status::RACE_DH; // return a race result
    auto res = last_stream_sync.find(r->threadId.device.job.STREAM);
    // the last stream sync event must have occurred (at all and) before the associated host read (else race)
    if (res != last_stream_sync.end() && (res->second > r->time_stamp)) 
      return Result::Status::RACE_DH; // return a race result
  } // else    // if not a host read we don't care ... yet
    return Result::Status::GOOD;
}

StateMachine::Result::Status StateMachine::check_race_write_hd(const StateMachine::DataPtr_t& w, 
                                                               const StateMachine::DataPtr_t& r) 
{
  // if mem_dh stores a device read event 
  if ((r->data & (ON_DEVICE | READ)) == (ON_DEVICE | READ)) { 
    // the last global launch event must have occurred (at all and) before the associated device read (else race)
    if (last_global_launch < UINT64_MAX && last_global_launch > r->time_stamp) 
        return Result::Status::RACE_HD; // return a warning
    auto res = last_stream_launch.find(w->threadId.device.job.STREAM);
    // the last stream launch event must have occurred (at all and) before the associated device read (else race)
    if (res != last_stream_launch.end() && (res->second > r->time_stamp))
      return Result::Status::RACE_HD; // return a warning
  } // else    // if not a device read event we don't care ... yet
    return Result::Status::GOOD;
}


inline std::ostream& operator << (std::ostream& out, const StateMachine::Result::Status& status)
{
  switch (status)
  {
    case StateMachine::Result::Status::RACE_DH:
    case StateMachine::Result::Status::RACE_HD:
      return (out << "DATA RACE FOUND");
      break;
    case StateMachine::Result::Status::POS_RACE_DH:
    case StateMachine::Result::Status::POS_RACE_HD:
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
      && ((L.read && R.read) ? L.read->metadata == R.read->metadata : L.read == R.read)
      && ((L.write && R.write) ? L.write->metadata == R.write->metadata : L.write == R.write)
    );
}

inline bool operator < (const StateMachine::Result& l, const StateMachine::Result& r)
{
  if (l.status > r.status)
    return false;
  return ( (l.status < r.status)
      || ((l.read && r.read) && l.read->metadata < r.read->metadata)
      || ((l.write && r.write) && l.write->metadata < r.write->metadata)
    );
}

explicit inline StateMachine& operator << (StateMachine& SM, StateMachine::DataPtr_t&& __Ptr)
{
  SM.trace.emplace(__Ptr);
  return SM;
}

explicit inline StateMachine& operator << (StateMachine& SM, StateMachine::DataPtr_t& Ptr)
{
  SM.trace.push(Ptr);
  return SM;
}

} //?namespace rtl
} //?namespace scabbard
