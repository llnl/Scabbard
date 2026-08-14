  Scabbard Dev Notes
====================================================================================================

Thought: change data race algorithm to be good time bad time for each kind of HR,DR,HW,DW event.
Could this work?
how does intra host/device threads complicate thing?

Reason: pure host side or pure device side data fighting can hide the existence of a H<->D data race
especially if I were to use one mem data-structure.
- this might not be valid though -- brain to foggy must come back to this thought after a little break.

Design: start (valid: HW,HR) -> kernel launch (v: DR,DW) -> sync (v: HW,HR) -> kl again
Problems:
 - no support for kernel launches (no different from current design)
 - during start can't tell if events happen on memory of real concern or not (as no DR/DW in mem to confirm)
   - likely not an issue as all HR/HW are valid before a kernel launch
 - will need to confirm that where logical clocks sync is valid across all threads and devices
   - might need to lock all host threads on launch instead of just unifying with a callback
 - possibly more sensitive to the trace getting out of order insertions during live runs
 - the use of callbacks will always result in a false positive
   - would need to introduce support for callbacks to avoid this issue.

What I need:
  - Interval map containing the last DR/DW to occur on a piece of memory
    - could just be streamID, 
      - might reduce the richness of our results if no workaround could be found
        - could greatly simplify memory usage by not storing `TraceData` objects and always releasing them after they are processed in the state machine.
    - would be better to still be the smart pointer to DR/DW `TraceData` objects
  - a map of stream_id to state of valid host or device control of memory
  - change state in launch and sync zones
  - a check of the current state in read and wrote zones
  - add wrapper around callback registry to establish callback control of memory on a thread per for a stream
    - change the way we store `HostThreadID` to be a integer rather than stdlib struct



 TODO:
----------------------------------------------------------------------------------------------------

-[ ] get time data from applications:
     - Quicksilver
       - NI-Build: 77.61s/21.644s (user/total)
       - I-Build: 108.89s/55.609s (user/total)
       - NI-Run: 7.83s (user)
       - I-Run: 11.4s (user)
     - XSBench
       - NI-Build: 7.41s/1.709s (user/total)
       - I-Build: 8.36s/2.59s (user/total)
       - NI-Run: 0.306s/2.88s (prog/user)
       - I-Run: 8.841s/85.38s (prog/user)
       - trace size 15G

-[X] write pseudocode for logical clock branching
  - option 1
    ```rust
    use mut atomic<u64> as HostClock;
    use mut atomic<u64> as DeviceClock;
    let HOST_CLOCK: HostClock = 0;  // instrumented global
    let HOST_MUTEX: std::mutex;     // instrumented global

    /// [instrumented user fn] 
    /// Some device kernel that reads from `in` and writes to `out`
    ///  and when the write occurs each thread will tick `DC: DeviceClock` up by one
    ///    (no locks or mutexes, just atomics and memory coherency)
    //   instrumented to accept the `DC: DeviceClock` argument as it's last argument
    __global__ fn kernel(in: u64[], out: mut u64[], DC: DeviceClock) -> void { /*...*/ }

    /// [instrumented user fn]
    /// This is the user's function that is instrumented 
    __host__ fn someFn() {
      let S: StreamID = HANDLE_HIP!(hipCreateStream(0));
      let in, out: mut u64* = nullptr;
      HANDLE_HIP!(hipMalloc(&in, DATA_SIZE));
      HANDLE_HIP!(hipMalloc(&out, DATA_SIZE));
      setInputData(in);
      zeroOutput(out);
      let DC: DeviceClock* = scabbard.branchDeviceClock(S); // instrumented
      kernel<<<G_DIM,B_DIM,S,SHARE_MEM_SIZE>>>(in, out, DC); // instrumented to pass DC in w/ orig arguments
      HANDLE_HIP!(hipRegisterCallback(S, scabbard.jobEndClockJoinCallback, {DC})); // instrumented

      // Sync (or don't if you want scabbard to register it)
      
      for (i: u64; i < DATA_SIZE; i++) {
        readData!(out[i]);
        scabbard.host.trace_append((void*)&out[i],        // instrumented
                                    ActionData::READ|ActionData::DeviceHeap,    // instrumented
                                    /* id representing the line where readData occurs */12); // instrumented
      }
    }

    /// [scabbard rtl fn] 
    /// Allocate a job specific device clock and
    /// bBranch the device clock from the host clock
    __host__ fn scabbard.branchDeviceClock(S: StreamID) -> DeviceClock* {
      let DC: DeviceClock* = nullptr;
      HANDLE_HIP(hipMallocManaged(&DC, sizeof(DeviceClock), hipManagedPreferDevice));
      *DC = HOST_CLOCK;
      return DC;
    }
    /// [scabbard rtl fn] 
    /// fn called when kernel finishes unites the clocks
    __host__ fn scabbard.jobEndClockJoinCallback(DEVICE_CLOCK: DeviceClock*) -> void {
      if (DEVICE_CLOCK > HOST_CLOCK)
        HOST_CLOCK = DEVICE_CLOCK;
    }
    /// [scabbard rtl fn]
    /// append any device reads to the trace file
    __host__ fn scabbard.host.trace_append(PTR: void*, data: ActionData, metadata: SrcLocID) -> void {
      HOST_MUTEX.lock();
      addToTraceQueue(TraceData!(HOST_CLOCK++, PTR, data, metadata));
      HOST_MUTEX.unlock();
    }
    ```
  - option 2
    ```
    
    ```


