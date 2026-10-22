
#pragma once

#define SCABBARD_UNIT_TEST

#include <gtest/gtest.h>
#include <scabbard/rtl/StateMachine.hpp>
#include <scabbard/rtl/GroupedPtr.hpp>
#include <scabbard/rtl/calls.hpp>
#include <atomic>
#include <unordered_map>

#define hipStreamLegacy_ull 1u
#define hipStreamPerThread_ull 2u



namespace scabbard {
namespace rtl {
namespace test {
class SM_Tests : public testing::Test {
  using DeviceVClks_t = std::unordered_map<StreamJobId,LTime_t>;
  using MetadataList_t = std::vector<const SrcMetadata*>;
  using GPtr_t = StateMachine::DataPtr_t;
  using EventData_t = StateMachine::Data_t;
private:
  GroupedPtrFactory<const EventData_t> GF;
  StateMachine SM;
  LTime_t vClk = 0u;
  DeviceVClks_t dev_vClks{{0u,1u}};
  std::uint16_t next_jobId = 0u;
  std::size_t next_TSId = 4u;
  std::size_t next_MId = 1u;
  std::size_t next_MAdd = 2u;
  MetadataList_t metadata_store;

protected:
  SM_Tests() = default;

  ~SM_Tests() {
    for (auto* ptr : metadata_store)
      if (ptr) {
        delete ptr;
        ptr = nullptr;
      }
  }

  std::uintptr_t malloc_event(HostThreadId TId, std::size_t SIZE=8u, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    std::uintptr_t PTR = generate_mem_address(SIZE); 
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | ALLOCATE_EVENT | _OPT_USED),
        ThreadId(TId),
        PTR,
        M,
        SIZE
      )));
    return PTR;
  }
  void free_event(HostThreadId TId, std::uintptr_t PTR, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | FREE_EVENT),
        ThreadId(TId),
        PTR,
        M,
        0u
      )));
  }

  void sync_event(HostThreadId TId, hipStream_t STREAM=nullptr, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | SYNC_EVENT),
        ThreadId(TId),
        (std::uintptr_t)STREAM,
        M,
        0u
      )));
  }
  jobId_t launch_event(HostThreadId TId, hipStream_t STREAM=nullptr, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    StreamJobId JId = next_jobId++;
    if (not STREAM and DEFAULT_STREAM_BEHAVIOR() == hipStreamPerThread_ull)
      STREAM = (hipStream_t)TId;
    dev_vClks[jobId_t::hash_stream_ptr(STREAM)] = vClk+1u;
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | LAUNCH_EVENT),
        ThreadId(TId),
        (std::uintptr_t)STREAM,
        M,
        0u
      )));
    return jobId_t{JId, jobId_t::hash_stream_ptr(STREAM)};
  }

  void host_read_event(HostThreadId TId, std::uintptr_t PTR, std::size_t SIZE=0u, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | READ_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(TId),
        PTR,
        M,
        SIZE
      )));
  }
  void host_write_event(HostThreadId TId, std::uintptr_t PTR, std::size_t SIZE=0u, const SrcMetadata* M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      EventData_t(
        ++vClk,
        ((InstrData) ON_HOST | WRITE_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(TId),
        PTR,
        M,
        SIZE
      )));
  }

  void device_read_event(jobId_t JId, std::uintptr_t PTR, std::size_t SIZE=0u, 
                          const SrcMetadata* M=nullptr, const DeviceThreadId* DTId = nullptr) {
    if (not M) M = generate_metadata();
    ThreadId TId = ((DTId) ? ThreadId(*DTId) : ThreadId(generate_DeviceThreadId(JId)));
    SM.append(GF.create(
      EventData_t(
        ++dev_vClks[JId.STREAM],
        ((InstrData) ON_DEVICE | READ_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        TId,
        PTR,
        M,
        SIZE
      )));
  }
  void device_write_event(jobId_t JId, std::uintptr_t PTR, std::size_t SIZE=0u, 
                          const SrcMetadata* M=nullptr, const DeviceThreadId* DTId = nullptr) {
    if (not M) M = generate_metadata();
    ThreadId TId = ((DTId) ? ThreadId(*DTId) : ThreadId(generate_DeviceThreadId(JId)));
    SM.append(GF.create(
      EventData_t(
        ++dev_vClks[JId.STREAM],
        ((InstrData) ON_DEVICE | WRITE_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        TId,
        PTR,
        M,
        SIZE
      )));
  }

  void device_job_completes_event(jobId_t JId) {
    LTime_t jVClk = dev_vClks[JId.STREAM];
    if (jVClk > vClk)
      vClk = jVClk;
  }

  HostThreadId spawn_host_thread() { return next_TSId++; }
  hipStream_t spawn_device_stream() { return (hipStream_t)next_TSId++; }

  DeviceThreadId generate_DeviceThreadId(jobId_t JId) const { return DeviceThreadId(JId, dim3{0,0,0}, dim3{0,0,0}); }
  const SrcMetadata* const generate_metadata() {
    return metadata_store.emplace_back(new SrcMetadata{metadata_store.size(), "test.cpp", "test", metadata_store.size(), 0u});
  }

  std::uintptr_t generate_mem_address(std::size_t SIZE=8u) {
    std::uintptr_t PTR = next_MAdd;
    next_MAdd += SIZE;
    return PTR;
  }

  void run(std::size_t reductionQt=0u) { SM.run(reductionQt); }
  const StateMachine::ResultList_t& get_results() { return SM.get_results(); } 

  bool _any_status_threshold(int threshold) {
    for (auto [res, count] : SM.get_results())
      if (res.status >= threshold)
        return true;
    return false;
  }

  bool any_race() { return _any_status_threshold(StateMachine::Result::Status::POS_RACE_DR_HW); }
  bool any_true_race() { return _any_status_threshold(StateMachine::Result::Status::RACE_DR_HW); }
  bool any_race_loose() { return _any_status_threshold(StateMachine::Result::Status::UNPROTECTED_HW); }
  bool any_issues() { return _any_status_threshold(StateMachine::Result::Status::READ_UNINIT_D); }

  std::size_t status_count(StateMachine::Result::Status target_status) {
    std::size_t total = 0u;
    for (auto [res, count] : SM.get_results())
      if (res.status == target_status)
        total += count;
    return total;
  }
  std::size_t status_count_all() {
    std::size_t total = 0u;
    for (auto [res, count] : SM.get_results())
      total += count;
    return total;
  }
  std::optional<std::pair<std::size_t,StateMachine::ResultList_t>> 
  get_internal_errors() {
    std::size_t total = 0u;
    StateMachine::ResultList_t list;
    for (auto [res, count] : SM.get_results())
      if (res.status < 0) {
        list.emplace(res, count);
        total += count;
      }
    if (total > 0)
      return {{total,list}};
    return {};
  }


};


void PrintTo(const std::optional<std::pair<std::size_t,StateMachine::ResultList_t>>& data, std::ostream* out) {
  if (not data) return;
  *out << "\n[scabbard.rtl.sm.test:ERR] Internal Errors Detected (x" << data->first << "):";
  for (auto [res, count] : data->second)
    *out << "\n[scabbard.rtl.sm.test:ERR]   - (x" << count <<") \"" << res.msg << '"';
  *out << '\n';
  return;
}

void PrintTo(const StateMachine::ResultList_t& data, std::ostream* out) {
  *out << "\n[scabbard.rtl.sm.test:INFO] Results found by state machine:";
  for (auto [res, count] : data)
    *out << "\n[scabbard.rtl.sm.test:INFO]   - (x" << count <<") `" << res.status << "`: \"" << res.msg << '"';
  *out << '\n';
  return;
}

// std::ostream& operator << (std::ostream& out, const std::optional<std::pair<std::size_t,StateMachine::ResultList_t>>& data) {
//   if (not data) return out;
//   out << "\n[scabbard.rtl.sm.test:ERR] Internal Errors Detected (x" << data->first << "):";
//   for (auto [res, count] : data->second)
//     out << "\n[scabbard.rtl.sm.test:ERR]   - (x" << count <<") \"" << res.msg << '"';
//   return (out << '\n');
// }

} //? namespace test
} //? namespace rtl
} //? namespace scabbard

