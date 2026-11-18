// =============================================================================
// graph.hip -- synthetic CSR graph analytics (scientific / data-intensive)
//
//   1. host builds the CSR row offsets directly in unified memory, GPUs generate the edges
//   2. out-degree histogram (LDS histogram merged into a UM histogram)
//   3. push-style PageRank with fixed-point atomics (deterministic result)
//   4. min-label propagation (connected-components style) iterated until a UM flag stays clear
//
// Work delegation
//   * W host worker threads, worker w on device (w % ndev), each owns a vertex range
//   * PageRank push kernel is split over two non-blocking streams per worker; finalize,
//     histogram and label propagation run on the DEFAULT stream (legacy or per-thread)
//   * phases are separated by host stream syncs + a thread barrier (no device-wide sync)
//   * a host callback reports per-iteration PageRank delta
//   * atomics are system-scope (atomic*_system): several devices hit the same locations
//
// UM ACCESS ANNOTATION LEGEND (applies to every file of the suite)
//   // UM: R=<n> W=<n> A=<n>
//        static number of reads / writes / atomic-RMWs to unified memory
//        (hipMallocManaged) performed by that source line, host OR device code.
//   * counted per leaf scalar: copying a Vec3<float> from UM is R=3; a Cell{u,v}
//     store is W=2. `x[i] += y` is R=1 W=1. (The compiler may merge these into
//     wider accesses at IR level; the counts are source-level.)
//   * a conditional/looped access is counted once per static occurrence.
//   * accesses are attributed to the line where the load/store/atomic is written,
//     including inside inlined helpers/functors (call lines carry no tag).
//   * (sometimes: R,W,A) = the same line is also executed with a pointer that is
//     NOT unified memory (hipMalloc scratch, LDS, stack). Listed kinds are the
//     ones that are only sometimes UM.
//   * lines with no tag perform no UM access. Locals / LDS / scratch are untagged.
//   * atomic-RMW counts include "atomic read" idioms such as atomicOr(p, 0).
// =============================================================================
#include <hip/hip_runtime.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#define HIP_CHECK(expr)                                                                      \
  do {                                                                                       \
    hipError_t e_ = (expr);                                                                  \
    if (e_ != hipSuccess) {                                                                  \
      std::fprintf(stderr, "HIP error '%s' (%d) at %s:%d: %s\n", #expr, (int)e_, __FILE__,  \
                   __LINE__, hipGetErrorString(e_));                                         \
      std::exit(2);                                                                          \
    }                                                                                        \
  } while (0)

constexpr int NVERT = 1 << 13;
constexpr int PR_ITERS = 10;
constexpr int CC_MAX_ITERS = 64;
constexpr int NBINS = 16;
constexpr int BS = 128;

// ----------------------------------------------------------------------------
// utilities
// ----------------------------------------------------------------------------
template <typename T>
class Managed {
 public:
  explicit Managed(size_t n) : n_(n) {
    HIP_CHECK(hipMallocManaged(reinterpret_cast<void**>(&p_), n * sizeof(T)));
  }
  ~Managed() {
    if (p_) (void)hipFree(p_);
  }
  Managed(const Managed&) = delete;
  Managed& operator=(const Managed&) = delete;
  T* get() const { return p_; }
  size_t size() const { return n_; }

 private:
  T* p_ = nullptr;
  size_t n_;
};

class Barrier {
 public:
  explicit Barrier(int n) : n_(n) {}
  void arrive_and_wait() {
    std::unique_lock<std::mutex> l(m_);
    const int g = gen_;
    if (++count_ == n_) {
      count_ = 0;
      ++gen_;
      cv_.notify_all();
    } else {
      cv_.wait(l, [&] { return g != gen_; });
    }
  }

 private:
  std::mutex m_;
  std::condition_variable cv_;
  int n_, count_ = 0, gen_ = 0;
};

__host__ __device__ inline uint64_t mix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

// ----------------------------------------------------------------------------
// data types
// ----------------------------------------------------------------------------
template <typename I>
struct Csr {  // non-owning CSR view; row/col point at unified memory
  I* row;
  I* col;
  I n;
  I nnz;
};

template <int Bits>
struct FixedPoint {
  unsigned long long raw;
  static constexpr unsigned long long ONE = 1ull << Bits;
  __device__ void atomic_add(unsigned long long d) {
    atomicAdd_system(&raw, d);  // UM: A=1
  }
};
using Rank = FixedPoint<40>;

