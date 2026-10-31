/**
 * @file test_sm_stress.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief more Claud generated unit tests, this time ones meant to stress the state machine with many
 *        different sized memory allocations with more threads and streams and more disorderly accesses.
 * @version alpha 0.0.1
 * @date 2026-10-21
 * 
 * @copyright Copyright (c) 2026
 * 
 */

/**
 * @file test_sm_stress.cpp
 * @brief Heavier, GPU-program-shaped stress tests for the StateMachine:
 *        large allocations, one element per device thread, broadcast reads,
 *        stencils, reductions, double-buffered streaming, multiple host threads,
 *        and a mix of the default stream with explicitly created streams.
 *
 * Design rules these tests follow (learned from the SM's model):
 *  - The SM only keeps the most recent event per address, so only the FIRST instance of a
 *    problem is reported. Racy tests assert `>= 1` of a class of status, never exact counts.
 *  - A possible race (DW -> HR) can surface as UNPROTECTED_HR (later reads, or reads from a host
 *    thread that never touched the stream), so unsynced-read checks accept either.
 *  - Clean workloads assert that NOTHING at all was reported (status_count_all() == 0).
 *  - Device events use a branched logical clock per stream (merged into the host clock by
 *    device_job_completes_event), so true-race tests are built with explicit event counts so the
 *    required ordering holds in logical time (see comments on those tests).
 *  - Metadata == source location: one metadata per event site, generated before the loop.
 *  - Streams: `hipStream_t` from spawn_device_stream() for explicit streams; omit it (nullptr)
 *    in launch_event()/sync_event() for the default stream.
 */
#include "sm_fixture.hpp"

#include <vector>
#include <cstdint>
#include <limits>
#include <utility>

// ---------------------------------------------------------------------------
// DEFAULT STREAM MODE
// Chosen by a compiler flag, so it is static at runtime. Tests whose outcome depends on the mode
// are wrapped in these macros. ADJUST THIS BLOCK if your build signals per-thread default
// streams with a different flag. Un-wrapped tests behave the same in both modes.
// ---------------------------------------------------------------------------
#if defined(HIP_API_PER_THREAD_DEFAULT_STREAM) || defined(__HIP_API_PER_THREAD_DEFAULT_STREAM__)
#  define SM_TEST_DEFAULT_STREAM_PER_THREAD
#else
#  define SM_TEST_DEFAULT_STREAM_LEGACY
#endif

namespace scabbard {
namespace rtl {
namespace test {

using Status = StateMachine::Result::Status;

class SM_Stress : public SM_Tests {
protected:
  static constexpr std::size_t ELEM = 4u;         // sizeof(float)
  static constexpr std::size_t BLOCK_DIM = 256u;  // device threads per block
  static constexpr std::size_t NO_SKIP = std::numeric_limits<std::size_t>::max();
  static constexpr std::size_t DT = 4u;           // host threads in the decomposition tests
  static constexpr std::size_t DS = 4096u;        // elements per host thread slice

  struct StencilSites { const SrcMetadata *left, *center, *right, *write; };
  struct Decomp {
    std::vector<HostThreadId> th;
    std::vector<hipStream_t> st;                  // nullptr entries == default stream
    std::uintptr_t in, out;
  };

  // ---- addressing -------------------------------------------------------
  std::uintptr_t malloc_array(HostThreadId tid, std::size_t n_elems, const SrcMetadata* M=nullptr) {
    return malloc_event(tid, n_elems*ELEM, M);
  }
  static std::uintptr_t at(std::uintptr_t base, std::size_t i) { return base + i*ELEM; }

