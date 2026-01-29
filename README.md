  Scabbard
============
_By: Andrew Osterhout [@osterhoutan-UofU](github.com/osterhoutan-UofU) [@osterhoutan](github.com/osterhoutan) , Ignacio Laguna [@ilagunap](github.com/laguna) , Ganesh Gopalakrishnan [@ganeshutah](github.com/ganeshutah) &nbsp;_

A GPU data race detector based upon the methods used in SWORD for AMD HIP (aka a HIP mounted sheath for a SWORD)

Scabbard is designed to detect races in the heterogenous memory system the exists between a AMD gpu and your CPU in projects utilising ROCm's hip technology.
(OpenCL, scycl, and CUDA are not supported env when running on AMD GPUs).

Scabbard cannot tell you if a data race occurred inside your GPU code, only if the CPU read from the GPU before the GPU finished it's work.


### Compatibility
- scabbard officially only supports AMD GPUs that are compatible with ROCm (no intel, nvidia, arm, or apple GPUs supported).
- scabbard officially only supports AMD's ROCm + hip build system
  - you might have some success with openmp's offload target builds that target ROCm compatible AMD GPUs
  - you might also have some success with opencl code that is built with ROCm's hipcc
- scabbard does not support thin-lto in builds
- scabbard requires the use of the gpu-rdc tool chains so if your project requires the `-fno-gpu-rdc` flag scabbard will not work for you.