-[ ] fix invalid record issues in quicksilver
      - files issues are occurring in
        - CollisionEvent-hip-amdgcn-amd-amdhsa-gfx90a.o
        - CycleTracking-hip-amdgcn-amd-amdhsa-gfx90a.o
        - MCT-hip-amdgcn-amd-amdhsa-gfx90a.o
        - MC_Facet_Crossing_Event-hip-amdgcn-amd-amdhsa-gfx90a.o
        - MC_Load_Particle-hip-amdgcn-amd-amdhsa-gfx90a.o
        - MC_Segment_Outcome-hip-amdgcn-amd-amdhsa-gfx90a.o
        - MacroscopicCrossSection-hip-amdgcn-amd-amdhsa-gfx90a.o
        - NuclearData-hip-amdgcn-amd-amdhsa-gfx90a.o
      - issues derive from
        - `BitcodeReader::parseFunctionBody` -> `BitcodeReader::materialize(GlobalValue)` -> `IRLinker::linkFunctionBody()` while handling instrumented kernel function(s)
          - linked to CollisionEvent.cc in `"_Z14CollisionEventP10MonteCarloR11MC_Particlej"`
          - linked to CycleTracking.cc in `"_Z17CycleTrackingGutsP10MonteCarloiP13ParticleVaultS2_"`
      - output from modified lld build
          
          ```
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z14CollisionEventP10MonteCarloR11MC_Particlej`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z17CycleTrackingGutsP10MonteCarloiP13ParticleVaultS2_`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z17MCT_Nearest_FacetP11MC_ParticleR11MC_LocationR9MC_VectorPK15DirectionCosineddbP10MonteCarlo`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z23MC_Facet_Crossing_EventR11MC_ParticleP10MonteCarloiP13ParticleVault`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z16MC_Load_ParticleP10MonteCarloR11MC_ParticleP13ParticleVaulti`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z18MC_Segment_OutcomeP10MonteCarloR11MC_ParticleRj`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_Z31weightedMacroscopicCrossSectionP10MonteCarloiiii`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          [dbg] offending call inst calls `scabbard.trace.device.trace_append$mem` from inside of `_ZN19NuclearDataReaction15sampleCollisionEddPdS0_RiPmi`
          lld: error: Invalid record [inst-call-2] (Producer: 'LLVM17.0.0git' Reader: 'LLVM 17.0.0git')
          ```


-[ ] look into following projects to adapt them as tests for testing scabbard
     - smallpt-parallel-bvh-gpu \[[repo](https://github.com/andrewwuan/smallpt-parallel-bvh-gpu/tree/master)\] by An Wu \[[github-profile](https://github.com/andrewwuan)\]
     - dpid \[[repo](https://github.com/mergian/dpid)\] by Nicolas Weber \[[github-profile](https://github.com/mergian/)\]

-[ ] look into effects of only capturing first X GPU write operations and last X gpu write Operations

-[X] look into why the `SimplifyCFGPass` is removing the function bodies in the kernel

-[X] create the trace file analyzer / data race detector
  -[ ] design a range based data structure that lets you apply and remove multiple traits from ranges of integers (mem addresses)

-[ ] fix the issue of host modules not knowing when they have a gpu module to register the metadata variables for

-[ ] verify that we don't need the conditional trace append for the cpu, then remove it
  - this might take extra special care to do, as we need to ensure that we record all loads from gpu memory

-[ ] ...

-[X] add support for instrumenting `hipMemcpy`
-[X] add support for instrumenting `hipMalloc`
-[X] add support for instrumenting `hipFree`

-[X] use `AMD_LOG_LEVEL` to higher value to get more info
  ```
  $ ./test/test.instr.out
  :3:rocdevice.cpp            :426 : 1368798841724 us: 596395: [tid:0x7f7fa8f883c0] Initializing HSA stack.
  :1:rocdevice.cpp            :434 : 1368798841785 us: 596395: [tid:0x7f7fa8f883c0] hsa_init failed.
  :1:runtime.cpp              :78  : 1368798841790 us: 596395: [tid:0x7f7fa8f883c0] Runtime initialization failed
  :3:hip_memory.cpp           :511 : 1368798841799 us: 596395: [tid:0x7f7fa8f883c0] hipMalloc: Returned hipErrorInvalidDevice : 

  [scabbard::trace::init::ERROR] could not allocate space for the device side async queue! [hipError_t: 101]

  :3:hip_device_runtime.cpp   :541 : 1368798841829 us: 596395: [tid:0x7f7fa8f883c0]  hipGetDeviceCount ( 0x7fff7c41dc7c ) 
  :3:rocdevice.cpp            :426 : 1368798841879 us: 596395: [tid:0x7f7fa8f883c0] Initializing HSA stack.
  :1:rocdevice.cpp            :434 : 1368798841888 us: 596395: [tid:0x7f7fa8f883c0] hsa_init failed.
  :1:runtime.cpp              :78  : 1368798841891 us: 596395: [tid:0x7f7fa8f883c0] Runtime initialization failed
  :3:hip_device_runtime.cpp   :543 : 1368798841895 us: 596395: [tid:0x7f7fa8f883c0] hipGetDeviceCount: Returned hipErrorNoDevice : 

  [scabbard::trace::init::ERROR] could not get the number of compatible devices! [hipError_t: 100]
  ```
  - tracked the `rocdevice.cpp` to [this repo](https://github.com/ROCm-Developer-Tools/clr/blob/5914ac3c6e9b3848023a7fa25e19e560b1c38541/rocclr/device/rocm/rocdevice.cpp#L448)
    - The header called in is from [this repo](https://github.com/HSAFoundation/HSA-Runtime-AMD/blob/0579a4f41cc21a76eff8f1050833ef1602290fcc/include/hsa.h#L309),
      but this is just so there can be a common header for all AMD hardware when using ROCm
    - it calls a function called `hsa_init` which is not defined in that repo and links to a device and system determined 
      implementation of the HSA protocol.
    - I still don't know what one is for my machine but I was able to find one implementation in [this repo](https://github.com/RadeonOpenCompute/ROCR-Runtime/blob/e0fadddb3175cb95ce9e9af2ebd2a205045e854e/src/core/runtime/hsa.cpp#L204) 
      that I hope will be helpful (it is the primary runtime implementation)
      - this implementation just calls a function in the primary runtime singleton to acquire

- Discovery 

```ll
tail call void @__hipRegisterVar(
  i8** %6,                                                                 ; fatCubinHandle
  i8* bitcast (i64* @__d_val__ to i8*),                                 ; hostVar
  i8* getelementptr inbounds ([10 x i8], [10 x i8]* @2, i64 0, i64 0),  ; deviceAddress 
  i8* getelementptr inbounds ([10 x i8], [10 x i8]* @2, i64 0, i64 0),  ; deviceName
  i32 0,                                                                ; ext
  i64 8,                                                                ; size
  i32 0,                                                                ; constant
  i32 0                                                                 ; global
)
```


 Design Decisions:
----------------------------------------------------------------------------------------------------

- Decided to use a logical vector clock (where all cpu threads share one clock and a lock for said clock),
   and each GPU job has one clock, using callbacks teh GPU clock is reunited with the CPU clock after
   it completes a run
  - ALTERNATIVE:
    - Use wall clock time at highest resolution and try to compensate for the drift 
       and offset of these clocks between the CPU and GPU
  - SIDE EFFECTS:
    - Trace report will often report that a CPU event occurs before a GPU event because for every 
       event on teh CPU to tick it's clock up 1 there are at least 32 more ticks on the CPU clock.
      This can result in a CPU event altering the State machine before registering a GPU event in the
       Verification State machine.
      - EXAMPLE(S):
        - A user does not perform a sync before freeing the GPU memory they used, 
           but it does happen to occur in wall time after the write on the GPU.
          This results in the state machine loosing its memory of previous reads on said memory 
           address before the state machine gets to the write that would trigger the violation.



 Questions (& Answers)
----------------------------------------------------------------------------------------------------

### **Q1:** How does the IR denote host memory on the device? 
  (also when it does it do it if inconsistent)


#### **A:** It depends on what the classification of the memory is. 
  [_See unified memory attributes section below:_](#unified-memory-attributes)


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q2:** How does the IR denote device memory on the host? 
  (also when it does it do it if inconsistent)


#### **A:** It depends on what the classification of the memory is. 
      [ _See unified memory attributes section below:_](#unified-memory-attributes)


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q3:** How to identify where memory is stored for device function parameters?


#### **A:** two ways one runtime and one static 
   1. There exists a hip/cuda function called [`hipPointerGetAttributes()`](https://rocm-developer-tools.github.io/HIP/group__Memory.html#ga7c3e8663feebb7be9fd3a1e5139bcefc) that can retrieve a [`hipPointerAttribute_t`](https://rocm-developer-tools.github.io/HIP/structhipPointerAttribute__t.html) which contains the following information:
      ```cpp
        // summarized from hip_runtime_api.h 
        struct hipPointerAttribute {
          enum hipMemoryType {hipMemoryTypeHost,hipMemoryTypeDevice} memoryType; // enum for what pointer will be valid
          int device;           // id of the device
          void* devicePointer;  // pointer to mem location on device
          void* hostPointer;    // pointer to mem location on host
          int isManaged;        // if the mem is managed by linux's HMM system
          unsigned int allocationFlags;  // general allocation flags
        };
      ```
      This is runtime only, and costly in runtime to branch on this every time, but possibly even more costly in storage space for trace (and compression cycles) to output with every trace event to avoid branching.
      (also might not be callable from the GPU) <br/><br/>

   2. To figure out where memory probably is during compile-time (_aka_ during our instrumentation pass) except for some 
      - in host kernel you can identify global device & managed mem by looking in the `@__hip_module_ctor` function


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q4:** How to instrument external modules and libs into an llvm IR module?


#### **A:** 
  Looked into how Address Sanitizer works and they utilizes the builtin llvm 
  [`compiler-rt`](https://github.com/llvm/llvm-project/tree/main/compiler-rt)
  which we can't do until we are ready and sure this tool is worth upstream-ing.

  The Breakdown of how `compiler-rt` supposedly works, 
  is that it is a single library that gets compiled down to all supported architectures
  (presumably as a static link library; _aka_ a library compiled to native code that is folded into 
  the executable at compile time and not linked to dynamically for runtime linking).
  Then during linking it is auto-magically included as a library to link to.

  I am unsure if manually telling the compiler to link to our library is exactly replicating this process.
  There are several reasons why:
  - it is hard to tell from their cmake file if the compile to a static library
  - when I compile our lib clang's C/C++ options mangle the names with the namespace 
    and rest of the function signature 
    (I have found a way to counter act this 
    \[[stack-overflow](https://stackoverflow.com/questions/524633/how-can-i-avoid-name-mangling)\])
  - 


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q5:** How to deal with the use of external device side library calls?


#### **A:** ...


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q6:** How to deal with runtime memory property accesses?
  (through use of [`hipHostMalloc()`](https://docs.amd.com/bundle/HIP_API_5/page/group___memory.html#gaad40bc7d97ccc799403ef5a9a8c246e1), [`hipHostRegister()`](https://docs.amd.com/bundle/HIP_API_5/page/group___memory.html#gab8258f051e1a1f7385f794a15300e674),  [`hipHostUnregister()`](https://docs.amd.com/bundle/HIP_API_5/page/group___memory.html#ga4c9e1810b9f5858d36c4d28c91c86924), _etc_)


#### **A:** ...


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q7:** Will mem addresses be the same on host and the various devices?


#### **A:** Depends _(see conditions below)_ 

  Dependent on class of memory and the prescribed address space 
  \[[doc](https://llvm.org/docs/AMDGPUUsage.html#address-spaces)\].

  - **Managed Mem:** yes (though the global will contain a pointer to the managed mem which will be the same on both host and devices, but that global's address will be different on host and devices)
  - **Registered Host Mem:** System dependent \[[doc](https://docs.amd.com/bundle/HIP_API_5/page/group___memory.html#gab8258f051e1a1f7385f794a15300e674)\]
  - **Device Mem:** Yes always
  - **Device Shared Mem:** Yes always


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 


### **Q8:** How to deal with async operations 
  (_aka_ [`hipMemcpyAsync()`](https://docs.amd.com/bundle/HIP_API_5/page/group___memory.html#d189e8922))


#### **A:** ...
  - NOTE: when called with blocking `hipLaunchKernel()` it does not call the device stub,
    however it will call the device stub if it is called from a work stream's queue.
    - This means I might be able to handle memory allocation and free-ing from
      instrumented predecessor and post-cessor functions on the host,
      and more importantly do it per kernel call.
    - This still requires getting wither the device id or stream id on the kernel to interface
      with heterogeneous memory 


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q9:** How to deal with multiple concurrent streams to avoid data-races in trace buffers?
  There is no builtin way in hip (or cuda) to from the device at runtime get any notion of what stream,
  or other facility controls you, or even what device in a multi-device infrastructure you are running on.

#### **A:** several _**possible**_ options:
  NOTE: all options below have some design feature that prevents data-races from occurring, but they might have other slowdowns.

  WARNING: if you have concurrent streams that are running or using the same kernel code, there will be no way to differentiate which kernel launch they came from (so that is still a problem).

   1. \
      Setup a single global trace buffer in global memory, to some scale of .
      By making the next slot variable atomic we can prevent concurrent threads from getting the same offset when writing into the memory space.
        - PRO: easiest to implement
        - PRO: small (relatively) constant order of memory usage (probs $O(1)$)
        - PRO: Fixed sized buffer means it can be contiguous and therefore easy single copying to host reducing memory sync slowdowns.
        - CON: might cause **major** slowdowns in multiGPU environments
        - CON: during branch fragmentation, the excess memory slowdown and lane overlap will cause major slowdowns. (branch fragmentation is when a kernel branches to the degree where members of a warp are not adjacent threads anymore)
   2. \
      Same as option 1 but at kernel launch expand and contract the number of lanes in the device buffer,
      by a proportion of the active streams, say $warpSize\cdot(N_{streams}+1)$ buffer lanes, of constant size addressed by the modulo of the block and thread id's then let the thread/block id's offset everything.
      - PRO: small (relatively) order $O(n)$ memory usage (where $n$ is the number of instantiated streams)
      - PRO: easy to implement (compared to the others)
      - CON: being able to dynamically change the buffer means that it will need to be fragmented, slowing down and fragmenting copy operations and introducing complexity that could result in unknown behavior.
      - CON: if users aren't destroying unused streams memory could ballon, 
        copying the buffers could take up more time locking down memory,
        and 
   3. \
      Similar to options 1 & 2, 
      but expand and contract the device queues based upon kernel launches and the requisite sizes of the blocks and grids.
        - PRO: Possible to implement 
        - PRO: No need to encode thread or block id's
        - CON: more memory usage when doing larger launches
   4. _¿posible?_\
    Find a way to make all threads in a warp write to adjacent shared memory, then write the address of the shared memory to a common place.
      - CON: no way to identify an offset inside a warp, especially when branching gets messy
   5. \
    Instrument every function in every device module not marked `available_externally` to accept a new first argument, and make that argument be the address to launch specific device side implementation of the trace buffer.
   6. ...


  <br/><br/>
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 



### **Q10:** how to deal with host/device clock mismatches?
I have to use the high resolution clock for both devices, but they don't technically have to give me
time in the same units let alone be synced at their resolution to any standard epoch.

#### **A:** ...



 Unified Memory Attributes
----------------------------------------------------------------------------------------------------

### Global Managed Memory
| Scope  | Attribute     | ¿Atomic? | Loc    | ¿Host? | ¿Device? |
|:------:|:-------------:|:--------:|:------:|:------:|:--------:|
| global | `__managed__` | X        | ¿both? | ✔      | ✔        |

#### C/C++/HIP:
```cpp
__managed__ std::size_t __m_val__ = 0xFACE;
```

#### LLVM IR: 
```llvm
;   amd-hsa/kernel/device IR module
; ===================================
@__m_val__.managed = protected addrspace(1) externally_initialized global i64 64206, align 8  ; <- HMM (managed) memory location
;         ^^^^^^^^   ^^^^^^^^^              ^^^^^^^^^^^^^^^^^^^^^^
@__m_val__ = protected addrspace(1) externally_initialized global i64 addrspace(1)* null      ; <- device side ptr to HMM memory 
;            ^^^^^^^^^              ^^^^^^^^^^^^^^^^^^^^^^            ^^^^^^^^^^^^
; ---- load operation in kernel function ----
  %1 = load i64 addrspace(1)*, i64 addrspace(1)* addrspace(1)* @__m_val__, align 8  ; <- get location of managed memory
  %2 = load i64, i64* %1, align 8, !tbaa !3                                         ; <- get val from managed mem
