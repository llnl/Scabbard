// =============================================================================
// reaction_diffusion.hip -- 3-D Gray-Scott reaction-diffusion (scientific stencil)
//
// Domain is split into z-slabs, one per host worker (worker w on device w % ndev).
// Halo exchange is implicit: a slab's stencil simply reads the neighbouring slab's
// cells through unified memory -- no hipMemcpy, no explicit halo buffers.
//
// Work delegation per worker and step
//   * stream A (non-blocking): interior planes of the slab
//   * stream B (non-blocking): the two boundary planes (these read other slabs' cells)
//   * DEFAULT stream (legacy or per-thread, per build flag): mass reduction, which waits on
//     events recorded on A and B (hipStreamWaitEvent, never a device-wide sync)
//   * host stream sync + a thread barrier once per step; worker 0 fires a host callback
//   * run twice from one template: <float, Stencil7<1>> and <double, Stencil7<2>>
//     (second one weights the z neighbours twice as much)
//   * atomics are system-scope (several devices update the same diagnostics record)
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
//   * Cell<T> has 2 leaf scalars (u, v); Diag has 2 (mass_u, mass_v).
// =============================================================================
#include <hip/hip_runtime.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
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

constexpr int NX = 64, NY = 64, NZ = 64;
constexpr int STEPS = 20;
constexpr int RBLK = 256;

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
__host__ __device__ inline unsigned long long to_fixed(double v) {  // two's-complement fixed point
  return (unsigned long long)(long long)llround(v * 1.0e6);
}

// ----------------------------------------------------------------------------
// data types
// ----------------------------------------------------------------------------
struct Grid {
  int nx, ny, nz;
  __host__ __device__ size_t index(int x, int y, int z) const { return ((size_t)z * ny + y) * nx + x; }
  __host__ __device__ static int wrap(int i, int n) { return i < 0 ? i + n : (i >= n ? i - n : i); }
};

template <typename T>
struct Cell {
  T u, v;
  __host__ __device__ Cell operator+(const Cell& o) const { return Cell{u + o.u, v + o.v}; }
  __host__ __device__ Cell operator*(T s) const { return Cell{u * s, v * s}; }
};

template <typename T>
struct Params {
  T du, dv, feed, kill, dt;
};

struct Diag {
  unsigned long long mass_u, mass_v;
};

// 7-point stencil policy; ZW weights the z neighbours (template parameter of the step kernel)
template <int ZW>
struct Stencil7 {
  template <typename T>
  __device__ static Cell<T> laplacian(const Cell<T>* f, const Grid& g, int x, int y, int z, const Cell<T>& c) {
    Cell<T> xm = f[g.index(Grid::wrap(x - 1, g.nx), y, z)];  // UM: R=2
    Cell<T> xp = f[g.index(Grid::wrap(x + 1, g.nx), y, z)];  // UM: R=2
    Cell<T> ym = f[g.index(x, Grid::wrap(y - 1, g.ny), z)];  // UM: R=2
    Cell<T> yp = f[g.index(x, Grid::wrap(y + 1, g.ny), z)];  // UM: R=2
    Cell<T> zm = f[g.index(x, y, Grid::wrap(z - 1, g.nz))];  // UM: R=2
    Cell<T> zp = f[g.index(x, y, Grid::wrap(z + 1, g.nz))];  // UM: R=2
    Cell<T> s = xm + xp + ym + yp + (zm + zp) * T(ZW);
    return s + c * T(-(4 + 2 * ZW));
  }
};

// block reduction (shared memory, never UM)
__device__ inline double block_sum(double v, double* red) {
  red[threadIdx.x] = v;
  __syncthreads();
  for (int o = blockDim.x / 2; o > 0; o >>= 1) {
    if (threadIdx.x < (unsigned)o) red[threadIdx.x] += red[threadIdx.x + o];
    __syncthreads();
  }
  double r = red[0];
  __syncthreads();
  return r;
}

// ----------------------------------------------------------------------------
// kernels
// ----------------------------------------------------------------------------
template <typename T>
__global__ void init_field(Cell<T>* f, Grid g) {
  size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x;
  if (i >= (size_t)g.nx * g.ny * g.nz) return;
  const int x = (int)(i % g.nx);
  const int y = (int)((i / g.nx) % g.ny);
  const int z = (int)(i / ((size_t)g.nx * g.ny));
  Cell<T> c{T(1), T(0)};
  const bool seed = ((x / 8 + 3 * (y / 8) + 5 * (z / 8)) % 7 == 0) && (mix64(i) & 1ull);
  if (seed) {
    c.u = T(0.5);
    c.v = T(0.25);
  }
  f[i] = c;  // UM: W=2
}