## Table of Contents
- [scabbard](#scabbard)
    - [Compatibility](#compatibility)
  - [Table of Contents](#table-of-contents)
  - [Build:](#build)
    - [Dependencies](#dependencies)
  - [Using scabbard:](#using-scabbard)
    - [Step 1: Instrument your build](#step-1-instrument-your-build)
      - [Instrumenting with CMake](#instrumenting-with-cmake)
      - [Manually Adding Instrumentation to your Build System](#manually-adding-instrumentation-to-your-build-system)
      - [Instrumenting Simple Programs](#instrumenting-simple-programs)
    - [Step 2: Generate a Trace File](#step-2-generate-a-trace-file)
    - [Step 3: Check for Unified Memory Data Races](#step-3-check-for-unified-memory-data-races)
      - [Interpreting the output:](#interpreting-the-output)
  - [Getting Involved](#getting-involved)
  - [Contributing](#contributing)
  - [LLNL Software](#llnl-software)


## Build:

### Dependencies
- ROCm >= v5.4.3 (>=6.1.2 prefered)
- llvm (the llvm-dev build included with the ROCm version you use)
- python >= 3.10
- cmake >= 3.20
- zlib >= 1.2

First set your `ROCM_PATH` environment variable to point to your desired install of ROCm.
If you only have one installed version or the `/opt/rocm` sym-link exists you might be able to skip this step. 
```sh
mkdir build && cd build
cmake ../
make instr
```

## Using scabbard:

Follow these 3 steps:
  - [Step 1:](#step-1-instrument-your-build) Instrument your build
    - [Instrumenting Simple Programs](#instrumenting-simple-programs)
    - [Instrumenting with CMake](#instrumenting-with-cmake)
    - [Manually Adding Instrumentation to your Build System](#manually-adding-instrumentation-to-your-build-system)
  - [Step 2:](#step-2-generate-a-trace-file) Generate a Trace File
  - [Step 3:](#step-3-check-for-unified-memory-data-races) Check for Unified Memory Data Races

> #### For Best results
>  1. Define the `SCABBARD_PATH` environment variable for your situation
>     | situation:          | `SCABBARD_PATH` value:                   |
>     |:-------------------:|:----------------------------------------:|
>     | local build only    | `<path-to-scabbard-repo>/build/scabbard` |
>     | installed _(alpha)_ | `$ROCM_PATH/scababrd`                    |
>     ```sh
>     export SCABBARD_PATH=<scabbard-path-value-from-table-above>
>     ```
>  2. Add an alias for the scabbard interface/wrapper. _i.e._
>     ```sh
>     alias scabbard=$SCABBARD_PATH/scabbard.py
>     ```
>     **NOTE:**
>     using `scabbard --help` will provide basic instructions on how to use the scabbard interface

<br/>

### Step 1: Instrument your build

Use one of the following methods to configure and build your project with scabbards instrumentation:
- Option 1: [Instrumenting Simple Programs](#instrumenting-simple-programs)
- Option 2: [Instrumenting with CMake](#instrumenting-with-cmake)
- Option 3: [Manually Adding Instrumentation to your Build System](#manually-adding-instrumentation-to-your-build-system)

#### Instrumenting with CMake

 1. Build scabbard as [directed above](#build).
 2. Set the `SCABBARD_PATH` environment variable as [directed above](#for-best-results).
   ```sh
   export SCABBARD_PATH=<scabbard-path-value-from-table-above>
   ```
 3. Include the scabbard CMake module by adding _**ONE**_ the following options to your top level `CMakeLists.txt` somewhere after the `project()` call but before you define any targets.
   ```cmake
   include($ENV{SCABBARD_PATH}/scabbard.cmake)
   ```
 4. Use one of the following CMake Helper Functions
    - To instrument your entire project add the following to the end of your top level `CMakeLists.txt` file (recommended, if you don't know what your doing and have a straight forward build structure)
      ```cmake
      scabbard_instrument_all() # instrument all targets (skips custom targets)
      ```
    - To instrument a single target (can be used multiple times, and you will still need to instrument all relevant targets used by the final executable)
      ```cmake
      scabbard_instrument_target(<target>) # instruments a single target (ignores custom targets)
      ```
    - To instrument only specific targets with one call add the following to the end of your top level `CMakeLists.txt` file.
      ```cmake
      scabbard_instrument_targets(<target1> <target2> ...) # instrument all specified targets (ignores custom targets)
      ```
 5. Use CMake to configure your project normally
    ```sh
    # example
    mkdir build && cd build
    cmake ../
    ```
    - ***NOTE:*** you can add a `-DENABLE_SCABBARD=<On/Off>` to enable or disable scabbard instrumentation at config time without having to go back to update your `CMakeLists.txt` files.
 6. Use the scabbard interface tool in `build` mode to start your build and define the options scabbard needs at compile time to instrument your code.  (assumes you set the alias as described in [For Best Results](#for-best-results) section above)
    ```sh
    # example only (replace the meta-file path with whatever works for you)
    scabbard build --meta-file=../<project-name>.scabbard.meta <build-cmd>
    ```
 7. Let your build compile as normal (done with this step, continue to [step 2](#step-2-generate-a-trace-file)).
   


#### Manually Adding Instrumentation to your Build System

If you are using a different build system and have multiple build steps follow these instructions
 1. Build scabbard as [directed above](#build).
 2. Set the `SCABBARD_PATH` environment variable as [directed above](#for-best-results).
    ```sh
    export SCABBARD_PATH=<scabbard-path-value-from-table-above>
    ```
 3. Set the `SCABBARD_METADATA_FILE` environment variable.  
    This is where scabbard will export the metadata that will provide meaningful output for source location and the like for the output of [step 3](#step-3-check-for-unified-memory-data-races).
    So set it to save the file somewhere easily accessible to you that you will remember.
    - For unix makefile, add the following anywhere in your makefile
      ```make
      # path provided is an example template set it to whatever you need.
      export SCABBARD_METADATA_FILE=./<proj-name>.scabbard.meta
      ```
    - For manually invoking each build command, set it as an environment variable in your shell before you begin building. 
      ```sh
      # path provided is an example template set it to whatever you need.
      export SCABBARD_METADATA_FILE=./<proj-name>.scabbard.meta
      ```
    - Other build systems, the option above should work for you as well, but you can use whatever options they provide for defining environment variables that should be visible to the commands they issue.
 4. Add the following clang/hipcc/mpcc flags to your compile steps (_i.e._ `CXX_FLAGS` in make):
    ```
    -g -fgpu-rdc -flto
    ```
    And ensure you don't need the `-fno-lto` or `-fthin-lto` flags as scabbard is not compatible. 
 5. Add the following flags clang/hipcc/mpcc to your link step
    ```
    -flto -fgpu-rdc -Wl,--load-pass-plugin=${SCABBARD_PATH}/libinstr.so -Xoffload-linker --load-pass-plugin=${SCABBARD_PATH}/libinstr.so -L${SCABBARD_PATH} -ltrace -ltrace.device -lpthread -lz
    ```
    If you are manually calling ldd to link, you should know what to do to modify these for your needs.
    > You may also need to add a `-L` option pointing to the appropriate version of zlib on your system.
 6. Let your build compile as normal (done with this step, continue to [step 2](#step-2-generate-a-trace-file)).




#### Instrumenting Simple Programs

> **NOTE:**
> using `scabbard instr --help` will provide basic instructions on how to use the scabbard interface

When building simple programs (i.e. ones with a single `.cpp` file) use can use the scabbard interface tool to build and instrument your project auto-magically.

To do this just use the `scabbard instr` tool to launch whatever your build/compile command as follows:
 1. Build scabbard as [directed above](#build).
 2. Set the `SCABBARD_PATH` environment variable as [directed above](#for-best-results).
    ```sh
    export SCABBARD_PATH=<scabbard-path-value-from-table-above>
    ```
 3. Set the Alias for the scabbard interface tool as [directed above](#for-best-results)
 4. Use the scabbard interface tool in `instr` mode to instrument your build command by providing your build command where it says `<build-cmd>` (and set the meta-file path to whatever pleases you.)
  ```sh
  # meta-file path provided is an example template set it to whatever you works for you.
  scabbard instr --meta-file=./<proj-name>.scabbard.meta <build-cmd>
  ```
  It is recommended to provide a `--meta-file` to scabbard so that you know where scabbard ends up 
  generating the metadata file that the verify step will use to inform you where in your source code
  the data races are linked to.
  If you don't provide this it will create a file called `anon.scabbard.meta` in your current working directory instead.
 5. Let your build compile as normal (done with this step, continue to [step 2](#step-2-generate-a-trace-file)).




### Step 2: Generate a Trace File

Use the scabbard interface tool in `trace` mode to configure the launch your instrumented program.

```sh
# trace-file path provided is an example template set it to whatever you works for you.
scabbard trace --trace-file=<proj-name>.scabbard.trace <run-cmd>
```

> **NOTE:**
> using `scabbard trace --help` will provide basic instructions on how to use the scabbard interface

It is recommended to provide a `--trace-file` so you know where the trace file will be generated at.
If you don't it will default to `<run-cmd>.scabbard.trace` wherever that is on your machine.

The `<run-cmd>` place holder can be replaced with whatever command and its arguments you would normally use to launch your program.

> **WARNING:**
> Scabbard trace files can get very large (>15GB for simple GPU code).
> It is recommended to try to run your scabbard instrumented code on as small of a data-set as possible.
> To reduce the number of write operations that will occur on the GPU as much as possible.
> Else be prepared for a very large trace file, and an even longer wait time for the offline analysis.

After your program finishes running ensure it exited normally then continue to [step 3](#step-3-check-for-unified-memory-data-races).



### Step 3: Check for Unified Memory Data Races

Now to get a list of data races and places in your code where data races didn't occur (this time) 
but could occur.
You must use the scabbard interface tool in `verif` mode using the metadata file generated in [step 1](#step-1-instrument-your-build), and the trace file generated in [step 2](#step-2-generate-a-trace-file) to generate a report on what data-races occurred and places in your code that might have a unified memory data-race in the future due to lacking proper sync events or other code patterns that can result in undefined behavior.

```sh
scabbard verif <meta-file\> <trace-file\>
```

> **NOTE:**
> The offline-analysis/verify tool is robust, but single threaded, a little slow, 
> and currently does not have a spinner or any kind if output until it finishes 
> to let you know it's not frozen or stuck.
> However we can assure you that it is very robust and is very very unlikely to be frozen or stuck.
> So just let it run until it is done.
>
> A rule of thumb for how long it will take is is roughly 15sec per 1GB of trace file
> _(this can very greatly between different CPUs)_ 

#### Interpreting the output:
Scabbard produces one of 4 kinds of results:
 - `<NONE>`: best result to get, nothing was found
 - `Warning: Unmatched Read`: there was a read to a unifiedm memory location that was never written to (possily uninitilized)
 - `Warning: Possible-Race/Missing-Sync`: There was a read after a write but no sync event in between, so order cannot be confirmed\*.
 - `Error: Data Race`: There was a read before a write and no sync event, order is bad data race is known to have occured.

> _\* - If you did **not** use stream host callbacks in your code to process data form the GPU then this warning is the same as the `Error Data Race` result, but if you did use stream callbacks then scabbard cannot tell what cpu reads occur in stream callbacks (with managed order) and which do not. Therefore we report all write read orderings that occur without a sync event in between as a warning._

Scabbard reduces the output by categorising all reported events that happen between the same read and write points in the source code as the same race and will report how many time a race occured between these two points but only provide the specific runtime information about the first instance to occur at that point in the output feed.



 Getting Involved
-----------------------------------------------
If you have questions or concerns please reach out via email to:
 - [andrew.osterhout@utah.edu](mailto://andrew.osterhout@utah.edu)
 - [ganesh@cs.utah.edu](mailto://ganesh@cs.utah.edu)

 Contributing
------------------------------------------------
This project is inteneded to be modiefied until it in sutable for upstream into the LLVM project.
Therefore we are limmiting direct contributions to those we can handle via direct communication.
After this project is upstreamed into the LLVM Project all contributions should be handled in the ways required by that project.

 LLNL Software
------------------------------------------------
This software was funded by and created for Lawrence Livermore National Laboratory (LLNL) in collaboration with the Kalhart School of Computing at Utah State University (UofU).