  // ---- device side ------------------------------------------------------
  DeviceThreadId gpu_thread(jobId_t jid, std::size_t t) const {
    return DeviceThreadId(jid,
                          dim3{static_cast<unsigned>(t % BLOCK_DIM), 0, 0},
                          dim3{static_cast<unsigned>(t / BLOCK_DIM), 0, 0});
  }
  /// run `body(t, dt)` once for every device thread t in [first,last)
  template<typename Body>
  void grid(jobId_t jid, std::size_t first, std::size_t last, Body&& body) {
    for (std::size_t t=first; t<last; ++t) {
      DeviceThreadId dt = gpu_thread(jid, t);
      body(t, dt);
    }
  }
  void dread(jobId_t jid, std::uintptr_t base, std::size_t i, const SrcMetadata* M, DeviceThreadId& dt) {
    device_read_event(jid, at(base,i), ELEM, M, &dt);
  }
  void dwrite(jobId_t jid, std::uintptr_t base, std::size_t i, const SrcMetadata* M, DeviceThreadId& dt) {
    device_write_event(jid, at(base,i), ELEM, M, &dt);
  }

  // ---- host side --------------------------------------------------------
  void hread(HostThreadId tid, std::uintptr_t base, std::size_t i, const SrcMetadata* M) {
    host_read_event(tid, at(base,i), ELEM, M);
  }
  void hwrite(HostThreadId tid, std::uintptr_t base, std::size_t i, const SrcMetadata* M) {
    host_write_event(tid, at(base,i), ELEM, M);
  }
  void host_fill(HostThreadId tid, std::uintptr_t base, std::size_t first, std::size_t count, const SrcMetadata* M) {
    for (std::size_t i=first; i<first+count; ++i) hwrite(tid, base, i, M);
  }
  void host_scan(HostThreadId tid, std::uintptr_t base, std::size_t first, std::size_t count, const SrcMetadata* M) {
    for (std::size_t i=first; i<first+count; ++i) hread(tid, base, i, M);
  }

  // ---- result helpers ---------------------------------------------------
  /// first unsynced host read of device output shows up as one of these two
  std::size_t unsynced_read_reports() {
    return status_count(Status::POS_RACE_HR_DW) + status_count(Status::UNPROTECTED_HR);
  }

  // ---- reusable workloads ----------------------------------------------
  StencilSites make_stencil_sites() {
    StencilSites s;
    s.left = generate_metadata(); s.center = generate_metadata();
    s.right = generate_metadata(); s.write = generate_metadata();
    return s;
  }
  /// 3-point stencil: thread t reads src[t-1], src[t], src[t+1] and writes dst[t]
  void stencil_step(jobId_t jid, std::uintptr_t src, std::uintptr_t dst, std::size_t n, const StencilSites& s) {
    grid(jid, 0, n, [&](std::size_t t, DeviceThreadId& dt) {
      if (t > 0)     dread(jid, src, t-1, s.left, dt);
      dread(jid, src, t, s.center, dt);
      if (t + 1 < n) dread(jid, src, t+1, s.right, dt);
      dwrite(jid, dst, t, s.write, dt);
    });
  }
  /// `steps` stencil kernels ping-ponging between two buffers on one stream
  /// (the default stream if `use_default_stream`, otherwise an explicit one).
  void stencil_run(std::size_t n, std::size_t steps, bool host_sync_each_step, bool use_default_stream) {
    auto tid = spawn_host_thread();
    hipStream_t stream = (use_default_stream) ? nullptr : spawn_device_stream();
    auto bufA = malloc_array(tid, n);
    auto bufB = malloc_array(tid, n);
    auto s_init = generate_metadata();
    auto s_launch = generate_metadata();
    auto s_sync = generate_metadata();
    auto s_check = generate_metadata();
    auto s_final = generate_metadata();
    auto sites = make_stencil_sites();

    host_fill(tid, bufA, 0, n, s_init);   // B is produced by the first kernel
    auto src = bufA, dst = bufB;
    for (std::size_t k=0; k<steps; ++k) {
      auto jid = launch_event(tid, stream, s_launch);
      stencil_step(jid, src, dst, n, sites);
      device_job_completes_event(jid);
      if (host_sync_each_step) {
        sync_event(tid, stream, s_sync);
        hread(tid, dst, 0, s_check);      // host-side convergence check
      }
      std::swap(src, dst);
    }
    if (not host_sync_each_step) sync_event(tid, stream);
    host_scan(tid, src, 0, n, s_final);   // `src` now holds the last output
    free_event(tid, bufA);
    free_event(tid, bufB);
  }