__global__ void init_diag(Diag* d, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) d[i] = Diag{0ull, 0ull};  // UM: W=2
}

// updates planes [zb, ze); neighbouring planes (possibly another worker's slab) are read from UM
template <typename T, typename St>
__global__ void step_kernel(const Cell<T>* cur, Cell<T>* nxt, Grid g, int zb, int ze, Params<T> p) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  const int z = zb + blockIdx.z;
  if (x >= g.nx || y >= g.ny || z >= ze) return;
  const size_t idx = g.index(x, y, z);
  Cell<T> c = cur[idx];  // UM: R=2
  Cell<T> lap = St::laplacian(cur, g, x, y, z, c);
  const T uvv = c.u * c.v * c.v;
  Cell<T> o;
  o.u = c.u + p.dt * (p.du * lap.u - uvv + p.feed * (T(1) - c.u));
  o.v = c.v + p.dt * (p.dv * lap.v + uvv - (p.feed + p.kill) * c.v);
  nxt[idx] = o;  // UM: W=2
}

template <typename T>
__global__ void reduce_mass(const Cell<T>* f, Grid g, int zb, int ze, Diag* d) {
  __shared__ double red[RBLK];
  const size_t total = (size_t)(ze - zb) * g.ny * g.nx;
  const size_t off = g.index(0, 0, zb);
  double su = 0.0, sv = 0.0;
  for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < total; i += (size_t)gridDim.x * blockDim.x) {
    Cell<T> c = f[off + i];  // UM: R=2
    su += (double)c.u;
    sv += (double)c.v;
  }
  su = block_sum(su, red);
  sv = block_sum(sv, red);
  if (threadIdx.x == 0) {
    atomicAdd_system(&d->mass_u, to_fixed(su));  // UM: A=1
    atomicAdd_system(&d->mass_v, to_fixed(sv));  // UM: A=1
  }
}

// ----------------------------------------------------------------------------
// host side
// ----------------------------------------------------------------------------
struct StepCtx {
  const Diag* d;
  int step;
  const char* tag;
};
static std::mutex g_print_mu;

static void on_step(void* arg) {  // host callback: no HIP calls allowed here
  auto* c = static_cast<StepCtx*>(arg);
  long long mu = static_cast<long long>(c->d->mass_u);  // UM: R=1
  long long mv = static_cast<long long>(c->d->mass_v);  // UM: R=1
  std::lock_guard<std::mutex> lk(g_print_mu);
  std::printf("  [%s] step %2d  sum(u)=%.3f  sum(v)=%.3f\n", c->tag, c->step, mu * 1e-6, mv * 1e-6);
}

template <typename T, typename St>
static void launch_step(hipStream_t s, const Cell<T>* cur, Cell<T>* nxt, Grid g, int zb, int ze, Params<T> p) {
  if (ze <= zb) return;
  dim3 block(32, 8, 1);
  dim3 grid(g.nx / 32, g.ny / 8, ze - zb);
  step_kernel<T, St><<<grid, block, 0, s>>>(cur, nxt, g, zb, ze, p);
  HIP_CHECK(hipGetLastError());
}

