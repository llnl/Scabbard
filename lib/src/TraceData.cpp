/**
 * @file TraceData.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief where the library version of TraceData has some implementation data
 * @version alpha 0.0.1
 * @date 2023-06-16
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include <scabbard/TraceData.hpp>
#include <scabbard/Metadata.hpp>

#include <cstring>

namespace scabbard {


//   TraceData::TraceData(const char* buffer, const size_t WORD_LEN)
//   {
//     //TODO
// #   if (__WORDSIZE == 16)
//       if (WORD_LEN != __WORDSIZE)
//         throw std::exception("scabbard does not support handling trace data of a larger bit size on 16 bit arches!");
//       std::memcpy(this, buffer, sizeof(TraceData));
// #   elif (__WORDSIZE == 32)
//       switch (WORD_LEN)
//       {
//         case 16:
//         case 32:
//         case 64:
//         case 128:
//           return;
//         default:
//           throw std::exception("trace file has unsupported WORD_SIZE/WORD_LEN!");
//       }
// #   elif (__WORDSIZE == 64)
//       switch (WORD_LEN)
//       {
//         case 16:
//         case 32:
//         case 64:
//         case 128:
//           return;
//         default:
//           throw std::exception("trace file has unsupported WORD_SIZE/WORD_LEN!");
//       }
// #   elif (__WORDSIZE == 128)
//       switch (WORD_LEN)
//       {
//         case 16:
//         case 32:
//         case 64:
//         case 128:
//           return;
//         default:
//           throw std::exception("trace file has unsupported WORD_SIZE/WORD_LEN!");
//       }
// #   endif
//   }


} //?namespace scabbard