__host__ __device__ inline int out_degree(int v) {
  uint64_t h = mix64((uint64_t)v * 0x9E37ull + 5);
  int d = 1 + (int)(h % 12);
  if (((h >> 20) % 97) == 0) d += 48;  // occasional hub
  return d;
}
__host__ __device__ inline int edge_target(int v, int k, int n) {
  uint64_t h = mix64(((uint64_t)v << 20) ^ (uint64_t)k ^ 0xABCDEFull);
  if ((h & 15) == 0) return (int)((h >> 8) % (uint64_t)n);  // long-range edge
  return (int)((v + 1 + (h >> 8) % 32) % n);                // local band edge
}

// ----------------------------------------------------------------------------
// kernels
// ----------------------------------------------------------------------------
template <typename U>
__global__ void fill_kernel(U* p, size_t n, U v) {
  size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x;
  if (i < n) p[i] = v;  // UM: W=1
}

__global__ void init_state(Rank* cur, Rank* nxt, int* label, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  cur[i].raw = Rank::ONE / (unsigned long long)n;  // UM: W=1
  nxt[i].raw = 0ull;                                // UM: W=1
  label[i] = i;                                     // UM: W=1
}

__global__ void gen_edges(Csr<int> g) {
  int v = blockIdx.x * blockDim.x + threadIdx.x;
  if (v >= g.n) return;
  int beg = g.row[v], end = g.row[v + 1];  // UM: R=2
  for (int e = beg; e < end; ++e) {
    g.col[e] = edge_target(v, e - beg, g.n);  // UM: W=1
  }
}

// shared by the LDS histogram (block local) and the UM histogram (global merge)
template <typename U>
__device__ inline void bin_add(U* bins, int b, U n) {
  atomicAdd_system(&bins[b], n);  // UM: A=1 (sometimes: A)
}

__global__ void degree_hist(Csr<int> g, unsigned int* bins, int vb, int ve) {
  __shared__ unsigned int lb[NBINS];
  for (int i = threadIdx.x; i < NBINS; i += blockDim.x) lb[i] = 0u;
  __syncthreads();
  const int v = vb + blockIdx.x * blockDim.x + threadIdx.x;
  if (v < ve) {
    int d = g.row[v + 1] - g.row[v];  // UM: R=2
    bin_add(lb, min(d / 4, NBINS - 1), 1u);  // LDS target (atomic counted in bin_add)
  }
  __syncthreads();
  for (int i = threadIdx.x; i < NBINS; i += blockDim.x) {
    unsigned int c = lb[i];
    if (c) bin_add(bins, i, c);  // UM target (atomic counted in bin_add)
  }
}

__global__ void pagerank_push(Csr<int> g, Rank* cur, Rank* nxt, int vb, int ve) {
  const int v = vb + blockIdx.x * blockDim.x + threadIdx.x;
  if (v >= ve) return;
  int beg = g.row[v], end = g.row[v + 1];  // UM: R=2
  unsigned long long r = cur[v].raw;       // UM: R=1
  unsigned long long share = (r * 85ull / 100ull) / (unsigned long long)(end - beg);
  for (int e = beg; e < end; ++e) {
    int u = g.col[e];  // UM: R=1
    nxt[u].atomic_add(share);  // (atomic counted in FixedPoint::atomic_add)
  }
}

__global__ void pagerank_finalize(Rank* cur, Rank* nxt, int vb, int ve, unsigned long long base,
                                  unsigned long long* l1) {
  const int v = vb + blockIdx.x * blockDim.x + threadIdx.x;
  if (v >= ve) return;
  unsigned long long nv = base + nxt[v].raw;  // UM: R=1
  unsigned long long old = cur[v].raw;        // UM: R=1
  cur[v].raw = nv;                            // UM: W=1
  nxt[v].raw = 0ull;                          // UM: W=1
  atomicAdd_system(l1, nv > old ? nv - old : old - nv);  // UM: A=1
}

