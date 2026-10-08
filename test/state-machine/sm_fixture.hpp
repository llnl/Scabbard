
#pragma once

#include <gtest/gtest.h>
#include <scabbard/rtl/StateMachine.hpp>
#include <scabbard/rtl/GroupedPtr.hpp>
#include <atomic>
#include <unordered_map>


class SM_Tests : public testing::Test {
  using namespace scabbard::rtl;
  using DeviceVClks_t = std::unordered_map<StreamJobID,LTime_t>;
  using MetadataList_t = std::vector<SrcMetadata>;
  using GPtr_t = StateMachine::DataPtr_t;
  using EventData_t = StateMachine::Data_t;
private:
  StateMachine SM;
  GroupedPtrFactory<EventData_t> GF;
  LTime_t vClk = 0u;
  DeviceVClks_t dev_vClks{{0u,0u}};
  std::uint16_t next_jobID = 0u;
  std::uint16_t next_TSID = 4u;
  std::size_t next_MID = 1u;
  std::size_t next_MAdd = 2u;
  MetadataList_t metadata_store;

protected:
  SM_Tests() = default;

  std::uintptr_t malloc_event(HostThreadID TID, std::size_t SIZE=8u, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    std::uintptr_t PTR = next_MAdd;
    next_MAdd += SIZE; 
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | ALLOCATE_EVENT | _OPT_USED),
        ThreadId(TID),
        PTR,
        M,
        SIZE
      )));
    return PTR;
  }
  void free_event(HostThreadID TID, std::uintptr_t PTR, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | FREE_EVENT),
        ThreadId(TID),
        PTR,
        M,
        0u
      )));
  }

  void sync_event(HostThreadID TID, StreamID STREAM, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | SYNC_EVENT),
        ThreadId(TID),
        (std::uintptr_t)STREAM,
        M,
        0u
      )));
  }
  jobId_t&& launch_event(HostThreadID TID, StreamID STREAM, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    StreamJobID JID = next_jobID++;
    dev_vClks[jobID_t::hash_stream_ptr(STREAM)] = vClkv+1u;
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | LAUNCH_EVENT),
        ThreadId(TID),
        (std::uintptr_t)STREAM,
        M,
        0u
      )));
    return std::move(jobId_t(JID, STREAM));
  }

  void host_read_event(HostThreadID TID, std::uintptr_t PTR, std::size_t SIZE=0u, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | READ_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(TID),
        PTR,
        M,
        SIZE
      )));
  }
  void host_write_event(HostThreadID TID, std::uintptr_t PTR, std::size_t SIZE=0u, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_HOST | WRITE_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(TID),
        PTR,
        M,
        SIZE
      )));
  }

  void device_read_event(jobId_t JID, std::uintptr_t PTR, std::size_t SIZE=0u, SrcMetadata* const M=nullptr, DeviceThreadID* DTId = nullptr) {
    if (not M) M = generate_metadata();
    if (not DTId) DTId = generate_DeviceThreadID(JID);
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_DEVICE | READ_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(DTId),
        PTR,
        M,
        SIZE
      )));
  }
  void device_write_event(jobId_t JID, std::uintptr_t PTR, std::size_t SIZE=0u, DeviceThreadID* DTId = nullptr, SrcMetadata* const M=nullptr) {
    if (not M) M = generate_metadata();
    if (not DTId) DTId = generate_DeviceThreadID(JID);
    SM.append(GF.create(
      TraceData(
        ++vClk,
        ((InstrData) ON_DEVICE | READ_EVENT | ((SIZE) ? _OPT_USED : NONE)),
        ThreadId(DTId),
        PTR,
        M,
        SIZE
      )));
  }

  void device_job_completes_event(jobId_t JID) {
    LTime_t jVClk = dev_vClks[JID.STREAM];
    if (jVClk > vClk)
      vClk = jVClk;
  }

  HostThreadID spawn_host_thread() { return next_TSID++; }
  StreamID spawn_device_stream() { return next_TSID++; }

  DeviceThreadID& generate_DeviceThreadID(jobId_t JID) const { return DeviceThreadID(JID, dim3{0,0,0}, dim3{0,0,0}); }
  SrcMetadata* const generate_metadata() {
    metadata_store.emplace(metadata_store.size(), "test.cpp", "test", metadata_store.size(), 0u);
    return &metadata_store[metadata_store.size()-1u];
  }

  std::uintptr_t generate_mem_address() {}

  void run(std::size_t reductionQt=0u) { SM.run(reductionQt); }
  StateMachine::ResultList_t& get_results() { return SM.get_results(); } 

  bool _any_status_threshold(int threshold) {
    for (auto [res, count] : SM.get_results())
      if (res.status >= threshold)
        return true;
  }

  bool any_race() { return _any_status_threshold(StateMachine::Result::Status::POS_RACE_DR_HW); }
  bool any_true_race() { return _any_status_threshold(StateMachine::Result::Status::RACE_DR_HW) }
  bool any_race_lose() { return _any_status_threshold(StateMachine::Result::Status::UNPROTECTED_HW); }
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


};
