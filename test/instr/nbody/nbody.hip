// =============================================================================
// nbody.hip -- gravitational N-body, symplectic Euler (scientific workload)
//
// Work delegation
//   * W host worker threads, worker w on device (w % ndev); each owns a slice of bodies
//   * force phase: the slice is split in NCHUNK pieces, each on its own non-blocking stream;
//     every GPU reads ALL bodies' positions straight out of unified memory (no copies)
//   * integrate phase: runs on the DEFAULT stream (legacy or per-thread, per build flag)
//   * phases are separated by host stream syncs + a thread barrier (no device-wide sync)
//   * a host callback (hipLaunchHostFunc) reports per-step diagnostics
//   * run twice from one template: <float, Softening<10>> and <double, Softening<40>>
//   * atomics are system-scope (atomicAdd_system) because several devices update one location
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
//   * Body<T> has 10 leaf scalars: pos(3) mass(1) vel(3) acc(3).
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

constexpr int TILE = 128;   // force-kernel block size == LDS tile size
constexpr int NCHUNK = 2;   // streams per worker in the force phase

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
__host__ __device__ inline float uniform01(uint64_t seed, uint64_t i) {
  return (float)(mix64(seed ^ (i * 0x2545F4914F6CDD1Dull)) >> 40) * (1.0f / 16777216.0f);
}
template <typename T>
__host__ __device__ inline unsigned long long to_fixed(T v) {  // two's-complement fixed point, 1e-6 units
  return (unsigned long long)(long long)llround((double)v * 1.0e6);
}

// ----------------------------------------------------------------------------
// data types
// ----------------------------------------------------------------------------
template <typename T>
struct Vec3 {
  T x, y, z;
  __host__ __device__ Vec3& operator+=(const Vec3& o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
};
template <typename T>
__host__ __device__ inline Vec3<T> operator*(Vec3<T> a, T s) {
  return Vec3<T>{a.x * s, a.y * s, a.z * s};
}
template <typename T>
__host__ __device__ inline T dot(const Vec3<T>& a, const Vec3<T>& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
template <typename T>
struct Vec4 {
  T x, y, z, w;
};
template <typename T>
struct Body {
  Vec3<T> pos;
  T mass;
  Vec3<T> vel;
  Vec3<T> acc;
};
struct Diag {  // per-step diagnostics, fixed-point so atomic sums are order independent
  unsigned long long ke, px, py, pz;
};

// softening policy (template parameter of the force kernel)
template <int Milli>
struct Softening {
  template <typename T>
  __device__ T operator()(T r2) const {
    T s = r2 + T(Milli) * T(1e-3);
    T inv = T(1) / sqrt(s);
    return inv * inv * inv;
  }
};

// ----------------------------------------------------------------------------
// kernels
// ----------------------------------------------------------------------------
template <typename T>
__global__ void init_bodies(Body<T>* b, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  Body<T> o;
  o.pos = Vec3<T>{T(20) * T(uniform01(11, i) - 0.5f), T(20) * T(uniform01(12, i) - 0.5f),
                  T(20) * T(uniform01(13, i) - 0.5f)};
  o.mass = T(0.5f + uniform01(14, i)) * T(1000) / T(n);
  o.vel = Vec3<T>{T(0.1f * (uniform01(15, i) - 0.5f)), T(0.1f * (uniform01(16, i) - 0.5f)),
                  T(0.1f * (uniform01(17, i) - 0.5f))};
  o.acc = Vec3<T>{T(0), T(0), T(0)};
  b[i] = o;  // UM: W=10
}

__global__ void init_diag(Diag* d, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) d[i] = Diag{0ull, 0ull, 0ull, 0ull};  // UM: W=4
}

// acc[i] for i in [begin,end); tiles all N bodies through LDS
template <typename T, typename Soft>
__global__ void force_kernel(Body<T>* bodies, int n, int begin, int end, Soft soft) {
  __shared__ Vec4<T> tile[TILE];
  const int i = begin + blockIdx.x * blockDim.x + threadIdx.x;
  const bool active = i < end;
  Vec3<T> pi{T(0), T(0), T(0)};
  if (active) pi = bodies[i].pos;  // UM: R=3
  Vec3<T> a{T(0), T(0), T(0)};
  for (int base = 0; base < n; base += TILE) {
    const int j = base + threadIdx.x;
    if (j < n) {
      tile[threadIdx.x] = Vec4<T>{bodies[j].pos.x, bodies[j].pos.y, bodies[j].pos.z, bodies[j].mass};  // UM: R=4
    } else {
      tile[threadIdx.x] = Vec4<T>{T(0), T(0), T(0), T(0)};
    }
    __syncthreads();
    if (active) {
      for (int k = 0; k < TILE; ++k) {
        Vec4<T> q = tile[k];
        Vec3<T> d{q.x - pi.x, q.y - pi.y, q.z - pi.z};
        T w = q.w * soft(dot(d, d));
        a += d * w;
      }
    }
    __syncthreads();
  }
  if (active) bodies[i].acc = a;  // UM: W=3
}

template <typename T>
__global__ void integrate_kernel(Body<T>* bodies, int begin, int end, T dt, Diag* d) {
  const int i = begin + blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= end) return;
  Vec3<T> v = bodies[i].vel;  // UM: R=3
  Vec3<T> a = bodies[i].acc;  // UM: R=3
  Vec3<T> p = bodies[i].pos;  // UM: R=3
  T m = bodies[i].mass;       // UM: R=1
  v += a * dt;
  p += v * dt;
  bodies[i].vel = v;  // UM: W=3
  bodies[i].pos = p;  // UM: W=3
  atomicAdd_system(&d->ke, to_fixed(T(0.5) * m * dot(v, v)));  // UM: A=1
  atomicAdd_system(&d->px, to_fixed(m * v.x));                 // UM: A=1
  atomicAdd_system(&d->py, to_fixed(m * v.y));                 // UM: A=1
  atomicAdd_system(&d->pz, to_fixed(m * v.z));                 // UM: A=1
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
  double ke = static_cast<long long>(c->d->ke) * 1e-6;  // UM: R=1
  long long px = static_cast<long long>(c->d->px);      // UM: R=1
  long long py = static_cast<long long>(c->d->py);      // UM: R=1
  long long pz = static_cast<long long>(c->d->pz);      // UM: R=1
  std::lock_guard<std::mutex> lk(g_print_mu);
  std::printf("  [%s] step %2d  KE=%.6f  |P|=%.6f\n", c->tag, c->step, ke,
              std::sqrt((double)px * px + (double)py * py + (double)pz * pz) * 1e-6);
}

