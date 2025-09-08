  LLNL README for Scabbard
=============================

> **NOTE:** These instructions are for LLNL affiliates, to be used on Tioga,
> and may not apply to general users of Scabbard.
<!-- > For General Configuration and Usage info please see [`scabbard/README.md`](https://github.com/osterhoutan-UofU/scabbard).
> >_**NOTE:** at time of writing Scabbard is not yet published, so the above link might not work until the code is approved for release._ -->


 Use Prebuilt Scabbard
-----------------------------
> _**NOTE:** the Scabbard provided in Tioga's LMod is ONLY compatible with ROCm 6.1.2_
> _If you absolutely need to use a different Scabbard, please contact the scabbard creator(s), or wait until it's full release to build your own Scabbard for a specific ROCm version._
<!-- > _Scabbard will eventually be available at https://github.com/osterhoutan-UofU/scabbard_ -->

 1. Load the relevant LMod modules:
    - ROCm == 6.1.2
    - Python >= 3.10
    - CMake >= 3.20
 2. Load Scabbard from LMod:
    ```sh
    module load scabbard/0.1
    ```
 3. Check to see that Scabbard is loaded by running the following commands:
    - Check that `$SCABBARD_PATH` environment variable is defined:
      ```sh
      echo $SCABBARD_PATH
      ```
      So long as this command returns a path you should be good to go.
    - Check that ROCm 6.1.2 is the only ROCm loaded (and that the magic ROCm compiler `rocmcc` modules are not loaded):
      ```sh
      if module is-loaded rocm/6.1.2 && ! module is loaded rocmcc; then echo "Correct ROCm config"; else echo "WARN: bad ROCm config (only supports rocm/6.1.2)"; fi
      ```
      This should return:
      ```
      Correct ROCm config
      ```
 4. Follow the instructions in the _[CMake Configuration](#cmake-configuration)_ section to instrument and run your instrumented code.



 CMake Configuration
-----------------------------

> The following instructions are for your project not Scabbard.

### Load the Scabbard CMake Module
Add the following code to the top of your Primary `CMakeLists.txt` File:
```cmake
list(APPEND CMAKE_MODULE_PATH "$ENV{SCABBARD_PATH}")
include(scabbard)
```
Then add the following to the end of the same file:
```cmake
scabbard_instrument_all()
```

### Set C++ and HIP Compilers
Due to issues with file suffix ambiguities both the C++ compiler and HIP compiler in your CMake config should be set 
to the same ROCm compiler.
You can do this however you see fit but we recommend just adding the following line to your top level `CMakeLists.txt` file
someplace after the ROCm/HIP package import/find line in said top level `CMakeLists.txt` file:
```cmake
set(CMAKE_CXX_COMPILER "${ROCM_DIR}/bin/hipcc")
```


 Usage Instructions
------------------------------

### Running your instrumented code
Scabbard uses the contents of the `$SCABBARD_TRACE_FILE` environment variable to know where to save the trace file to.
So all you need to do is set that variable in one of the following ways--where `<launch-cmd>` is whatever command you would use to luanch and run your code, and `<trace-file>` is the filepath of where you want to save the trace file:
- Use the Scabbard command line utility:
  ```sh
  scabbard trace --trace-file=<trace-file> <launch-cmd>
  ```
  This command line utility just sets the environment variable for you before running the command given in `<lanch-cmd>`.
- Export the environment variable:
  ```sh
  export SCABBARD_TRACE_FILE=<trace-file>
  <launch-cmd>
  ```
  This will set the environment variable for the length of the session, so all subsequent runs of the `<launch-cmd>` will **overwrite** the same trace file.

- Temporarily set the environment variable:
  ```sh
  SCABBARD_TRACE_FILE=<trace-file> <launch-cmd>
  ```
  This will set the environment variable just for this specific launch.
> _**NOTE:** if you do not set `SCABBARD_TRACE_FILE` it will use the name of the executable with a `.scabbard.trace` suffix._


### Verifying the Trace File
After generating the trace file run the Scabbard verify tool.

To do this you will need to find the metadata file generated durring instrumentation.
> _If you used CMake to build your project the metadata file should be in the top level of the build directory, with a `.scabbard.meta` file extension._

Then just run the verify tool and read the output:
```sh
scabbard verif <metadata-file> <trace-file>
```

### Interpreting the Results
Scabbard groups all like data races to only report each one once, but keeps a talley of how many it has seen.
> A data race is _"like"_ another race if they share the same read and write locations in the source code.

Scabbard reports on the following occurrences:
- ERROR/`DATA RACE`: This is a true and verified data race, where the CPU read before the GPU could write to the same location.
- WARNING/`POSSIBLE DATA RACE`: This is a place in your code where the read and write events happened in the correct order, 
  but something else could be wrong.
  It is usually one of the two scenarios, and Scabbard can tell the difference between the two, but not if there is no confounding issues.
   1. The CPU read from a shared memory location that the GPU has never written to.
      - This could mean multiple things neither is in the scope of Scabbard
        - The CPU incorrectly read from uninitialized unified memory
        - The memory location was initialized by the CPU which Scabbard would not know about
   2. There was no recognizable synchronization event between the GPU's write event and the CPU's read event.
      - This could mean that a data race could occur in the future but didn't this time.
      - If you are using custom barrier or synchronizations systems Scabbard will report most operations as a WARNING.
- SUCCESS/`NO RACE`: This means that there wasn't even a warning, and your code is clean.

In most cases Scabbard will report some number of WARNINGs even if your code has no races,
 due to our inability to differentiate between the two scenarios listed above.
It is recommended that you still investigate the WARNINGs, to ensure that there is no chance of a race in the future.



 Simple Code Example:
--------------------------------
A simple single file example of how to use Scabbard with CMake on Tioga:

Assume you have a project with the following files:
 1. A source code file `SimpleExample.cpp`:
    ```cpp
    #include <hip/hip_runtime.h>

    #include <string>
    #include <iostream>

    #define DIM (32ul)

    __global__
    auto matrix_mul(double* A, double* B, double* C) -> void
    {
      const size_t ROW = blockIdx.y*blockDim.y+threadIdx.y;
      const size_t COL = blockIdx.x*blockDim.x+threadIdx.x;
      double tmp_sum = 0.0L;
      for (size_t i=0; i<DIM; ++i)
        tmp_sum += A[ROW*DIM + i] * B[i*DIM + COL];
      C[ROW*DIM + COL] = tmp_sum;    // <<<<<<<<<<<<<<<<<<<<<   GPU WRITE LOCATION
    }

    __host__
    auto HIP_CHECK(hipError_t hipRes, const std::string& errMsg) -> void
    {
      if (hipRes == hipSuccess) return;
      std::cerr << "\n[hip ERR: " << hipRes << "] " << errMsg << std::endl;
      exit(EXIT_FAILURE);
    }

    __host__
    auto main() -> int 
    {
      double* A, * B, * C;
      double out[DIM*DIM];

      HIP_CHECK(hipMalloc(&A, sizeof(double)*DIM*DIM), "from `hipMalloc(&A, ...)`");
      HIP_CHECK(hipMalloc(&B, sizeof(double)*DIM*DIM), "from `hipMalloc(&B, ...)`");
      HIP_CHECK(hipMalloc(&C, sizeof(double)*DIM*DIM), "from `hipMalloc(&C, ...)`");

      matrix_mul<<<(dim3){1u,1u,1u},(dim3){DIM,DIM,1u},0ul,0ul>>>(A,B,C);

      HIP_CHECK(hipDeviceSyncronize();, "from `hipDeviceSyncronize()`");  // <<< Comment me out to create data race <<<
      
      double res_sum = 0.0L;
      for (int64_t i=(DIM*DIM)-1l; i>=0l; --i) // iterating backwards should ensure that we read something before a write.
        res_sum += C[i];  // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  CPU READ LOCATION

      HIP_CHECK(hipStreamSynchronize(0ul), "from `hipStreamSynchronize(0ul)`");

      HIP_CHECK(hipFree(A), "from `hipFree(A)`");
      HIP_CHECK(hipFree(B), "from `hipFree(B)`");
      HIP_CHECK(hipFree(C), "from `hipFree(C)`");

      std::cout << res_sum <<std::endl;
      
      return 0;
    }
    ```
 2. A Cmake File `CMakeLists.txt`:
    ```cmake
    cmake_minimum_required(VERSION 3.10)
    project(HIPExample LANGUAGES CXX HIP)

    
    find_package(HIP REQUIRED)

    # Set HIP compiler flags (optional)
    set(CMAKE_HIP_FLAGS "${CMAKE_HIP_FLAGS} -std=c++14")
    set(CMAKE_CXX_FLAGS "${CMAKE_HIP_FLAGS} -std=c++14")

    # Add executable
    add_executable(HIPExample HIPExample.cpp)

    # Link against HIP libraries
    target_link_libraries(HIPExample PRIVATE hip::device)

    # set language for target to be HIP
    set_property(TARGET HIPExample PROPERTY LANGUAGE HIP)
    ```

### Step 0: Prep Work
Make sure your project builds and runs on it's own without Scabbard,
but with the environment you will need to run Scabbard later.

This means you need to ensure you have a ROCm 6.1.2 loaded as your only ROCm version.
```sh
module unload rocm rocmcc
module load rocm/6.1.2
```
To be sure also run one of the following command(s) to confirm the version:
  - The `which` shell command (might point to global bin, so use the second option if it does not work out)
    ```sh
    which hipcc
    ```
    You should get the following result:
    ```
    hipcc=/opt/rocm-6.1.2/bin/hipcc
    ```
  - `hipcc`'s `--version` command
    ```sh
    hipcc --version
    ```
    You should get the following result:
    ```
    HIP version: 6.1.40093-bd86f1708
    AMD clang version 17.0.0 (https://github.com/RadeonOpenCompute/llvm-project roc-6.1.2 24193 669db884972e769450470020c06a6f132a8a065b)
    Target: x86_64-unknown-linux-gnu
    Thread model: posix
    InstalledDir: /opt/rocm-6.1.2/llvm/bin
    Configuration file: /opt/rocm-6.1.2/lib/llvm/bin/clang++.cfg
    ```

Now build your project like you would normally, for our example that means:
```sh
mkdir build
cd build
cmake -B. -S.. -G"Unix Makefiles" -DCMAKE_CXX_COMPILER=hipcc
make
flux run -N1 -n1 -c8 -g1 ./HIP_example
```

### Step 1: Load Scabbard module
```sh
module load scabbard
```
If you get any messages about ROCm versions you have not properly loaded the correct ROCm module.

Test to make sure that the `$SCABBARD_PATH` variable is defined
```sh
echo $SCABBARD_PATH
```
So long as this command returns a path you should be good to go.

Test that the Scabbard Command line utility is available to you
```sh
which scabbard
scabbard --help
```


### Step 2: Add Scabbard to your CMake Configuration
Add the following line near the top of your top level `CMakeLists.txt`
to load Scabbard's CMake module into your CMake environment.
```cmake
include(scabbard)
```
Then insert the following to the end of the file after all target definitions
to have Scabbard modify the build configuration of all necessary targets.
```cmake
scabbard_instrument_all()
```

For our example the resulting `CMakeLists.txt` should look like this:
```cmake
cmake_minimum_required(VERSION 3.10)
project(HIPExample LANGUAGES CXX HIP)

include(scabbard)                                       # <<<

find_package(HIP REQUIRED)

# Set HIP compiler flags (optional)
set(CMAKE_HIP_FLAGS "${CMAKE_HIP_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_HIP_FLAGS} -std=c++14")

# Add executable
add_executable(HIPExample HIPExample.cpp)

# Link against HIP libraries
target_link_libraries(HIPExample PRIVATE hip::device)

# set language for target to be HIP
set_property(TARGET HIPExample PROPERTY LANGUAGE HIP)

scabbard_instrument_all()                             # <<< 
```

### Step 4: Reconfigure and Build your Project
> _**NOTE:** we recommend working from a clean build, so try to remove the old build directory and start again._

Use CMake to configure and build your project as normal.


For Our Example it will look like the following:
```sh
mkdir build
cd build
cmake -B. -S.. -G"Unix Makefiles" -DCMAKE_CXX_COMPILER=hipcc
```
The output from cmake should contain some number of the following lines with messages from Scabbard:
```
[scabbard:DBG] SCABBARD_PATH: '<whatever-your-scabbard-path-is>'
[scabbard:NOTE] instrumenting 'EXECUTABLE': '<exe-target>'
[scabbard:NOTE] instrumenting 'DYNAMIC_LIBRARY': '<dyn-lib-target>'
[scabbard:NOTE] enabling GPU-RDC and LTO for 'STATIC_LIBRARY' lib: '<st-lib-target>'
[scabbard:NOTE] enabling GPU-RDC and LTO for 'OBJECT_LIBRARY' lib: '<obj-lib-target>'
```
These will tell you what targets have had their configs modified by Scabbard.

Then use whatever build system to build your project.

For our example just run `make` using Scabbard's CLI utility to set the save location of the metadata file generated during instrumentation.
```sh
scabbard build --meta-file=./HIP_example.scabbard.meta make
```
> If you build without the Scabbard CLI to set the metadata save file it will automatically save to `./anon.scabbard.meta` in your current working directory.

Somewhere in the output of the make file you should see some messages from Scabbard.
These will let you know that Scabbard is instrumenting your code.
```
[scabbard.instr.device.run:DBG] running instrumentation pass on device/GPU module (LTO)
[scabbard.instr.host.run:DBG] running instrumentation pass on host/CPU module (LTO)
```


### Step 5: Run Your Instrumented Code:

You will want to set where your trace file is saved to _(see "[Running Your Instrumented Code](#running-your-instrumented-code)" section for the various options)_

For our example we will use the Scabbard CLI tool with Flux to run our program
```sh
flux run -N1 -n1 -c8 -g1 scabbard trace --trace-file=HIP_example.scabbard.trace ./HIP_example
```

During the run you should see messages that look like the following:
```
[scabbard.trace:DBG] reading 0/0 data points from GPU s:0x14f j:0
[scabbard.trace:DBG] reading 1024/1024 data points from GPU s:0x14f j:0
```
> _**NOTE:** If any message has a fraction that is not improper (value greater than or equal to one) or `0/0`, then scabbard was unable to get all the trace data from the GPU.  You will want to build your own version of Scabbard with a bigger buffer to match your project._

A trace file should be produced, you will need this for the next step.


### Step 6: Use Scabbard Verify to Check for Data Races

> _**NOTE:** Scabbard verify can use a lot of memory and CPU cycles, we recommend using flux to run it off of your CLI Node._

To check for data races with Scabbard you will need the metadata file generated during the instrumented build step,
and the trace file from the last step.

Just provide Scabbard Verify with the two files in the specified order and let the program run for a good long while.
```sh
scabbard verif <metadata-file> <trace-file>
```

For our example the command should look like the following:
```sh
scabbard verif HIP_example.scabbard.meta HIP_example.scabbard.trace
```
The example will report no race unless you commented out the line indicated in the source file.

See the [_"Interpreting the Results"_](#interpreting-the-results) section for information on interpreting the results.



