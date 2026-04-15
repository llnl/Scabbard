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

#include <nlohmann/json.hpp>

#include <cstdint>
#include <ostream>
#include <sstream>
#include <bitset>


namespace scabbard {
namespace instr {

  enum ModuleType { HOST=0, DEVICE=1, UNKNOWN_MODULE=-1 };

  NLOHMANN_JSON_SERIALIZE_ENUM( ModuleType, {
      {ModuleType::HOST, "HOST"},
      {ModuleType::DEVICE, "DEVICE"},
      {ModuleType::UNKNOWN_MODULE, "<UNKNOWN_MODULE_TYPE>"}
  })

  inline std::string to_string(const ModuleType& MOD_TY);

} //?namespace instr

std::ostream& operator << (std::ostream& out, const instr::ModuleType& modTy) noexcept;

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

  std::ostream& operator << (std::ostream& out, const InstrData& data) noexcept;

} //?namespace scabbard