template <typename T, typename Soft>
static void sim_worker(int w, int dev, int nworkers, Body<T>* bodies, int N, int steps, Diag* diag,
                       Barrier& bar, std::vector<StepCtx>& ctxs) {
  HIP_CHECK(hipSetDevice(dev));
  const int begin = (int)((long long)N * w / nworkers);
  const int end = (int)((long long)N * (w + 1) / nworkers);
  hipStream_t chunk[NCHUNK];
  for (auto& s : chunk) HIP_CHECK(hipStreamCreateWithFlags(&s, hipStreamNonBlocking));
  hipStream_t cb_stream = nullptr;
  if (w == 0) HIP_CHECK(hipStreamCreateWithFlags(&cb_stream, hipStreamNonBlocking));

  const T dt = T(0.01);
  for (int step = 0; step < steps; ++step) {
    // ---- phase A: forces (reads every body's pos, writes own slice's acc) ----
    for (int c = 0; c < NCHUNK; ++c) {
      const int cb = begin + (int)((long long)(end - begin) * c / NCHUNK);
      const int ce = begin + (int)((long long)(end - begin) * (c + 1) / NCHUNK);
      if (ce <= cb) continue;
      force_kernel<T, Soft><<<(ce - cb + TILE - 1) / TILE, TILE, 0, chunk[c]>>>(bodies, N, cb, ce, Soft{});
      HIP_CHECK(hipGetLastError());
    }
    for (auto& s : chunk) HIP_CHECK(hipStreamSynchronize(s));
    bar.arrive_and_wait();  // nobody may move positions until all forces are done

    // ---- phase B: integrate on the default stream ----
    if (end > begin) {
      integrate_kernel<T><<<(end - begin + 127) / 128, 128, 0, nullptr>>>(bodies, begin, end, dt, diag + step);
      HIP_CHECK(hipGetLastError());
    }
    HIP_CHECK(hipStreamSynchronize(nullptr));  // this thread's default stream
    bar.arrive_and_wait();  // positions final, diag[step] complete

    if (w == 0) HIP_CHECK(hipLaunchHostFunc(cb_stream, on_step, &ctxs[step]));
  }
  if (w == 0) {
    HIP_CHECK(hipStreamSynchronize(cb_stream));
    HIP_CHECK(hipStreamDestroy(cb_stream));
  }
  for (auto& s : chunk) HIP_CHECK(hipStreamDestroy(s));
}

template <typename T, typename Soft>
static int run_sim(const char* tag, int workers, int ndev, int N, int steps) {
  Managed<Body<T>> bodies(N);
  Managed<Diag> diag(steps);
  init_bodies<T><<<(N + 127) / 128, 128, 0, nullptr>>>(bodies.get(), N);
  HIP_CHECK(hipGetLastError());
  init_diag<<<(steps + 63) / 64, 64, 0, nullptr>>>(diag.get(), steps);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipStreamSynchronize(nullptr));

  Barrier bar(workers);
  std::vector<StepCtx> ctxs(steps);
  for (int s = 0; s < steps; ++s) ctxs[s] = StepCtx{diag.get() + s, s, tag};

  std::vector<std::thread> th;
  for (int w = 0; w < workers; ++w)
    th.emplace_back(sim_worker<T, Soft>, w, w % ndev, workers, bodies.get(), N, steps, diag.get(),
                    std::ref(bar), std::ref(ctxs));
  for (auto& t : th) t.join();

  // host verification pass over UM
  size_t bad = 0;
  for (int i = 0; i < N; ++i) {
    Vec3<T> p = bodies.get()[i].pos;  // UM: R=3
    if (!std::isfinite((double)p.x) || !std::isfinite((double)p.y) || !std::isfinite((double)p.z)) ++bad;
  }
  double ke0 = static_cast<long long>(diag.get()[0].ke) * 1e-6;          // UM: R=1
  double ke1 = static_cast<long long>(diag.get()[steps - 1].ke) * 1e-6;  // UM: R=1
  std::printf("[%s] KE first=%.6f last=%.6f  non-finite positions=%zu\n", tag, ke0, ke1, bad);
  return bad == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
  int ndev = 0;
  HIP_CHECK(hipGetDeviceCount(&ndev));
  if (ndev < 1) {
    std::fprintf(stderr, "no HIP device\n");
    return 1;
  }
  const int workers = argc > 1 ? std::max(1, std::atoi(argv[1])) : std::max(2, ndev);
  const int N = 4096, steps = 10;
  std::printf("nbody: %d device(s), %d worker thread(s), N=%d, steps=%d\n", ndev, workers, N, steps);
  int rc = 0;
  rc |= run_sim<float, Softening<10>>("float ", workers, ndev, N, steps);
  rc |= run_sim<double, Softening<40>>("double", workers, ndev, N, steps);
  return rc;
}
