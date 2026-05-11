/**
 * @file Enums.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Convenient place to put shared enums
 * @version alpha 0.0.1
 * @date 2023-05-16
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <cstdint>
#include <ostream>
#include <sstream>
#include <bitset>


namespace scabbard {

   /**
    * @brief ENUM BYTE MAP\n
    *   \code{.txt}
    *     0b_0000_0000_0000_0000
    *        ^^^^ ^^^^ ^^^^ ^^^^
    *        |||| |||| |||| |||L Runtime Conditional
    *        |||| |||| |||| ||L Optional data used
    *        |||| |||| |||| |L Instr-ed in DEVICE (GPU) module
    *        |||| |||| |||| L Instr-ed in HOST (CPU) module
    *        |||| |||| |||L Instr as ALLOCATE event
    *        |||| |||| ||L Instr as READ event
    *        |||| |||| |L Instr as FREE event
    *        |||| |||| L Instr as WRITE event
    *        |||| |||L Instr as SYNC event
    *        |||| ||L Instr as a DeSYNC/Kernel-Launch event
    *        |||| |L Is an ATOMIC operation
    *        |||| L Ptr is in a UNKNOWN memory heap
    *        |||L Ptr is in a MANAGED MEM heap
    *        ||L Ptr is in DEVICE HEAP memory
    *        |L Ptr is in (registered) HOST HEAP memory
    *        L Operation performed ASYNC
    *   \endcode
    */
  enum InstrData : std::uint16_t {
    // This inst should never be instrumented (no chance of being of interest in traces)
    NEVER                 = 0,
    NO                    = 0, // just a useful duplicate of NEVER
    NONE                  = 0, // just a useful duplicate of NEVER
    LOCAL                 = 0, // we don't want to instrument if mem is confirmed scope/block Local
    // This might be of interest, but we can only know after a runtime conditional is run.
    _RUNTIME_CONDITIONAL  = 1<<0,  // Note: conditional is only valid for HOST/CPU
    // This is used to indicate that the trace data's optional data slot is used
    _OPT_USED             = 1<<1,
    _OPT_DATA             = 1<<1,
    _OPT_DATA_USED        = 1<<1,
    // This inst should always be instrumented (no chance of NOT being of interest in traces) 
    ALWAYS                = (1<<2)|(1<<3),
    // If this is on the GPU/Device it should be instrumented
    ON_DEVICE             = 1<<2,
    ON_GPU                = 1<<2,
    // If this is on the CPU/Host it shoule be instrumented
    ON_HOST               = 1<<3,
    ON_CPU                = 1<<3,
    //
    ALLOCATE              = 1<<4,
    ALLOCATE_EVENT        = 1<<4,
    //
    READ                  = 1<<5,
    READ_EVENT            = 1<<5,
    //
    FREE                  = 1<<6,
    FREE_EVENT            = 1<<6,
    //
    WRITE                 = 1<<7,
    WRITE_EVENT           = 1<<7,
    //
    SYNC                  = 1<<8,
    SYNC_EVENT            = 1<<8,
    //
    LAUNCH_EVENT          = 1<<10,
    DESYNC_EVENT          = 1<<10,
    // this memory should be treated as atomic
    ATOMIC                = 1<<9,
    // 
    UNKNOWN_HEAP          = 1<<11,
    //
    MANAGED_MEM           = 1<<12,
    //
    DEVICE_HEAP           = 1<<13,
    //
    HOST_HEAP             = 1<<14,
    //
    ASYNC_OPERATION       = 1<<15,
    ASYNC                 = 1<<15
  };

  inline InstrData operator | (InstrData l, InstrData r) 
  {
    return (InstrData)(static_cast<std::uint16_t>(l) | static_cast<std::uint16_t>(r));
  }
  inline InstrData& operator |= (InstrData& l, InstrData r) 
  {
    return (l = (InstrData)(static_cast<std::uint16_t>(l) | static_cast<std::uint16_t>(r)));
  }
  inline InstrData operator & (InstrData l, InstrData r) 
  {
    return (InstrData)(static_cast<std::uint16_t>(l) & static_cast<std::uint16_t>(r));
  }
  inline InstrData& operator &= (InstrData& l, InstrData r) 
  {
    return (l = (InstrData)(static_cast<std::uint16_t>(l) & static_cast<std::uint16_t>(r)));
  }

  std::ostream& operator << (std::ostream& out, const InstrData& data) noexcept
  {
    std::bitset<16> bs(data);
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

} //?namespace scabbard