; ---- store operation in kernel function ----
  store i64 %5, i64 addrspace(1)* %1, align 8, !tbaa !4                             ; <- store val in managed mem
; ---- atomic operation in kernel function ----
  %5 = atomicrmw xor i64 addrspace(1)* %4, i64 %3 seq_cst, align 8, !dbg !1703

;   x86_64/host IR module
; =========================
@__m_val__.managed = internal global i64 64206, align 8, !dbg !7  ; <- HMM (managed) memory location
;         ^^^^^^^^   ^^^^^^^^
@__m_val__ = internal externally_initialized global i64* null     ; <- host side pointer to HMM memory
;         ^? ^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^
; ---- load operation in host function ----
  %1 = load i64 addrspace(1)*, i64 addrspace(1)* addrspace(1)* @__m_val__, align 8  ; <- get location of managed memory
  %2 = load i64, i64 addrspace(1)* %1, align 8, !tbaa !4                            ; <- get val from managed mem
; ---- store operations in host function ----
  store i64 %5, i64* %1, align 8, !tbaa !3                                          ; <- store val in managed memory
```
As seen above managed memory when part of a global definition is marked with the `.managed` name attribute.

However, functions never directly read from this location, rather there exists another variable that
contains a pointer to wherever the managed memory location is at any given time.
Functions first load this memory to get an up to date pointer then perform operations from those mem
locations.

<br/><br/>



### Global Managed Atomic Memory
| Scope  | Attribute     | ¿Atomic? | Loc    | ¿Host? | ¿Device? |
|:------:|:-------------:|:--------:|:------:|:------:|:--------:|
| global | `__managed__` | ✔        | ¿both? | ✔      | ✔        |

#### C/C++/HIP:
```c
__managed__ _Atomic(size_t) __m_atomic__(0xDAB);
```

#### LLVM IR: 
```llvm
;   amd-hsa/kernel/device IR module
; ===================================
@__m_atomic__.managed = protected addrspace(1) externally_initialized global i64 3499, align 8  ; <- HMM (managed) memory location
;            ^^^^^^^^   ^^^^^^^^^
@__m_atomic__ = protected addrspace(1) externally_initialized global i64 addrspace(1)* null     ; <- device side ptr to HMM (managed) memory
;               ^^^^^^^^^
; ---- load operation in kernel function ----
  %10 = load i64 addrspace(1)*, i64 addrspace(1)* addrspace(1)* @__m_atomic__, align 8  ; <- get memory location of managed memory
  %11 = load atomic i64, i64 addrspace(1)* %10 seq_cst, align 8, !tbaa !8               ; <- atomic read of managed memory location
  %14 = load i64 addrspace(1)*, i64 addrspace(1)* addrspace(1)* @__m_atomic__, align 8  ; <- get managed memory loc, for atomic rw-op
