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

namespace scabbard {
namespace rtl {

StateMachine::StateMachine(StateMachine::Trace_t& trace_)
  : trace(trace_)
{}


inline void add_result(StateMachine::ResultList_t& results, const StateMachine::Result& res, std::size_t n=1u)
{
  auto it = results.find(res);
  if (it == results.end()) // case not encountered yet
    results.insert(std::make_pair(res,1ul));
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
  const std::size_t GOAL_SIZE = trace.size() >> remainder_quotient;
  while (not trace.empty() && trace.size() > GOAL_SIZE) {
    const DataPtr_t& td = trace.top();
    if (/* td->time_stamp == 0u || */ td->data == InstrData::NEVER) dbg_j++; //DEBUG
    if (td->data & InstrData::ON_GPU) dbg_k++; //DEBUG
    MemTable_t::iterator idh = mem_dh.end();
    MemTable_t::iterator ihd = mem_hd.end();
    switch (td->data & FILTER)
    {
      // << ======= Driver Events ======= >> 

      case ON_HOST | SYNC_EVENT: //NOTE: hipMemcpy might fuck this up look into separating trace data fields into two entires in some tbd order
        if (td->ptr == 0) {
          last_global_launch = UINT64_MAX;
          last_global_sync = td->time_stamp;
          last_stream_sync.clear();
        } else {
          last_stream_sync[jobId_t::hash_stream_ptr(td->ptr)] = td->time_stamp; 
          last_stream_launch[jobId_t::hash_stream_ptr(td->ptr)] = UINT64_MAX; //replace with erase?
        }
        break;

      case ON_HOST | LAUNCH_EVENT:
        //NOTE: currently just used to help when debugging
        if (td->ptr == 0ul) {
          last_global_sync = UINT64_MAX;
          last_global_launch = td->time_stamp;
          last_stream_launch.clear(); //NOTE: should this form also be copied???
        } else {
          last_stream_sync[jobId_t::hash_stream_ptr(td->ptr)] = UINT64_MAX;  //replace with erase?
          last_stream_launch[jobId_t::hash_stream_ptr(td->ptr)] = td->time_stamp;  
        }
        break;

      case ON_HOST | ALLOCATE:
        allocs[td->ptr] = td->_OPT_DATA;
        last_global_sync = td->time_stamp;   // currently assuming all allocate and free are synchonous
        break;

      case ON_HOST | FREE: {
        auto r = allocs.find(td->ptr);
        if (r == allocs.end()) {
          add_result(results,{Status::INTERNAL_ERROR, nullptr, nullptr, "\n[scabbard.rtl:ERR] bad alloc data (could not find hipMalloc associated with hipFree in trace history)"}); //DEBUG
          last_global_sync = td->time_stamp; //DEBUG
          break; //DEBUG
          // return {{{INTERNAL_ERROR, nullptr, nullptr, "\n[scabbard.rtl:ERR] bad alloc data (could not find hipMalloc associated with hipFree in trace history)"}, 1ul}};
        }
        for (idh = mem_dh.find(td->ptr); idh != mem_dh.end() && idh->second->ptr < td->ptr+r->second; ++idh)
          idh = mem_dh.erase(idh);
        for (ihd = mem_hd.find(td->ptr); ihd != mem_hd.end() && ihd->second->ptr < td->ptr+r->second; ++ihd)
          ihd = mem_hd.erase(ihd);
        allocs.erase(r);
        last_global_sync = td->time_stamp;   // currently assuming all allocate and free are synchonous
        break;
      }

      // << ===== Host Events ===== >> 

      case ON_HOST | READ | WRITE:
      case ON_HOST | READ:
        idh = mem_dh.find(td->ptr);
        if (idh == mem_dh.end()) {// read with no preceding write
          // if (td->data & _RUNTIME_CONDITIONAL && td->data & HOST_HEAP)) // if the memory location was conditional and verified to be on the host heap
          //   break;  // it is not likely to be relevant to the gpu; skip it
          add_result(results,{Status::WARNING, td, nullptr, "Read with no preceding/matching Write"},
                      ((td->data & _OPT_USED) ? td->_OPT_DATA : 1u)); // read with no preceding write
          mem_dh[td->ptr] = td;
          break;
        } else if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          Result::Status res = GOOD;
          for (auto _idh = idh; _idh != mem_dh.end() && _idh->second->ptr < td->ptr+td->_OPT_DATA; ++_idh) {
            auto res = check_race_read_dh(td, _idh->second);
            if (res != Status::GOOD) {
              mem_dh[_idh->second->ptr] = td;
              break;
            }
          }
          if (res != GOOD)
            add_result(results,{res, td, idh->second, "Bulk Read/MemCpyAsync occurs before any identifiably relevant sync event"},
                       td->_OPT_DATA);
        } else { // single read
          auto res = check_race_read_dh(td, idh->second);
            if (res != Status::GOOD) {
              add_result(results,{res, td, idh->second, "Read occurs before any identifiably relevant sync event"});
              mem_dh[idh->second->ptr] = td;
              break;
            }
        }
        if (not td->data & WRITE) //allow atomic instructions to fall through.
          break;
        
      case ON_HOST | WRITE:
        ihd = mem_hd.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
        // first write of a pair (empty or just allocated)
        if (ihd == mem_hd.end()) 
          mem_hd[td->ptr] = td;
        // OR the last operation on the mem_dh space was a read 
        else if (not (idh->second->data & InstrData::READ)) 
          mem_hd[td->ptr] = td;
        else // This is probably a race  //NOTE: expand this to search for read times compared to last desync event? (after ending mem_dh wipes during free events)
          add_result(results,{ERROR, ihd->second, td, "Data Written to after idh was Read from"});
        break;


      // << ===== Device Events ===== >> 

      case ON_DEVICE | READ | WRITE:
      case ON_DEVICE | READ:
        ihd = mem_hd.find(td->ptr);
        if (ihd == mem_hd.end()) {// read with no preceding write
          add_result(results,{Status::WARNING, td, nullptr, "Read with no preceding/matching Write"}); 
          mem_hd[td->ptr] = td;
          break;
        } else if (td->data & InstrData::_OPT_USED) { // bulk read (memcpy device to host)
          for (; ihd != mem_hd.end() && ihd->second->ptr < td->ptr+td->_OPT_DATA; ++ihd) {
            auto res = check_race_read_hd(td, ihd->second);
            if (res != Status::GOOD) {
              add_result(results,{res, td, ihd->second, "Bulk Read/device-memcpy occurs before any identifiably relevant launch event"});
              mem_hd[ihd->second->ptr] = td;
              break;
            }
          }
        } else { // single read
          auto res = check_race_read_hd(td, ihd->second);
            if (res != Status::GOOD) {
              add_result(results,{res, td, ihd->second, "Read occurs before any identifiably relevant sync event"});
              mem_hd[ihd->second->ptr] = td;
              break;
            }
        }
        if (not td->data & WRITE) //allow atomic instructions to fall through.
          break;

      case ON_DEVICE | WRITE:
        idh = mem_dh.find(td->ptr); //TODO: \/ logic below needs a refresh (might be flawed) \/
        if (idh == mem_dh.end()) // first write of a pair (empty or just allocated)
          mem_dh[td->ptr] = td;
        else if (not (idh->second->data & InstrData::READ)) // OR the last operation on the mem_dh space was a read 
          mem_dh[td->ptr] = td;
        else // This is probably a race  //NOTE: expand this to search for read times compared to last desync event? (after ending mem_dh wipes during free events)
          add_result(results,{ERROR, idh->second, td, "Data Written to after idh was Read from"});
        break;

      default:
        break;
    }
    dbg_i++; //DEBUG
  }
}


void StateMachine::reset()
{
  mem_dh.clear();
  last_global_sync = __UINT64_MAX__;
  last_stream_sync.clear();
}




StateMachine::Result::Status StateMachine::check_race_read_dh(const StateMachine::DataPtr_t& r, 
                                                           const StateMachine::DataPtr_t& o) 
{
  // mem_dh[o->ptr] = &r;
  // if mem_dh stores a write event
  if (o->data & (ON_DEVICE | WRITE)) { 
    // the write happened after the last global sync event
    if (last_global_sync < o->time_stamp || last_global_sync > r->time_stamp ) 
        return Result::Status::WARNING; // return a warning
    auto res = last_stream_sync.find(o->threadId.device.job.STREAM);
    // the write happened after the last global sync event or the read occurred after the last global sync event
    if (res != last_stream_sync.end() && (res->second < o->time_stamp || res->second >= r->time_stamp )) 
      return Result::Status::WARNING; // return a warning
  } // else    // if a read event we don't care yet (could be a double read)
    mem_dh[o->ptr] = r;
    return Result::Status::GOOD;
}

StateMachine::Result::Status StateMachine::check_race_read_hd(const StateMachine::DataPtr_t& r, 
                                                           const StateMachine::DataPtr_t& o) 
{
  // mem_hd[o->ptr] = &r;
  // if mem_dh stores a write event 
  if (o->data & (ON_HOST | WRITE)) { 
    // the write happened after the last global launch event
    if (last_global_launch < o->time_stamp || last_global_launch > r->time_stamp ) 
        return Result::Status::WARNING; // return a warning
    auto res = last_stream_launch.find(r->threadId.device.job.STREAM);
    // the write happened after the last global launch event or the read occurred after the last global launch event
    if (res != last_stream_launch.end() && (res->second < o->time_stamp || res->second >= r->time_stamp )) 
      return Result::Status::WARNING; // return a warning
  } // else    // if a read event we don't care yet (could be a double read)
    mem_hd[o->ptr] = r;
    return Result::Status::GOOD;
}


inline std::ostream& operator << (std::ostream& out, const StateMachine::Result::Status& status)
{
  switch (status)
  {
    case StateMachine::Result::Status::ERROR:
      return (out << "DATA RACE FOUND");
      break;
    case StateMachine::Result::Status::WARNING:
      return (out << "POSSIBLE Data Race Found");
    case StateMachine::Result::Status::GOOD:
      return (out << "NO data races detected");
    case StateMachine::Result::Status::INTERNAL_ERROR:
      return (out << "Internal ERROR occurred in Scabbard RTL");
    default:
      return (out << "<UNKNOWN>");
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
      || ((l.write && r.write) && l.write->metadata < r.other.write->metadata)
    );
}

explicit inline StateMachine& operator << (StateMachine& SM, StateMachine::DataPtr_t&& __Ptr)
{
  SM.trace.insert(__Ptr);
  return SM;
}

explicit inline StateMachine& operator << (StateMachine& SM, StateMachine::DataPtr_t& Ptr)
{
  SM.trace.insert(Ptr);
  return SM;
}

} //?namespace rtl
} //?namespace scabbard