__global__ void cc_propagate(Csr<int> g, int* label, unsigned int* changed, int vb, int ve) {
  const int v = vb + blockIdx.x * blockDim.x + threadIdx.x;
  if (v >= ve) return;
  int lv = atomicOr_system(&label[v], 0);  // UM: A=1 (atomic read idiom)
  int beg = g.row[v], end = g.row[v + 1];  // UM: R=2
  bool any = false;
  for (int e = beg; e < end; ++e) {
    int u = g.col[e];                          // UM: R=1
    int old = atomicMin_system(&label[u], lv);  // UM: A=1
    if (old > lv) any = true;
  }
  if (any) atomicOr_system(changed, 1u);  // UM: A=1
}

// ----------------------------------------------------------------------------
// host side
// ----------------------------------------------------------------------------
struct IterCtx {
  const unsigned long long* delta;  // points into UM
  int iter;
};
static std::mutex g_print_mu;

static void on_pr_iter(void* arg) {  // host callback: no HIP calls allowed here
  auto* c = static_cast<IterCtx*>(arg);
  unsigned long long d = *c->delta;  // UM: R=1
  std::lock_guard<std::mutex> lk(g_print_mu);
  std::printf("  [callback] pagerank iter %2d  L1 delta=%.6f\n", c->iter,
              (double)d / (double)Rank::ONE);
}

struct Shared {
  Csr<int> g;
  Rank *cur, *nxt;
  int* label;
  unsigned int *changed, *bins;
  unsigned long long* delta;
  Barrier* bar;
  std::vector<IterCtx>* ctxs;
  std::atomic<bool> more{true};
  std::atomic<int> cc_iters{0};
};

static inline unsigned grid_for(int n) { return (unsigned)((n + BS - 1) / BS); }

static void worker_main(int w, int dev, int nworkers, Shared& S) {
  HIP_CHECK(hipSetDevice(dev));
  const int vb = (int)((long long)NVERT * w / nworkers);
  const int ve = (int)((long long)NVERT * (w + 1) / nworkers);
  const int vm = vb + (ve - vb) / 2;
  hipStream_t s[2];
  for (auto& x : s) HIP_CHECK(hipStreamCreateWithFlags(&x, hipStreamNonBlocking));
  hipStream_t cb_stream = nullptr;
  if (w == 0) HIP_CHECK(hipStreamCreateWithFlags(&cb_stream, hipStreamNonBlocking));
  Barrier& bar = *S.bar;

  // --- degree histogram on the default stream ---
  if (ve > vb) degree_hist<<<grid_for(ve - vb), BS, 0, nullptr>>>(S.g, S.bins, vb, ve);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipStreamSynchronize(nullptr));
  bar.arrive_and_wait();

  // --- PageRank ---
  const unsigned long long base = Rank::ONE * 15ull / 100ull / (unsigned long long)NVERT;
  for (int it = 0; it < PR_ITERS; ++it) {
    if (vm > vb) pagerank_push<<<grid_for(vm - vb), BS, 0, s[0]>>>(S.g, S.cur, S.nxt, vb, vm);
    if (ve > vm) pagerank_push<<<grid_for(ve - vm), BS, 0, s[1]>>>(S.g, S.cur, S.nxt, vm, ve);
    HIP_CHECK(hipGetLastError());
    HIP_CHECK(hipStreamSynchronize(s[0]));
    HIP_CHECK(hipStreamSynchronize(s[1]));
    bar.arrive_and_wait();  // all pushes landed in nxt

    if (ve > vb) pagerank_finalize<<<grid_for(ve - vb), BS, 0, nullptr>>>(S.cur, S.nxt, vb, ve, base, S.delta + it);
    HIP_CHECK(hipGetLastError());
    HIP_CHECK(hipStreamSynchronize(nullptr));
    bar.arrive_and_wait();  // cur is final for this iteration, delta[it] complete

    if (w == 0) HIP_CHECK(hipLaunchHostFunc(cb_stream, on_pr_iter, &(*S.ctxs)[it]));
  }

  // --- label propagation until the UM flag stays clear ---
  for (int it = 0; it < CC_MAX_ITERS; ++it) {
    if (w == 0) *S.changed = 0u;  // UM: W=1 (host write)
    bar.arrive_and_wait();

    if (ve > vb) cc_propagate<<<grid_for(ve - vb), BS, 0, nullptr>>>(S.g, S.label, S.changed, vb, ve);
    HIP_CHECK(hipGetLastError());
    HIP_CHECK(hipStreamSynchronize(nullptr));
    bar.arrive_and_wait();

    if (w == 0) {
      S.more.store(*S.changed != 0u);  // UM: R=1 (host read)
      S.cc_iters.store(it + 1);
    }
    bar.arrive_and_wait();
    if (!S.more.load()) break;
  }

  if (w == 0) {
    HIP_CHECK(hipStreamSynchronize(cb_stream));
    HIP_CHECK(hipStreamDestroy(cb_stream));
  }
  for (auto& x : s) HIP_CHECK(hipStreamDestroy(x));
}