  /// chunked double-buffered streaming over two slots, one stream per slot.
  /// If `slot0_default_stream`, slot 0 runs on the default stream and slot 1 on an explicit one.
  /// If `skip_sync_at` is a chunk index, the host forgets the sync before reading that chunk's slot.
  void streaming_pipeline(std::size_t chunks, std::size_t chunk_elems, std::size_t skip_sync_at,
                          bool slot0_default_stream) {
    auto tid = spawn_host_thread();
    hipStream_t st[2] = { (slot0_default_stream) ? nullptr : spawn_device_stream(),
                          spawn_device_stream() };
    std::uintptr_t in[2], out[2];
    for (int b=0; b<2; ++b) { in[b] = malloc_array(tid, chunk_elems); out[b] = malloc_array(tid, chunk_elems); }
    auto s_fill = generate_metadata();
    auto s_launch = generate_metadata();
    auto s_sync = generate_metadata();
    auto s_k_in = generate_metadata();
    auto s_k_out = generate_metadata();
    auto s_readback = generate_metadata();

    for (std::size_t k=0; k<chunks; ++k) {
      std::size_t b = k % 2u;
      if (k >= 2u) {                                  // slot reused: collect result of chunk k-2
        if (k != skip_sync_at) sync_event(tid, st[b], s_sync);
        host_scan(tid, out[b], 0, chunk_elems, s_readback);
      }
      host_fill(tid, in[b], 0, chunk_elems, s_fill);  // "H2D copy" of the next chunk
      auto jid = launch_event(tid, st[b], s_launch);
      grid(jid, 0, chunk_elems, [&](std::size_t t, DeviceThreadId& dt) {
        dread(jid, in[b], t, s_k_in, dt);
        dwrite(jid, out[b], t, s_k_out, dt);
      });
      device_job_completes_event(jid);
    }
    for (std::size_t b=0; b<2u; ++b) {                // drain
      sync_event(tid, st[b], s_sync);
      host_scan(tid, out[b], 0, chunk_elems, s_readback);
    }
  }

  /// DT host threads, each owning a slice of one big buffer and its own stream (explicit, or the
  /// default stream of that thread). Everyone only touches their own slice, then syncs and reads it.
  Decomp decomposition_phase1(bool use_default_streams) {
    Decomp d;
    for (std::size_t t=0; t<DT; ++t) {
      d.th.push_back(spawn_host_thread());
      d.st.push_back((use_default_streams) ? nullptr : spawn_device_stream());
    }
    d.in  = malloc_array(d.th[0], DT*DS);
    d.out = malloc_array(d.th[0], DT*DS);
    auto s_init = generate_metadata(), s_launch = generate_metadata(), s_k_in = generate_metadata();
    auto s_k_out = generate_metadata(), s_sync = generate_metadata(), s_scan = generate_metadata();

    for (std::size_t t=0; t<DT; ++t) host_fill(d.th[t], d.in, t*DS, DS, s_init);
    std::vector<jobId_t> jobs;
    for (std::size_t t=0; t<DT; ++t) jobs.push_back(launch_event(d.th[t], d.st[t], s_launch));
    for (std::size_t e=0; e<DS; ++e)                      // the kernels run concurrently
      for (std::size_t t=0; t<DT; ++t) {
        DeviceThreadId dt = gpu_thread(jobs[t], e);
        dread(jobs[t], d.in, t*DS+e, s_k_in, dt);
        dwrite(jobs[t], d.out, t*DS+e, s_k_out, dt);
      }
    for (std::size_t t=0; t<DT; ++t) device_job_completes_event(jobs[t]);
    for (std::size_t t=0; t<DT; ++t) {
      sync_event(d.th[t], d.st[t], s_sync);
      host_scan(d.th[t], d.out, t*DS, DS, s_scan);
    }
    return d;
  }
  /// threads 1 and 3 re-run their kernels and sync THEIR OWN stream; then threads 0 and 2 read
  /// those slices without ever having synced those streams.
  void decomposition_phase2(const Decomp& d) {
    auto s_launch = generate_metadata(), s_k_in = generate_metadata(), s_k_out = generate_metadata();
    auto s_sync = generate_metadata(), s_xread = generate_metadata();
    for (std::size_t t : {1u, 3u}) {
      auto jid = launch_event(d.th[t], d.st[t], s_launch);
      grid(jid, 0, DS, [&](std::size_t e, DeviceThreadId& dt) {
        dread(jid, d.in, t*DS+e, s_k_in, dt);
        dwrite(jid, d.out, t*DS+e, s_k_out, dt);
      });
      device_job_completes_event(jid);
      sync_event(d.th[t], d.st[t], s_sync);               // synced on the owner thread only
    }
    host_scan(d.th[0], d.out, 1*DS, DS, s_xread);
    host_scan(d.th[2], d.out, 3*DS, DS, s_xread);
  }

