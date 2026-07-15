/**
 * @file main.cpp
 * @author Andrew Osterhout
 * @brief 
 * @version 0.1
 * @date 2023-04-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <scabbard/TraceData.hpp>
#include <scabbard/Metadata.hpp>
#include <scabbard/rtl/osteam.hpp>
#include <scabbard/rtl/ReportWriter.hpp>
#include <scabbard/rtl/globals.hpp>


#include <sstream>
#include <bitset>

namespace scabbard{
namespace rtl {


inline ostream& operator << (ostream&, const StateMachine::DataPtr_t&);

void printResult(const StateMachine::Result& res);


void print_report(const StateMachine::ResultList_t& results) {
  SCAB_SOUT.setLogType("REPORT").setLabel("scab.rtl");
  if (results.size() == 0u) {
    SCAB_SOUT << nl() << "NO ISSUES FOUND" << endl();
    return;
  }
  size_t i = 0u;
  for (auto res : results) { 
    switch (res.first.status)
    {
      case StateMachine::Result::Status::RACE_DH:
      case StateMachine::Result::Status::RACE_HD:
      case StateMachine::Result::Status::POS_RACE_DH:
      case StateMachine::Result::Status::POS_RACE_HD:
        break;

      case StateMachine::Result::Status::GOOD:
        SCAB_SOUT << nl() << "- NO data races were found :)" << endl(); 
        continue;

      case StateMachine::Result::Status::INTERNAL_ERROR:
        SCAB_SOUT.setLogType("ERROR");
        SCAB_SOUT << nl() << nl() << "- { ERROR_MSG: \"" << res.first.msg
                  << "\", COUNT: " << res.second;
        if (res.first.read)
          SCAB_SOUT << indent(4u) << nl()
                    << "FROM: " << *res.first.read->metadata << nl()
                    << "  AT: " << res.first.read->time_stamp 
                    << dedent(4u) << nl();
        SCAB_SOUT.setLogType("REPORT");
        //exit(EXIT_FAILURE);
        continue;

      default:
        SCAB_SERR << SetLabel("scabbard.rtl.report") << SetLogType("ERROR") << nl() 
                  << "!unknown result code!" << endl();
        // exit(EXIT_FAILURE);
        return;   
    }

    SCAB_SOUT << nl()
              << "- {" << indent(2u) << nl()
              << "RESULT: " << res.first.status << nl()
              << "INFO: \"" << res.first.msg << '"' << nl()
              << "OCCURRENCE_COUNT: " << res.second << nl();
    if (not res.first.write) 
      SCAB_SOUT << "READ: " << indent(4u) << res.first.read << dedent(4u) << nl()
                << "WRITE: null" << dedent(2u) << nl();
    else if (res.first.read->time_stamp < res.first.write->time_stamp)
      SCAB_SOUT << "READ: " << indent(4u) << res.first.read  << dedent(4u) << nl()
                << "WRITE: " << indent(4u) << res.first.write << dedent(6u) << nl();
    else
      SCAB_SOUT << "WRITE: " << indent(4u) << res.first.write << dedent(4u) << nl()
                << "READ: " << indent(4u) << res.first.read  << dedent(6u) << nl();

    SCAB_SOUT << '}';
  }
  SCAB_SOUT.reset();
  return;
}

inline ostream& operator << (ostream& out, const SrcMetadata& data) {
  out << "[`" << data.fnName << "()`](\""<< data.srcFile << "\":" 
              << data.line << ':' << data.col << ')';
  return out;
}
inline ostream& operator << (ostream& out, const HostThreadId& threadId) {
  out << "0x" << std::hex << std::hash<std::thread::id>()(threadId) << std::dec;
  return out;
}
inline ostream& operator << (ostream& out, const jobId_t& jobId) {
  (*out) << "{stream: 0x" << std::hex << jobId.STREAM << std::dec 
         << ", job: " << jobId.JOB << '}'; 
  return out;
}
inline ostream& operator << (ostream& out, const blockId_t& id) {
  (*out) << "{x: " << id.x << ", y: " << id.y << ", z: " << id.z << '}';
  return out;
}
inline ostream& operator << (ostream& out, const threadId_t& id) {
  (*out) << "{x: " << id.x << ", y: " << id.y << ", z: " << id.z << '}';
  return out;
}
inline ostream& operator << (ostream& out, const DeviceThreadId& threadId) {
  out << '{' << indent(2u) << nl()
      << "job: " << threadId.job << nl()
      << "block: " << threadId.block << nl()
      << "thread: " << threadId.thread << dedent(2u) << nl()
      << '}';
  return out;
}
ostream& operator << (ostream& out, const InstrData& data) {
  std::bitset<16u> bs(data);
  return (out << std::string((data & InstrData::_RUNTIME_CONDITIONAL) ? "RT_COND, " : "")
        << std::string((data & InstrData::ON_DEVICE) ? "ON_DEVICE, " : "")
        << std::string((data & InstrData::ON_HOST) ? "ON_HOST, " : "")
        << std::string((data & InstrData::UNKNOWN_HEAP) ? "UNKNOWN_HEAP, " : "")
        << std::string((data & InstrData::DEVICE_HEAP) ? "DEVICE_HEAP, " : "")
        << std::string((data & InstrData::HOST_HEAP) ? "HOST_HEAP, " : "")
        << std::string((data & InstrData::ATOMIC) ? "ATOMIC, " : "")
        << std::string((data & InstrData::MANAGED_MEM) ? "MANAGED_MEM, " : "")
        << std::string((data & InstrData::READ) ? "READ, " : "")
        << std::string((data & InstrData::WRITE) ? "WRITE, " : "")
        << std::string((data & InstrData::ALLOCATE) ? "ALLOCATE, " : "")
        << std::string((data & InstrData::FREE) ? "FREE, " : "")
        << std::string((data & InstrData::LAUNCH_EVENT) ? "KERNEL_LAUNCH, " : "")
        << std::string((data & InstrData::SYNC_EVENT) ? "SYNC_EVENT, " : "")
        << std::string((data & InstrData::FREE) ? "FREE, " : "")
        << std::string((data & InstrData::_OPT_USED) ? "OPT_DATA, " : "")
        << std::string((data & InstrData::ASYNC) ? "ASYNC_OP, " : "")
        << "(0b" << bs << ")");
}

inline ostream& operator << (ostream& out, const TraceData& td) {
  if (td.data & ON_CPU) {
    out << '{' << indent(2u) << nl()
        << "time: " << td.time_stamp << nl()
        << "device: HOST / CPU" << nl()
        << "address: 0x" << std::hex << td.ptr << std::dec << nl() 
        << "srcLoc: " << *td.metadata << nl()
        << "metadata: {" << td.data << '}' << nl()
        << "threadID: " << td.threadId.host
        << dedent(2u) << nl() << '}';
  } else {
    out << '{' << indent(2u) << nl()
        << "time: " << td.time_stamp << nl()
        << "device: DEVICE / GPU" << nl()
        << "address: 0x" << std::hex << td.ptr << std::dec << nl() 
        << "srcLoc: " << *td.metadata << nl() //might cause issues with memory accessibility
        << "metadata: [" << td.data << ']' << nl()
        << "threadID: " << td.threadId.device
        << dedent(2u) << nl() << '}';
  }
  return out;
}
inline ostream& operator << (ostream& out, const StateMachine::DataPtr_t& _td) {
  if (_td)
    return (out << *_td);
  return (out << "null");
}


} //?namespace rtl 
} //?namespace scabbard
