/**
 * @file ReportWriter.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief output to desired location the final report of Scabbard's verification process
 * @version alpha 0.0.1
 * @date 2026-05-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <scabbard/rtl/StateMachine.hpp>

namespace scabbard {
namespace rtl {

void print_report(const StateMachine::ResultList_t& results);


} //?namespace rtl
} //?namespace scabbard