  /// each of the given host threads fills its slice of `in`, launches a kernel on the DEFAULT stream
  /// (read in, write out over its slice) and lets it complete. Returns the `out` base address.
  std::uintptr_t default_stream_slices(const std::vector<HostThreadId>& th) {
    auto in  = malloc_array(th[0], th.size()*DS);
    auto out = malloc_array(th[0], th.size()*DS);
    auto s_init = generate_metadata();
    auto s_k_in = generate_metadata(), s_k_out = generate_metadata();
    for (std::size_t t=0; t<th.size(); ++t) {
      host_fill(th[t], in, t*DS, DS, s_init);
      auto jid = launch_event(th[t]);                     // default stream
      grid(jid, 0, DS, [&](std::size_t e, DeviceThreadId& dt) {
        dread(jid, in, t*DS+e, s_k_in, dt);
        dwrite(jid, out, t*DS+e, s_k_out, dt);
      });
      device_job_completes_event(jid);
    }
    return out;
  }
};


// ===========================================================================
// CLEAN WORKLOADS: nothing at all may be reported
// ===========================================================================

// y = alpha*x + y over 64K elements on the DEFAULT stream; every thread reads the same `alpha`
TEST_F(SM_Stress, saxpy_broadcast_scalar_default_stream_clean) {
  constexpr std::size_t N = 1u << 16;
  auto tid = spawn_host_thread();
  auto alpha = malloc_array(tid, 1u);
  auto x = malloc_array(tid, N);
  auto y = malloc_array(tid, N);
  auto s_alpha_w = generate_metadata(), s_x_w = generate_metadata(), s_y_w = generate_metadata();
  auto s_alpha_r = generate_metadata(), s_x_r = generate_metadata();
  auto s_y_r = generate_metadata(), s_y_kw = generate_metadata(), s_out = generate_metadata();

  hwrite(tid, alpha, 0, s_alpha_w);
  host_fill(tid, x, 0, N, s_x_w);
  host_fill(tid, y, 0, N, s_y_w);
  auto jid = launch_event(tid);
  grid(jid, 0, N, [&](std::size_t t, DeviceThreadId& dt) {
    dread(jid, alpha, 0, s_alpha_r, dt);
    dread(jid, x, t, s_x_r, dt);
    dread(jid, y, t, s_y_r, dt);
    dwrite(jid, y, t, s_y_kw, dt);
  });
  device_job_completes_event(jid);
  sync_event(tid);
  host_scan(tid, y, 0, N, s_out);
  free_event(tid, alpha);
  free_event(tid, x);
  free_event(tid, y);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

// 8 stencil sweeps on an explicit stream, host syncs and checks after every kernel
TEST_F(SM_Stress, stencil_host_synced_each_step_clean) {
  stencil_run(1u << 13, 8u, /*host_sync_each_step=*/true, /*use_default_stream=*/false);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

// same, on the default stream
TEST_F(SM_Stress, stencil_host_synced_each_step_default_stream_clean) {
  stencil_run(1u << 13, 8u, /*host_sync_each_step=*/true, /*use_default_stream=*/true);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

// 8 back-to-back stencil kernels on ONE explicit stream with no host sync in between: consecutive
// kernels on a stream are ordered, so kernel k+1 reading kernel k's output is fine.
// NOTE: this relies on the SM treating same-stream job ordering as protection. If it reports
//       here, that is the behaviour to look at first.
TEST_F(SM_Stress, stencil_stream_ordered_chain_no_host_sync_clean) {
  stencil_run(1u << 13, 8u, /*host_sync_each_step=*/false, /*use_default_stream=*/false);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

// per-block partial sums: every thread reads in[t], thread 0 of each block writes partial[block]
TEST_F(SM_Stress, reduction_block_partials_clean) {
  constexpr std::size_t N = 1u << 15, BLOCKS = N / BLOCK_DIM;
  auto tid = spawn_host_thread();
  auto stream = spawn_device_stream();
  auto in = malloc_array(tid, N);
  auto partial = malloc_array(tid, BLOCKS);
  auto result = malloc_array(tid, 1u);
  auto s_init = generate_metadata(), s_k_in = generate_metadata(), s_k_part = generate_metadata();
  auto s_part_r = generate_metadata(), s_res_w = generate_metadata(), s_res_r = generate_metadata();

  host_fill(tid, in, 0, N, s_init);
  auto jid = launch_event(tid, stream);
  grid(jid, 0, N, [&](std::size_t t, DeviceThreadId& dt) {
    dread(jid, in, t, s_k_in, dt);
    if (t % BLOCK_DIM == 0) dwrite(jid, partial, t / BLOCK_DIM, s_k_part, dt);
  });
  device_job_completes_event(jid);
  sync_event(tid, stream);
  host_scan(tid, partial, 0, BLOCKS, s_part_r);     // final reduction on the host
  hwrite(tid, result, 0, s_res_w);
  hread(tid, result, 0, s_res_r);
  free_event(tid, in);
  free_event(tid, partial);
  free_event(tid, result);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u) << get_results();
}

// 16 chunks through two buffers / two explicit streams, host syncs before reusing a slot
TEST_F(SM_Stress, double_buffered_streaming_clean) {
  streaming_pipeline(16u, 4096u, NO_SKIP, /*slot0_default_stream=*/false);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

#ifdef SM_TEST_DEFAULT_STREAM_PER_THREAD
// same pipeline with slot 0 on the default stream and slot 1 on an explicit stream
TEST_F(SM_Stress, double_buffered_streaming_default_plus_explicit_clean) {
  streaming_pipeline(16u, 4096u, NO_SKIP, /*slot0_default_stream=*/true);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues()) << get_results();
  EXPECT_EQ(status_count_all(), 0u);
}
#endif //? SM_TEST_DEFAULT_STREAM_PER_THREAD

// 64 allocations of varying size alternating between the default stream and an explicit stream:
// malloc, host init, kernel in place, sync, readback, free
TEST_F(SM_Stress, allocation_churn_clean) {
  constexpr std::size_t BUFFERS = 64u;
  auto tid = spawn_host_thread();
  hipStream_t st[2] = { nullptr, spawn_device_stream() };   // [0] == default stream
  auto s_malloc = generate_metadata(), s_fill = generate_metadata(), s_launch = generate_metadata();
  auto s_k_r = generate_metadata(), s_k_w = generate_metadata();
  auto s_sync = generate_metadata(), s_scan = generate_metadata(), s_free = generate_metadata();

  for (std::size_t b=0; b<BUFFERS; ++b) {
    std::size_t n = 256u + (b % 8u) * 256u;           // 1 KiB .. 8 KiB
    auto buf = malloc_array(tid, n, s_malloc);
    host_fill(tid, buf, 0, n, s_fill);
    auto jid = launch_event(tid, st[b % 2u], s_launch);
    grid(jid, 0, n, [&](std::size_t t, DeviceThreadId& dt) {
      dread(jid, buf, t, s_k_r, dt);
      dwrite(jid, buf, t, s_k_w, dt);
    });
    device_job_completes_event(jid);
    sync_event(tid, st[b % 2u], s_sync);
    host_scan(tid, buf, 0, n, s_scan);
    free_event(tid, buf, s_free);
  }

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}

// 50 solver iterations on the default stream: host writes a scalar parameter, kernel updates x and a
// residual, host syncs and reads the residual. Hot addresses bounce between host and device every round.
TEST_F(SM_Stress, iterative_host_device_ping_pong_default_stream_clean) {
  constexpr std::size_t N = 2048u, ITERS = 50u;
  auto tid = spawn_host_thread();
  auto param = malloc_array(tid, 1u);
  auto x = malloc_array(tid, N);
  auto res = malloc_array(tid, 1u);
  auto s_x_init = generate_metadata(), s_param_w = generate_metadata(), s_launch = generate_metadata();
  auto s_param_r = generate_metadata(), s_x_r = generate_metadata(), s_x_w = generate_metadata();
  auto s_res_w = generate_metadata(), s_sync = generate_metadata(), s_res_r = generate_metadata();

  host_fill(tid, x, 0, N, s_x_init);
  for (std::size_t it=0; it<ITERS; ++it) {
    hwrite(tid, param, 0, s_param_w);
    auto jid = launch_event(tid, nullptr, s_launch);
    grid(jid, 0, N, [&](std::size_t t, DeviceThreadId& dt) {
      dread(jid, param, 0, s_param_r, dt);
      dread(jid, x, t, s_x_r, dt);
      dwrite(jid, x, t, s_x_w, dt);
      if (t == 0) dwrite(jid, res, 0, s_res_w, dt);
    });
    device_job_completes_event(jid);
    sync_event(tid, nullptr, s_sync);
    hread(tid, res, 0, s_res_r);
  }
  free_event(tid, param);
  free_event(tid, x);
  free_event(tid, res);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues());
  EXPECT_EQ(status_count_all(), 0u);
}


// ===========================================================================
// RACY WORKLOADS
// ===========================================================================

// default-plus-explicit pipeline where the host forgets the sync before collecting chunk 6's
// result (chunk 6 lives in slot 0, i.e. the default stream)
TEST_F(SM_Stress, double_buffered_streaming_missing_sync_on_default_stream) {
  streaming_pipeline(16u, 4096u, /*skip_sync_at=*/6u, /*slot0_default_stream=*/true);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_race_loose());
  EXPECT_GE(unsynced_read_reports(), 1u);
}

// same pipeline on two explicit streams, chunk 7 (slot 1) is the one missing its sync
TEST_F(SM_Stress, double_buffered_streaming_missing_sync_on_explicit_stream) {
  streaming_pipeline(16u, 4096u, /*skip_sync_at=*/7u, /*slot0_default_stream=*/false);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_race_loose());
  EXPECT_GE(unsynced_read_reports(), 1u);
}

// 4 host threads, each owning a slice of one big buffer and its own EXPLICIT stream.
// Phase 1 only touches own synced slices -> clean. Phase 2: cross-thread reads without a sync.
TEST_F(SM_Stress, multi_host_thread_domain_decomposition) {
  auto d = decomposition_phase1(/*use_default_streams=*/false);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;
  EXPECT_EQ(status_count_all(), 0u) << "each thread only used its own synced slice";

  decomposition_phase2(d);

  run();
  iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_race_loose());
  EXPECT_GE(unsynced_read_reports(), 1u);
}

// Host reads the tail of the output while the kernel (default stream) is still running; the kernel
// writes that tail last. The host reads are the first host events after the launch (logical time
// V+2..V+9) and the device writes of those elements are the last ~16 of a 32K-event job, so HR
// precedes DW in logical time.
TEST_F(SM_Stress, host_read_during_kernel_true_race) {
  constexpr std::size_t N = 1u << 14, TAIL = 8u;
  auto tid = spawn_host_thread();
  auto in = malloc_array(tid, N);
  auto out = malloc_array(tid, N);
  auto s_in_w = generate_metadata(), s_out_w = generate_metadata(), s_hr = generate_metadata();
  auto s_k_in = generate_metadata(), s_k_out = generate_metadata();

  host_fill(tid, in, 0, N, s_in_w);
  host_fill(tid, out, 0, N, s_out_w);
  auto jid = launch_event(tid);
  host_scan(tid, out, N - TAIL, TAIL, s_hr);            // no sync: job is still running
  grid(jid, 0, N, [&](std::size_t t, DeviceThreadId& dt) {
    dread(jid, in, t, s_k_in, dt);
    dwrite(jid, out, t, s_k_out, dt);
  });
  device_job_completes_event(jid);
  sync_event(tid);
  free_event(tid, in);
  free_event(tid, out);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_true_race());
  EXPECT_GE(status_count(Status::RACE_HR_DW), 1u);
}

// Every thread reads a small coefficient table (coeff[t % 8]) on an explicit stream; the host rewrites
// that table in the middle of the kernel. The first 8 threads issue 24 device events (logical time
// V+2..V+25), the host spends 4096 events on unrelated scratch writes and then rewrites the table
// (V+4098..V+4105), while the remaining ~49K device events keep running and keep reading the table:
// DR (early), HW, DR (late) in logical time.
TEST_F(SM_Stress, host_rewrites_coefficients_during_kernel_true_race) {
  constexpr std::size_t N = 1u << 14, WIN = 8u, SCRATCH = 4096u;
  auto tid = spawn_host_thread();
  auto stream = spawn_device_stream();
  auto coeff = malloc_array(tid, WIN);
  auto in = malloc_array(tid, N);
  auto out = malloc_array(tid, N);
  auto scratch = malloc_array(tid, SCRATCH);
  auto s_coeff_w = generate_metadata(), s_in_w = generate_metadata();
  auto s_k_coeff = generate_metadata(), s_k_in = generate_metadata(), s_k_out = generate_metadata();
  auto s_scratch_w = generate_metadata(), s_coeff_rw = generate_metadata();

  host_fill(tid, coeff, 0, WIN, s_coeff_w);
  host_fill(tid, in, 0, N, s_in_w);
  auto jid = launch_event(tid, stream);
  auto kernel = [&](std::size_t t, DeviceThreadId& dt) {
    dread(jid, coeff, t % WIN, s_k_coeff, dt);
    dread(jid, in, t, s_k_in, dt);
    dwrite(jid, out, t, s_k_out, dt);
  };
  grid(jid, 0, WIN, kernel);                            // first threads read the table
  host_fill(tid, scratch, 0, SCRATCH, s_scratch_w);     // host busy with something else...
  host_fill(tid, coeff, 0, WIN, s_coeff_rw);            // ...then updates the table, no sync
  grid(jid, WIN, N, kernel);                            // the rest of the grid keeps reading it
  device_job_completes_event(jid);
  sync_event(tid, stream);
  free_event(tid, coeff);
  free_event(tid, in);
  free_event(tid, out);
  free_event(tid, scratch);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_true_race());
  EXPECT_GE(status_count(Status::RACE_DR_HW), 1u);
}