template<>
testing::Message& testing::Message::operator << (const std::optional<std::pair<std::size_t,scabbard::rtl::StateMachine::ResultList_t>>& data) {
  if (not data) return *this;
  *this << "\n[scabbard.rtl.sm.test:ERR] Internal Errors Detected (x" << data->first << "):";
  for (auto [res, count] : data->second)
    *this << "\n[scabbard.rtl.sm.test:ERR]   - (x" << count <<") \"" << res.msg << '"';
  *this << '\n';
  return *this;
}

template<>
testing::Message& testing::Message::operator << (const scabbard::rtl::StateMachine::Result::Status& status) {
  using Status = scabbard::rtl::StateMachine::Result;
  switch (status)
  {
    case Status::READ_UNINIT_D: return *this << "READ_UNINIT_D";
    case Status::READ_UNINIT_H: return *this << "READ_UNINIT_H";
    case Status::RACE_DR_HW: return *this << "RACE_DR_HW";
    case Status::RACE_HR_DW: return *this << "READ_UNINIT_D";
    case Status::POS_RACE_DR_HW: return *this << "POS_RACE_DR_HW";
    case Status::POS_RACE_HR_DW: return *this << "POS_RACE_HR_DW";
    case Status::UNPROTECTED_HR: return *this << "UNPROTECTED_HR";
    case Status::UNPROTECTED_HW: return *this << "UNPROTECTED_HW";
    case Status::GOOD: return *this << "GOOD";
    case Status::INTERNAL_ERROR: return *this << "INTERNAL_ERROR";
    default: return *this << "<UNKNOWN_STATUS>";
  }
  return *this;
}

template<>
testing::Message& testing::Message::operator << (const scabbard::rtl::StateMachine::ResultList_t& data) {
  *this << "\n[scabbard.rtl.sm.test:INFO] Results found by state machine:";
  for (auto [res, count] : data)
    *this << "\n[scabbard.rtl.sm.test:INFO]   - (x" << count <<") `" << res.status << "`: \"" << res.msg << '"';
  *this << '\n';
  return *this;
}