; ---- atomic operation in kernel function ----
  %15 = atomicrmw xor i64 addrspace(1)* %14, i64 %13 seq_cst, align 8                   ; <- atomic rw-op on managed memory

;   x86_64/host IR module
; =========================
@__m_atomic__.managed = internal global i64 3499, align 8         ; <- HMM (managed) memory location
;            ^^^^^^^^   ^^^^^^^^
@__m_atomic__ = internal externally_initialized global i64* null  ; <- host side ptr to HMM (managed) memory
;               ^^^^^^^^
; ---- load operation in host function ----
  %18 = load i64*, i64** @__m_atomic__, align 8               ; <- get memory location of managed memory
  %19 = load atomic i64, i64* %18 seq_cst, align 8, !tbaa !7  ; <- atomic read of managed memory location
  %22 = load i64*, i64** @__m_atomic__, align 8               ; <- get managed memory loc, for atomic rw-op
; ---- atomic operation in host function ----
  %23 = atomicrmw xor i64* %22, i64 %21 seq_cst, align 8      ; <- atomic rw-op on managed memory
```
As seen above managed memory when part of a global definition is marked with the `.managed` name attribute.

However, functions never directly read from this location, rather there exists another variable that
contains a pointer to wherever the managed memory location is at any given time.
Functions first load this memory to get an up to date pointer then perform operations from those mem
locations.

<br/><br/>



### Global Device Memory
| Scope  | Attribute     | ¿Atomic? | Loc    | ¿Host? | ¿Device? |
|:------:|:-------------:|:--------:|:------:|:------:|:--------:|
| global | `__device__`  | X        | device | ✔      | ✔        |

#### C/C++/HIP:
```cpp
__device__ std::size_t __d_val__ = 0xBA5E;
```

#### LLVM IR: 
```llvm
;   amd-hsa/kernel/device IR module
; ===================================
@__d_val__ = protected addrspace(1) externally_initialized global i64 47710, align 8  ; <- device side storage location
; ---- load op in kernel function ----
  %1 = load i64 addrspace(1)*, i64 addrspace(1)* addrspace(1)* @__m_val__, align 8