// ===========================================================================
// UNINITIALIZED MEMORY AT SCALE
// ===========================================================================

// host initializes only the first 3/4 of the input; the kernel (default stream) reads all of it
TEST_F(SM_Stress, device_reads_partially_initialized_buffer) {
  constexpr std::size_t N = 1u << 15;
  auto tid = spawn_host_thread();
  auto in = malloc_array(tid, N);
  auto s_init = generate_metadata(), s_k_in = generate_metadata();

  host_fill(tid, in, 0, (N / 4u) * 3u, s_init);
  auto jid = launch_event(tid);
  grid(jid, 0, N, [&](std::size_t t, DeviceThreadId& dt) {
    dread(jid, in, t, s_k_in, dt);
  });
  device_job_completes_event(jid);
  sync_event(tid);
  free_event(tid, in);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_GE(status_count(Status::READ_UNINIT_D), 1u);
  EXPECT_EQ(status_count(Status::READ_UNINIT_H), 0u);
  EXPECT_FALSE(any_race());
}

// kernel (explicit stream) only fills the first half of the output; after a proper sync the host
// reads all of it
TEST_F(SM_Stress, host_reads_partially_written_output) {
  constexpr std::size_t N = 1u << 15;
  auto tid = spawn_host_thread();
  auto stream = spawn_device_stream();
  auto out = malloc_array(tid, N);
  auto s_k_out = generate_metadata(), s_scan = generate_metadata();

  auto jid = launch_event(tid, stream);
  grid(jid, 0, N / 2u, [&](std::size_t t, DeviceThreadId& dt) {
    dwrite(jid, out, t, s_k_out, dt);
  });
  device_job_completes_event(jid);
  sync_event(tid, stream);
  host_scan(tid, out, 0, N, s_scan);
  free_event(tid, out);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_GE(status_count(Status::READ_UNINIT_H), 1u);
  EXPECT_EQ(status_count(Status::READ_UNINIT_D), 0u);
  EXPECT_FALSE(any_race());
}


