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


#include <scabbard/Enums.hpp>

namespace scabbard {


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


namespace instr {

  inline std::string to_string(const ModuleType& modTy)
  {
    switch (modTy) {
      case ModuleType::HOST:
        return "HOST";
      case ModuleType::DEVICE:
        return "DEVICE";
      case ModuleType::UNKNOWN_MODULE:
      default:
        return "<UNKNOWN_MODULE_TYPE>";
    }
  }



} //?namespace instr

std::ostream& operator << (std::ostream& out, const instr::ModuleType& modTy) noexcept
{
  return (out << ::scabbard::instr::to_string(modTy));
}

} //?namespace scabbard