; ---- store op in kernel function ----
  store i64 %9, i64 addrspace(1)* @__d_val__, align 8, !tbaa !4


;   x86_64/host IR module
; =========================
@__d_val__ = internal global i64 undef, align 8 ; <- device memory global (no indicator that it is any different from host side globals)
; ---- load op in host function ----
  %1 = load i64*, i64** @__m_val__, align 8
; ---- store op in host function ----
  store i64 %13, i64* @__d_val__, align 8, !tbaa !3
```

This situation is straight forward, for both device and host.

This is interesting because there is absolutely no indicator that this is not a host side variable,
***implying that the memory address space is shared with AMD gpu's***

<br/><br/>



### Global Atomic Device Memory
| Scope  | Attribute     | ¿Atomic? | Loc    | ¿Host? | ¿Device? |
|:------:|:-------------:|:--------:|:------:|:------:|:--------:|
| global | `__device__`  | ✔        | device | ✔      | ✔        |

#### C/C++/HIP:
```cpp
__device__ _Atomic(size_t) __d_atomic__(0x5AD);
```

#### LLVM IR: 
```llvm
;   amd-hsa/kernel/device IR module
; ===================================
@__d_atomic__ = protected addrspace(1) externally_initialized global i64 1453, align 8  ; <- device side storage location
; ---- load op in kernel function ----
  %16 = load atomic i64, i64 addrspace(1)* @__d_atomic__ seq_cst, align 8, !tbaa !8
; ---- atomic rw-op in kernel function ----
  %19 = atomicrmw xor i64 addrspace(1)* @__d_atomic__, i64 %18 seq_cst, align 8


;   x86_64/host IR module
; =========================
@__d_atomic__ = internal global i64 undef, align 8 ; <- device memory global (no indicator that it is any different from host side globals)
; ---- load op in host function ----
  %28 = load atomic i64, i64* @__d_atomic__ seq_cst, align 8, !tbaa !7
; ---- atomic rw-op in host function ----
  %31 = atomicrmw xor i64* @__d_atomic__, i64 %30 seq_cst, align 8
```

This situation is straight forward, for both device and host.

This is interesting because there is absolutely no indicator that this is not a host side variable,
***implying that the memory address space is shared with AMD gpu's***
