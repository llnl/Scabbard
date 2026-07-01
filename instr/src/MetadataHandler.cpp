/**
 * @file MetadataParser.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief a utility class for organizing and handling lazy storage of appropriate metadata in
 *        a runtime accessible manner
 * @version alpha 0.0.1
 * @date 2023-06-15
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "MetadataHandler.hpp"

#include <scabbard/instr-names.def>

#include <llvm/IR/DerivedTypes.h> 
#include <llvm/IR/IRBuilder.h>

#include <cassert> 
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>
#include <sstream>

#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <sys/file.h>  
#include <unistd.h> 
#include <errno.h>


namespace scabbard {
  namespace instr {


    llvm::Constant* LocResult::get_as_constant(llvm::LLVMContext& CTX) const 
    {
      return llvm::ConstantInt::get(llvm::IntegerType::get(CTX, 64u), llvm::APInt(64u, srcID, false)); 
    }


    // << ------------------------------------------------------------------------------------------ >> 

    const uint64_t MetadataHandler::default_srcID = 0ul;
    const uint64_t MetadataHandler::libtrace_srcID = 1ul;
    const uint64_t MetadataHandler::hipAPI_srcID = 2ul;

    inline bool file_exists(const std::string& name);
    // inline MetadataJSONFile_t initial_metadata();

    inline int acquire_file_lock(const std::string& filepath);
    inline void release_file_lock(int LFD);


    inline std::string get_full_file_path(const llvm::DIFile& DIF);
    inline std::string get_str_rep(const llvm::DebugLoc& DIL, const llvm::DIFile& DIF, const ModuleType MOD_TY);
    inline std::unordered_map<std::string,uint64_t> convert_to_str_ref_set(const MetadataJSONFile_t& original);


    LocResult MetadataHandler::_trace_file(llvm::Function& F, const llvm::DebugLoc& DIL, const llvm::DIFile& DIF, const ModuleType MOD_TY) const
    {
      // acquire metadata file lock
      const int LFD = acquire_file_lock(metadata_file_lock_file);

      // read in metadata file to update
      auto metadata = ((file_exists(metadata_file)) ? read_metadata_file(metadata_file) : MetadataHandler::initial_metadata());
      const auto stored = convert_to_str_ref_set(metadata);

      // check to see if src location is already registered
      const auto it = stored.find(get_str_rep(DIL,DIF,MOD_TY));
      if (it != stored.end()) { // if so release metadata file lock and return the relevant source id srcID
        release_file_lock(LFD);
        // llvm::errs() << "\n[scabbard.instr.meta:DBG] reusing metadata entry!\n[scabbard.instr.meta:DBG]    repr: `" << repr(metadata[it->second]) << "`\n\n"; //DEBUG
        return {it->second};
      }
      
      // add new SrcMetadata to metadata object
      const uint64_t srcID = stored.size();
      auto dbg = metadata.insert(std::make_pair(srcID, (SrcMetadata){srcID, get_full_file_path(DIF), DIL.getLine(), DIL.getCol(), MOD_TY}));
      // llvm::errs() << "\n[scabbard.instr.meta:DBG] added a new metadata entry!\n[scabbard.instr.meta:DBG]    repr: `" << repr(dbg.first->second) << "`\n\n"; //DEBUG

      // update the metadata file
      write_metadata_file(metadata_file, metadata);

      //release metadata file lock
      release_file_lock(LFD);

      return {srcID};
    }


    
    LocResult MetadataHandler::_trace_scope(llvm::Function& F, const llvm::DebugLoc& DIL, const llvm::DIScope& DIS, const ModuleType MOD_TY) const
    {
      return _trace_file(F, DIL, *DIS.getFile(), MOD_TY);
    }


    // template<class DINode_t>
    // llvm::GlobalVariable* MetadataHandler::_trace(llvm::Module& M, const DINode_t* DI)
    // {
    //   return nullptr;
    // }


    LocResult MetadataHandler::trace(llvm::Function& F, const llvm::DebugLoc& DI, const ModuleType MOD_TY) const
    {
      if (not DI)
        llvm::errs() << "\n[scabbard.instr.meta:ERR] The Debug location data does not exist! Try compiling with debug data! [in `" << F.getName() << "`]\n";
      assert(DI && "[scabbard.instr.meta:ERR] The Debug location data does not exist! Try compiling with debug data!");
      return _trace_scope(F, DI, *llvm::dyn_cast_or_null<llvm::DIScope>(DI.getScope()), MOD_TY);
    }



    // << ------------------------------------------------------------------------------------------ >> 

    inline bool file_exists(const std::string& name) {
      struct stat buffer;   
      return (stat(name.c_str(), &buffer) == 0); 
    }

    inline MetadataJSONFile_t MetadataHandler::initial_metadata()
    {
      return (MetadataJSONFile_t){
        {default_srcID,(SrcMetadata){default_srcID, "<UNKNOWN_SRC_FILE>", 0ul, 0ul, ModuleType::UNKNOWN_MODULE}},
        {libtrace_srcID,(SrcMetadata){libtrace_srcID, "<SCABBARD_TRACE>", 0ul, 0ul, ModuleType::HOST}},
        {hipAPI_srcID,(SrcMetadata){hipAPI_srcID, "<HIP_API>", 0ul, 0ul, ModuleType::HOST}}
      };
    }
    
    const char* str_open_errno_msg(int __errno) 
    {
      switch (__errno) {
        case EACCES:  return "Permission denied.";
        case ENFILE:  return "Too many open files in system.";
        case EMFILE:  return "Too many open files.";
        case EINTR:   return "Interrupted function call.";
        case ENOTDIR: return "Not a directory.";
        case EEXIST:  return "File exists.";
        case EFAULT:  return "Bad address.";
        case EINVAL:  return "Invalid argument.";
        case ENAMETOOLONG:  return "Filename too long.";
        case ENOMEM:  return "Not enough space.";
        case EISDIR:  return "Is a directory.";
        case ENOENT:  return "No such file or directory.";
        case ENOSPC:  return "No space left on device.";
        case EROFS:   return "Read-only file system.";

        default:      return "<UNKNOWN_ERROR>";
      }
    }

    const char* str_flock_errno_msg(int __errno) 
    {
      switch (__errno) {
        case EBADF:   return "fd is not an open file descriptor.";
        case EINTR:   return "While waiting to acquire a lock, the call was interrupted by delivery of a signal caught by a handler; see signal(7).";
        case EINVAL:  return "operation is invalid.";
        case ENOLCK:  return "The kernel ran out of memory for allocating lock records.";
        case EWOULDBLOCK: return "The file is locked and the LOCK_NB flag was selected.";

        default:      return "<UNKNOWN_ERROR>";
      }
    }


    inline int acquire_file_lock(const std::string& filepath) 
    {
      llvm::errs() << "\n[scabbard.instr.metadataHandler.acquireLock:DBG] START ACQUIRING file system lock for metadata file. "; //DEBUG
      const size_t ATTEMPT_LIMIT = 20ul;
      int lfd = -1;
      size_t tries = 0ul;
      // open the lock file
      while (-1 == (lfd = open(filepath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 00766))) {
        tries++;
        if (tries > ATTEMPT_LIMIT) {
          llvm::errs() << "\n[scabbard.instr.meta:ERR] failed to open teh metadata lock file " << tries << "x times!\n"
                          "\n[scabbard.instr.meta:ERR]        last error: `" << strerror(errno) << "`: \"" << str_open_errno_msg(errno) << "\""
                          "\n[scabbard.instr.meta:ERR]    recommendation: IF issue persists AND you are building multithreaded (`-j` flag == multithreaded);"
                          "\n[scabbard.instr.meta:ERR]                      try switch to single threaded build.\n";
          assert(tries <= ATTEMPT_LIMIT && "[scabbard.instr.meta:ERR] too many attempts to open the file lock.");
        }
        switch (errno) {
          case EACCES:  // "Permission denied.\n";
          case ENFILE:  // "Too many open files in system.\n";
          case EMFILE:  // "Too many open files.\n";
          case EINTR:   // "Interrupted function call.\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10u)); // wait and try again
            break;
          default:
            llvm::errs() << "\n[scabbard.instr.meta:ERR] Error occurred opening the metadata lock file."
                            "\n[scabbard.instr.meta:ERR]             error: `" << strerror(errno) << "`: \"" << str_flock_errno_msg(errno) << "\""
                            "\n[scabbard.instr.meta:ERR]    recommendation: IF issue persists AND you are building multithreaded (`-j` flag == multithreaded);"
                            "\n[scabbard.instr.meta:ERR]                      try switch to single threaded build.\n";
            assert(false && "[scabbard.instr.meta:ERR] unrecoverable error occurred while acquiring metadata file lock file.");
            break;
        }
      }
      tries = 0ul;
      // initiate a POSIX lock on the lock file
      while (-1 == flock(lfd, LOCK_EX)) {
        if (tries > ATTEMPT_LIMIT) {
          llvm::errs() << "\n[scabbard.instr.meta:ERR] failed to acquire lock for metadata file " << tries << "x times!\n"
                          "\n[scabbard.instr.meta:ERR]        last error: `" << strerror(errno) << "`: \"" << str_flock_errno_msg(errno) << "\""
                          "\n[scabbard.instr.meta:ERR]    recommendation: IF issue persists AND you are building multithreaded (`-j` flag == multithreaded);"
                          "\n[scabbard.instr.meta:ERR]                      try switch to single threaded build.\n";
          assert(tries <= ATTEMPT_LIMIT && "[scabbard.instr.meta:ERR] too many attempts to acquire metadata file lock.");
        }
        switch (errno) {
          case EWOULDBLOCK: // The file is locked and the LOCK_NB flag was selected.
          case EINTR:   // While waiting to acquire a lock, the call was interrupted by delivery of a signal caught by a handler; see signal(7).
            std::this_thread::sleep_for(std::chrono::milliseconds(10u)); // wait and try again
            break;
          case EINVAL:  // operation is invalid.
          case ENOLCK:  // The kernel ran out of memory for allocating lock records.
          case EBADF:   // fd is not an open file descriptor.
          default:
            close(lfd); // don't care about close exceptions rn (might be a mistake)
            llvm::errs() << "\n[scabbard.instr.meta:ERR] Error occurred while acquiring the lock for the metadata file's lock file."
                            "\n[scabbard.instr.meta:ERR]             error: `" << strerror(errno) << "`: \"" << str_open_errno_msg(errno) << "\""
                            "\n[scabbard.instr.meta:ERR]    recommendation: IF issue persists AND you are building multithreaded (`-j` flag == multithreaded);"
                            "\n[scabbard.instr.meta:ERR]                      try switch to single threaded build.\n";
            assert(false && "[scabbard.instr.meta:ERR] unrecoverable error occurred while acquiring the lock for the metadata file's lock file.");
            break;
        }
      }
      llvm::errs() << "\n[scabbard.instr.metadataHandler.acquireLock:DBG] END file system lock for metadata file ACQUIRED. "; //DEBUG
      return lfd;
    }

    inline void release_file_lock(int LFD)
    {
      // the flock release
      flock(LFD, LOCK_UN);
      llvm::errs() << "\n[scabbard.instr.metadataHandler.acquireLock:DBG] RELEASED file system lock for metadata file. "; //DEBUG
      // close the lock file
      close(LFD);
    }


    // << ------------------------------------------------------------------------------------------ >> 

    inline std::string get_full_file_path(const llvm::DIFile& DIF)
    {
      std::stringstream out;
      out << DIF.getDirectory().str() << '/' << DIF.getFilename().str();
      return out.str();
    }

    inline std::string get_str_rep(const llvm::DebugLoc& DIL, const llvm::DIFile& DIF, const ModuleType MOD_TY)
    {
      std::stringstream out;
      out << '[' << MOD_TY << "] \"" << DIF.getDirectory().str() << '/' << DIF.getFilename().str() << ':' << DIL.getLine() << ',' << DIL.getCol() << "\"";
      return out.str();
    }


    inline std::unordered_map<std::string,uint64_t> convert_to_str_ref_set(const MetadataJSONFile_t& original)
    {
      std::unordered_map<std::string,uint64_t> result;
      for (const auto i : original)
        result.insert(std::make_pair(::scabbard::to_string(i.second), i.second.srcID));
      return result;
    }

  } //?namespace instr
} //?namespace scabbard
