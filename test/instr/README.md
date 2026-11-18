# HIP unified-memory instrumentation test suite

Four self-contained, run-once HIP apps for validating an LLVM pass plugin's instrumentation.
All data is generated internally; all shared data is `hipMallocManaged`; no `hipMemcpy`, no
`hipDeviceSynchronize` (only stream sync, events for stream ordering, and `hipLaunchHostFunc`).

| app | flavour | highlights |
|---|---|---|
| transformer | AI | templated strided `Mat<T>` views, GEMM with epilogue functors, UM + hipMalloc scratch mix (the "sometimes UM" lines), agent-scope atomics, per-head streams + events |
| nbody | science | `Body<T>`/`Vec3<T>` AoS in UM, LDS tiling, softening policy template, float + double instantiations, system-scope atomics |
| graph | science/data | host-built CSR in UM, LDS-vs-UM shared `bin_add` helper, fixed-point PageRank atomics, atomicMin label propagation, host-polled UM flag |
| reaction_diffusion | science | z-slab decomposition, halo exchange via UM only, stencil policy template, event-ordered streams |

## Build / run
```
cd appN_*/ && cmake -S . -B build -DCMAKE_HIP_ARCHITECTURES=gfx90a   # or omit for "native"
cmake --build build
./build/appN_* [num_worker_threads]      # workers map to device (w % ndev), so 1 GPU still runs N threads
```
* `-DPER_THREAD_DEFAULT_STREAM=ON` builds with the per-thread default stream (default OFF = legacy).
* The pass plugin is applied via `include(scabbard)` (needs `SCABBARD_PATH` in the environment) and `scabbard_instrument_all()` at the end of each CMakeLists.txt.
* `ctest` runs each app with 1 and 4 workers. Exit code 0 = self-checks passed.
* On GPUs that support it, `export HSA_XNACK=1` for page-fault driven managed memory.

## Annotation legend (also at the top of every source file)
`// UM: R=<n> W=<n> A=<n>` = static count of unified-memory reads / writes / atomic-RMWs on that line
(host or device code). Counts are per leaf scalar (a `Vec3<float>` copy = R=3). `(sometimes: ...)`
marks lines that only sometimes touch UM because of code reuse. Untagged lines have no UM access.