int main(int argc, char** argv) {
  int ndev = 0;
  HIP_CHECK(hipGetDeviceCount(&ndev));
  if (ndev < 1) {
    std::fprintf(stderr, "no HIP device\n");
    return 1;
  }
  const int workers = argc > 1 ? std::max(1, std::atoi(argv[1])) : std::max(2, ndev);
  std::printf("graph: %d device(s), %d worker thread(s), %d vertices\n", ndev, workers, NVERT);

  // CSR row offsets built on the host, in unified memory
  Managed<int> rowbuf(NVERT + 1);
  int* row = rowbuf.get();
  row[0] = 0;  // UM: W=1
  for (int v = 0; v < NVERT; ++v) row[v + 1] = row[v] + out_degree(v);  // UM: R=1 W=1
  const int nnz = row[NVERT];  // UM: R=1
  Managed<int> colbuf(nnz);

  Managed<Rank> cur(NVERT), nxt(NVERT);
  Managed<int> label(NVERT);
  Managed<unsigned int> changed(1), bins(NBINS);
  Managed<unsigned long long> delta(PR_ITERS);

  Csr<int> g{rowbuf.get(), colbuf.get(), NVERT, nnz};
  gen_edges<<<grid_for(NVERT), BS, 0, nullptr>>>(g);
  init_state<<<grid_for(NVERT), BS, 0, nullptr>>>(cur.get(), nxt.get(), label.get(), NVERT);
  fill_kernel<unsigned int><<<1, 64, 0, nullptr>>>(bins.get(), NBINS, 0u);
  fill_kernel<unsigned int><<<1, 64, 0, nullptr>>>(changed.get(), 1, 0u);
  fill_kernel<unsigned long long><<<1, 64, 0, nullptr>>>(delta.get(), PR_ITERS, 0ull);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipStreamSynchronize(nullptr));

  Barrier bar(workers);
  std::vector<IterCtx> ctxs(PR_ITERS);
  for (int i = 0; i < PR_ITERS; ++i) ctxs[i] = IterCtx{delta.get() + i, i};
  Shared S;
  S.g = g;
  S.cur = cur.get();
  S.nxt = nxt.get();
  S.label = label.get();
  S.changed = changed.get();
  S.bins = bins.get();
  S.delta = delta.get();
  S.bar = &bar;
  S.ctxs = &ctxs;

  std::vector<std::thread> th;
  for (int w = 0; w < workers; ++w) th.emplace_back(worker_main, w, w % ndev, workers, std::ref(S));
  for (auto& t : th) t.join();

  // ---- host verification over UM ----
  unsigned long long hist_total = 0;
  for (int i = 0; i < NBINS; ++i) hist_total += bins.get()[i];  // UM: R=1

  unsigned long long rank_sum = 0;
  for (int v = 0; v < NVERT; ++v) rank_sum += cur.get()[v].raw;  // UM: R=1

  long long violations = 0, roots = 0;
  for (int v = 0; v < NVERT; ++v) {
    int beg = row[v], end = row[v + 1];  // UM: R=2
    int lv = label.get()[v];             // UM: R=1
    if (lv == v) ++roots;
    for (int e = beg; e < end; ++e) {
      int u = colbuf.get()[e];                // UM: R=1
      if (label.get()[u] > lv) ++violations;  // UM: R=1
    }
  }
  const double mass = (double)rank_sum / (double)Rank::ONE;
  std::printf("edges=%d hist_total=%llu rank_mass=%.6f cc_iters=%d label_roots=%lld violations=%lld\n", nnz,
              hist_total, mass, S.cc_iters.load(), roots, violations);
  const bool ok = hist_total == (unsigned long long)NVERT && mass > 0.95 && mass <= 1.0001 &&
                  violations == 0 && !S.more.load();
  return ok ? 0 : 1;
}