template <typename T, typename St>
static void sim_worker(int w, int dev, int nworkers, Cell<T>* bufA, Cell<T>* bufB, Grid g, Params<T> p,
                       Diag* diag, Barrier& bar, std::vector<StepCtx>& ctxs) {
  HIP_CHECK(hipSetDevice(dev));
  const int z0 = (int)((long long)g.nz * w / nworkers);
  const int z1 = (int)((long long)g.nz * (w + 1) / nworkers);
  hipStream_t sA, sB, cb_stream = nullptr;
  HIP_CHECK(hipStreamCreateWithFlags(&sA, hipStreamNonBlocking));
  HIP_CHECK(hipStreamCreateWithFlags(&sB, hipStreamNonBlocking));
  hipEvent_t evA, evB;
  HIP_CHECK(hipEventCreateWithFlags(&evA, hipEventDisableTiming));
  HIP_CHECK(hipEventCreateWithFlags(&evB, hipEventDisableTiming));
  if (w == 0) HIP_CHECK(hipStreamCreateWithFlags(&cb_stream, hipStreamNonBlocking));

  Cell<T>* cur = bufA;  // field read this step
  Cell<T>* nxt = bufB;  // field written this step
  for (int step = 0; step < STEPS; ++step) {
    // interior planes on A, the two boundary planes on B
    launch_step<T, St>(sA, cur, nxt, g, z0 + 1, z1 - 1, p);
    launch_step<T, St>(sB, cur, nxt, g, z0, std::min(z0 + 1, z1), p);
    if (z1 - 1 > z0) launch_step<T, St>(sB, cur, nxt, g, z1 - 1, z1, p);
    HIP_CHECK(hipEventRecord(evA, sA));
    HIP_CHECK(hipEventRecord(evB, sB));

    // mass reduction on the default stream, ordered after A and B via events
    HIP_CHECK(hipStreamWaitEvent(nullptr, evA, 0));
    HIP_CHECK(hipStreamWaitEvent(nullptr, evB, 0));
    if (z1 > z0) reduce_mass<T><<<64, RBLK, 0, nullptr>>>(nxt, g, z0, z1, diag + step);
    HIP_CHECK(hipGetLastError());
    HIP_CHECK(hipStreamSynchronize(nullptr));  // this thread's default stream (covers A and B)
    bar.arrive_and_wait();  // every slab of nxt is complete and nobody still reads cur

    std::swap(cur, nxt);
    if (w == 0) HIP_CHECK(hipLaunchHostFunc(cb_stream, on_step, &ctxs[step]));
  }
  if (w == 0) {
    HIP_CHECK(hipStreamSynchronize(cb_stream));
    HIP_CHECK(hipStreamDestroy(cb_stream));
  }
  HIP_CHECK(hipEventDestroy(evA));
  HIP_CHECK(hipEventDestroy(evB));
  HIP_CHECK(hipStreamDestroy(sA));
  HIP_CHECK(hipStreamDestroy(sB));
}

template <typename T, typename St>
static int run_sim(const char* tag, int workers, int ndev) {
  const Grid g{NX, NY, NZ};
  const size_t cells = (size_t)NX * NY * NZ;
  Managed<Cell<T>> bufA(cells), bufB(cells);
  Managed<Diag> diag(STEPS);
  init_field<T><<<(unsigned)((cells + 255) / 256), 256, 0, nullptr>>>(bufA.get(), g);
  HIP_CHECK(hipGetLastError());
  init_diag<<<1, 64, 0, nullptr>>>(diag.get(), STEPS);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipStreamSynchronize(nullptr));

  Params<T> p{T(0.16), T(0.08), T(0.035), T(0.065), T(0.5)};
  Barrier bar(workers);
  std::vector<StepCtx> ctxs(STEPS);
  for (int s = 0; s < STEPS; ++s) ctxs[s] = StepCtx{diag.get() + s, s, tag};

  std::vector<std::thread> th;
  for (int w = 0; w < workers; ++w)
    th.emplace_back(sim_worker<T, St>, w, w % ndev, workers, bufA.get(), bufB.get(), g, p, diag.get(),
                    std::ref(bar), std::ref(ctxs));
  for (auto& t : th) t.join();

  // after STEPS swaps the newest field is in A (STEPS even) or B (STEPS odd)
  const Cell<T>* fin = (STEPS % 2 == 0) ? bufA.get() : bufB.get();
  size_t bad = 0;
  for (size_t i = 0; i < cells; ++i) {
    Cell<T> c = fin[i];  // UM: R=2
    if (!std::isfinite((double)c.u) || !std::isfinite((double)c.v) || c.u < T(-1) || c.u > T(2)) ++bad;
  }
  double mu = static_cast<long long>(diag.get()[STEPS - 1].mass_u) * 1e-6;  // UM: R=1
  double mv = static_cast<long long>(diag.get()[STEPS - 1].mass_v) * 1e-6;  // UM: R=1
  std::printf("[%s] final sum(u)=%.3f sum(v)=%.3f  bad cells=%zu\n", tag, mu, mv, bad);
  return bad == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
  int ndev = 0;
  HIP_CHECK(hipGetDeviceCount(&ndev));
  if (ndev < 1) {
    std::fprintf(stderr, "no HIP device\n");
    return 1;
  }
  int workers = argc > 1 ? std::max(1, std::atoi(argv[1])) : std::max(2, ndev);
  workers = std::min(workers, NZ);
  std::printf("reaction_diffusion: %d device(s), %d worker thread(s), %dx%dx%d grid, %d steps\n", ndev,
              workers, NX, NY, NZ, STEPS);
  int rc = 0;
  rc |= run_sim<float, Stencil7<1>>("float ", workers, ndev);
  rc |= run_sim<double, Stencil7<2>>("double", workers, ndev);
  return rc;
}