// ===========================================================================
// DEFAULT-STREAM MODE DEPENDENT (compile-time switch)
// ===========================================================================

#ifdef SM_TEST_DEFAULT_STREAM_LEGACY
// legacy mode: all 4 host threads launch kernels on the one shared default stream; a single sync of
// it by thread 0 covers every thread's kernel, so thread 0 may read every slice.
// NOTE: assumes the SM keys the zone on the shared default stream rather than on the launcher.
TEST_F(SM_Stress, legacy_default_stream_one_sync_covers_all_threads_clean) {
  ASSERT_TRUE(DEFAULT_STREAM_BEHAVIOR() == hipStreamLegacy_ull) 
    << "backend is not using the legacy default stream behavior";
  std::vector<HostThreadId> th;
  for (std::size_t t=0; t<DT; ++t) th.push_back(spawn_host_thread());
  auto out = default_stream_slices(th);
  auto s_scan = generate_metadata();

  sync_event(th[0]);                                    // shared default stream
  host_scan(th[0], out, 0, DT*DS, s_scan);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_FALSE(any_issues())  << get_results();
  EXPECT_EQ(status_count_all(), 0u);
}
#endif //? SM_TEST_DEFAULT_STREAM_LEGACY

#ifdef SM_TEST_DEFAULT_STREAM_PER_THREAD
// per-thread mode: every host thread's default stream is its own, so thread 0 syncing its default
// stream covers only its own slice. Reading its own slice is clean; reading the others is not.
TEST_F(SM_Stress, per_thread_default_stream_sync_covers_only_own_stream) {
  ASSERT_TRUE(DEFAULT_STREAM_BEHAVIOR() == hipStreamPerThread_ull) 
    << "backend is not using per-thread default stream behavior";
  std::vector<HostThreadId> th;
  for (std::size_t t=0; t<DT; ++t) th.push_back(spawn_host_thread());
  auto out = default_stream_slices(th);
  auto s_own = generate_metadata(), s_other = generate_metadata();

  sync_event(th[0]);                                    // thread 0's own default stream
  host_scan(th[0], out, 0, DS, s_own);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;
  EXPECT_EQ(status_count_all(), 0u) << "thread 0's own slice is covered by its own sync";

  host_scan(th[0], out, DS, (DT-1)*DS, s_other);        // other threads' streams were never synced

  run();
  iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_race_loose());
  EXPECT_GE(unsynced_read_reports(), 1u);
}

// per-thread mode: the domain decomposition where every thread uses the default stream (= its own
// stream); behaves exactly like the explicit-stream version.
TEST_F(SM_Stress, per_thread_default_streams_domain_decomposition) {
  ASSERT_TRUE(DEFAULT_STREAM_BEHAVIOR() == hipStreamPerThread_ull) 
    << "backend is not using per-thread default stream behavior";
  auto d = decomposition_phase1(/*use_default_streams=*/true);

  run();
  auto iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;
  EXPECT_EQ(status_count_all(), 0u) << "each thread only used its own synced slice";

  decomposition_phase2(d);

  run();
  iErrs = get_internal_errors();
  ASSERT_FALSE(iErrs) << iErrs;

  EXPECT_TRUE(any_race_loose());
  EXPECT_GE(unsynced_read_reports(), 1u);
}
#endif //? SM_TEST_DEFAULT_STREAM_PER_THREAD

} //? namespace test
} //? namespace rtl
} //? namespace scabbard
