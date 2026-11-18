// =============================================================================
// transformer.hip -- one transformer encoder layer (AI-like workload)
//
//   LN -> per-head Q/K/V projections -> QK^T -> softmax -> PV -> Wo (+residual)
//      -> LN -> W1 (+GELU) -> W2 (+residual) -> checksum
//
// Work delegation
//   * batch elements are dealt round-robin to host worker threads; worker w runs
//     on device (w % ndev), so one GPU can still exercise several host threads
//   * inside a worker: the DEFAULT stream (legacy or per-thread, whichever the
//     build selects) runs LN / output / MLP; one non-blocking stream per head runs
//     the attention math; a tail stream fires a host callback. Ordering between
//     streams is expressed only with events (no host-side device sync).
//   * weights/activations are hipMallocManaged. Only the per-head S x S score
//     matrix lives in plain hipMalloc scratch (NOT unified memory).
//   * atomics in this app are agent-scope (plain atomicAdd).
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
#include <cmath>
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

// ----------------------------------------------------------------------------
// configuration
// ----------------------------------------------------------------------------
constexpr int SEQ = 64;
constexpr int DMODEL = 128;
constexpr int HEADS = 4;
constexpr int DHEAD = DMODEL / HEADS;
constexpr int DFF = 256;
constexpr int TILE = 16;
constexpr int BLOCK = 128;  // block size for LN / softmax / checksum (power of two)

// ----------------------------------------------------------------------------
// small utilities
// ----------------------------------------------------------------------------
template <typename T>
class Managed {  // RAII hipMallocManaged buffer
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

__host__ __device__ inline uint64_t mix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}
__host__ __device__ inline float uniform01(uint64_t seed, uint64_t i) {
  return (float)(mix64(seed ^ (i * 0x2545F4914F6CDD1Dull)) >> 40) * (1.0f / 16777216.0f);
}
__host__ __device__ inline unsigned long long to_fixed(double v) {  // two's-complement fixed point
  return (unsigned long long)(long long)llround(v * 1024.0);
}

// 2-D strided view. The pointee may be UM, hipMalloc scratch, or anything else.
template <typename T>
struct Mat {
  T* p;
  int rows, cols, rs, cs;
  __host__ __device__ T& operator()(int r, int c) const {
    return p[(size_t)r * rs + (size_t)c * cs];
  }
};
template <typename T>
__host__ __device__ Mat<T> make_mat(T* p, int rows, int cols, int rs, int cs) {
  return Mat<T>{p, rows, cols, rs, cs};
}

template <typename T>
struct MeanVar {
  T mean, rstd;
};
struct Stats {
  unsigned long long checksum;
  unsigned long long entropy;
  unsigned int done;
};

// ----------------------------------------------------------------------------
// device helpers
// ----------------------------------------------------------------------------
__device__ inline float block_sum(float v, float* red) {
  red[threadIdx.x] = v;
  __syncthreads();
  for (int o = blockDim.x / 2; o > 0; o >>= 1) {
    if (threadIdx.x < (unsigned)o) red[threadIdx.x] += red[threadIdx.x + o];
    __syncthreads();
  }
  float r = red[0];
  __syncthreads();
  return r;
}
__device__ inline float block_max(float v, float* red) {
  red[threadIdx.x] = v;
  __syncthreads();
  for (int o = blockDim.x / 2; o > 0; o >>= 1) {
    if (threadIdx.x < (unsigned)o) red[threadIdx.x] = fmaxf(red[threadIdx.x], red[threadIdx.x + o]);
    __syncthreads();
  }
  float r = red[0];
  __syncthreads();
  return r;
}

// GEMM epilogues (functors, templated into the GEMM kernel)
struct EpiIdentity {
  __device__ float operator()(float a, int, int) const { return a; }
};
struct EpiScale {
  float s;
  __device__ float operator()(float a, int, int) const { return a * s; }
};
struct EpiGelu {
  __device__ float operator()(float a, int, int) const {
    return 0.5f * a * (1.f + tanhf(0.7978845608f * (a + 0.044715f * a * a * a)));
  }
};
struct EpiResidual {
  Mat<float> res;
  __device__ float operator()(float a, int r, int c) const {
    return a + res(r, c);  // UM: R=1
  }
};

// ----------------------------------------------------------------------------
// kernels
// ----------------------------------------------------------------------------
__global__ void init_uniform(float* p, size_t n, uint64_t seed, float offset, float scale) {
  size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x;
  if (i < n) p[i] = offset + scale * (2.f * uniform01(seed, i) - 1.f);  // UM: W=1
}

__global__ void init_stats(Stats* s, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) s[i] = Stats{0ull, 0ull, 0u};  // UM: W=3
}

// one block per row
__global__ void layernorm_kernel(Mat<float> x, Mat<float> y, const float* gamma, const float* beta,
                                 MeanVar<float>* mv, int D) {
  __shared__ float red[BLOCK];
  const int r = blockIdx.x;
  float s = 0.f, ss = 0.f;
  for (int c = threadIdx.x; c < D; c += blockDim.x) {
    float v = x(r, c);  // UM: R=1
    s += v;
    ss += v * v;
  }
  s = block_sum(s, red);
  ss = block_sum(ss, red);
  const float mean = s / D;
  const float rstd = rsqrtf(ss / D - mean * mean + 1e-5f);
  if (threadIdx.x == 0) mv[r] = MeanVar<float>{mean, rstd};  // UM: W=2
  for (int c = threadIdx.x; c < D; c += blockDim.x) {
    float v = x(r, c);                                    // UM: R=1
    y(r, c) = (v - mean) * rstd * gamma[c] + beta[c];     // UM: R=2 W=1
  }
}

// C = epi(A * B). A, B, C are strided views; which memory they live in depends on the call site:
//   QK^T : A=UM(Q)      B=UM(K^T)  C=scratch(scores)
//   PV   : A=scratch(P) B=UM(V)    C=UM(ctx)
//   proj : A, B, C all UM
template <int T, typename Epi>
__global__ void gemm_kernel(Mat<float> A, Mat<float> B, Mat<float> C, int M, int N, int K, Epi epi) {
  __shared__ float As[T][T];
  __shared__ float Bs[T][T];
  const int row = blockIdx.y * T + threadIdx.y;
  const int col = blockIdx.x * T + threadIdx.x;
  float acc = 0.f;
  for (int k0 = 0; k0 < K; k0 += T) {
    const int ak = k0 + threadIdx.x;
    const int bk = k0 + threadIdx.y;
    As[threadIdx.y][threadIdx.x] = (row < M && ak < K) ? A(row, ak) : 0.f;  // UM: R=1 (sometimes: R)
    Bs[threadIdx.y][threadIdx.x] = (bk < K && col < N) ? B(bk, col) : 0.f;  // UM: R=1
    __syncthreads();
    for (int k = 0; k < T; ++k) acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];
    __syncthreads();
  }
  if (row < M && col < N) C(row, col) = epi(acc, row, col);  // UM: W=1 (sometimes: W)
}

// in-place row softmax on hipMalloc scratch (never UM); entropy goes to UM via atomic
__global__ void softmax_rows(Mat<float> s, int n, unsigned long long* entropy_acc) {
  __shared__ float red[BLOCK];
  const int r = blockIdx.x;
  float m = -3.0e38f;
  for (int c = threadIdx.x; c < n; c += blockDim.x) m = fmaxf(m, s(r, c));
  m = block_max(m, red);
  float sum = 0.f;
  for (int c = threadIdx.x; c < n; c += blockDim.x) {
    float e = __expf(s(r, c) - m);
    s(r, c) = e;
    sum += e;
  }
  sum = block_sum(sum, red);
  float h = 0.f;
  for (int c = threadIdx.x; c < n; c += blockDim.x) {
    float p = s(r, c) / sum;
    s(r, c) = p;
    h -= p * __logf(p + 1e-20f);
  }
  h = block_sum(h, red);
  if (threadIdx.x == 0) atomicAdd(entropy_acc, to_fixed(h));  // UM: A=1
}

template <typename T>
__global__ void checksum_kernel(const T* p, size_t n, unsigned long long* acc) {
  __shared__ float red[BLOCK];
  size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x;
  float v = 0.f;
  if (i < n) v = static_cast<float>(p[i]);  // UM: R=1
  float s = block_sum(v, red);
  if (threadIdx.x == 0) atomicAdd(acc, to_fixed(s));  // UM: A=1
}

// ----------------------------------------------------------------------------
// host side
// ----------------------------------------------------------------------------
struct Weights {
  Managed<float> wq, wk, wv, wo, w1, w2, g1, b1, g2, b2;
  Weights()
      : wq((size_t)HEADS * DMODEL * DHEAD), wk((size_t)HEADS * DMODEL * DHEAD),
        wv((size_t)HEADS * DMODEL * DHEAD), wo((size_t)DMODEL * DMODEL), w1((size_t)DMODEL * DFF),
        w2((size_t)DFF * DMODEL), g1(DMODEL), b1(DMODEL), g2(DMODEL), b2(DMODEL) {}
};
struct Acts {
  Managed<float> x, xn, qkv, ctx, y1, y1n, hid, out;
  Managed<MeanVar<float>> mv1, mv2;
  Managed<Stats> stats;
  explicit Acts(int B)
      : x((size_t)B * SEQ * DMODEL), xn((size_t)B * SEQ * DMODEL),
        qkv((size_t)B * 3 * HEADS * SEQ * DHEAD), ctx((size_t)B * SEQ * DMODEL),
        y1((size_t)B * SEQ * DMODEL), y1n((size_t)B * SEQ * DMODEL), hid((size_t)B * SEQ * DFF),
        out((size_t)B * SEQ * DMODEL), mv1((size_t)B * SEQ), mv2((size_t)B * SEQ), stats(B) {}
};

struct CbCtx {
  Stats* st;
  MeanVar<float>* mv;
  int batch;
};
static std::mutex g_print_mu;

// host callback: runs on a driver thread, touches UM but makes no HIP calls
static void on_batch_done(void* arg) {
  auto* c = static_cast<CbCtx*>(arg);
  long long cs = static_cast<long long>(c->st->checksum);   // UM: R=1
  long long ent = static_cast<long long>(c->st->entropy);   // UM: R=1
  float mean0 = c->mv[0].mean;                              // UM: R=1
  __atomic_fetch_add(&c->st->done, 1u, __ATOMIC_RELAXED);   // UM: A=1
  std::lock_guard<std::mutex> lk(g_print_mu);
  std::printf("  [callback] batch %d checksum=%.3f entropy=%.3f ln2.mean[0]=%.4f\n", c->batch,
              cs / 1024.0, ent / 1024.0, mean0);
}

static void launch_init(float* p, size_t n, uint64_t seed, float offset, float scale) {
  init_uniform<<<(unsigned)((n + 255) / 256), 256, 0, nullptr>>>(p, n, seed, offset, scale);
  HIP_CHECK(hipGetLastError());
}

template <typename Epi>
static void launch_gemm(hipStream_t s, Mat<float> A, Mat<float> B, Mat<float> C, int M, int N, int K,
                        Epi epi) {
  dim3 block(TILE, TILE);
  dim3 grid((N + TILE - 1) / TILE, (M + TILE - 1) / TILE);
  gemm_kernel<TILE><<<grid, block, 0, s>>>(A, B, C, M, N, K, epi);
  HIP_CHECK(hipGetLastError());
}

struct Worker {
  int id = 0, dev = 0;
  hipStream_t heads[HEADS];
  hipStream_t tail = nullptr;
  hipEvent_t ev_ln, ev_head[HEADS], ev_main;
  float* scratch[HEADS];  // hipMalloc'd (NOT unified) score matrices
};

static void run_batch(Worker& w, const Weights& W, Acts& A, CbCtx* cb, int b) {
  const hipStream_t main_s = nullptr;  // default stream: legacy or per-thread depending on build
  const size_t SD = (size_t)SEQ * DMODEL;
  Mat<float> x = make_mat(A.x.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  Mat<float> xn = make_mat(A.xn.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  Mat<float> ctx = make_mat(A.ctx.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  Mat<float> y1 = make_mat(A.y1.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  Mat<float> y1n = make_mat(A.y1n.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  Mat<float> hid = make_mat(A.hid.get() + (size_t)b * SEQ * DFF, SEQ, DFF, DFF, 1);
  Mat<float> out = make_mat(A.out.get() + b * SD, SEQ, DMODEL, DMODEL, 1);
  float* qkv = A.qkv.get() + (size_t)b * 3 * HEADS * SEQ * DHEAD;
  Stats* st = A.stats.get() + b;

  layernorm_kernel<<<SEQ, BLOCK, 0, main_s>>>(x, xn, W.g1.get(), W.b1.get(), A.mv1.get() + b * SEQ, DMODEL);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipEventRecord(w.ev_ln, main_s));

  const float scale = 1.f / std::sqrt((float)DHEAD);
  for (int h = 0; h < HEADS; ++h) {
    hipStream_t s = w.heads[h];
    HIP_CHECK(hipStreamWaitEvent(s, w.ev_ln, 0));
    auto head_part = [&](int t) {
      return make_mat(qkv + ((size_t)t * HEADS + h) * SEQ * DHEAD, SEQ, DHEAD, DHEAD, 1);
    };
    Mat<float> Q = head_part(0), K = head_part(1), V = head_part(2);
    const size_t wo = (size_t)h * DMODEL * DHEAD;
    launch_gemm(s, xn, make_mat(W.wq.get() + wo, DMODEL, DHEAD, DHEAD, 1), Q, SEQ, DHEAD, DMODEL, EpiIdentity{});
    launch_gemm(s, xn, make_mat(W.wk.get() + wo, DMODEL, DHEAD, DHEAD, 1), K, SEQ, DHEAD, DMODEL, EpiIdentity{});
    launch_gemm(s, xn, make_mat(W.wv.get() + wo, DMODEL, DHEAD, DHEAD, 1), V, SEQ, DHEAD, DMODEL, EpiIdentity{});
    Mat<float> Kt = make_mat(K.p, DHEAD, SEQ, 1, DHEAD);              // transposed view, same UM
    Mat<float> sc = make_mat(w.scratch[h], SEQ, SEQ, SEQ, 1);          // scratch, not UM
    launch_gemm(s, Q, Kt, sc, SEQ, SEQ, DHEAD, EpiScale{scale});
    softmax_rows<<<SEQ, BLOCK, 0, s>>>(sc, SEQ, &st->entropy);
    HIP_CHECK(hipGetLastError());
    Mat<float> ctx_h = make_mat(ctx.p + h * DHEAD, SEQ, DHEAD, DMODEL, 1);
    launch_gemm(s, sc, V, ctx_h, SEQ, DHEAD, SEQ, EpiIdentity{});
    HIP_CHECK(hipEventRecord(w.ev_head[h], s));
  }
  for (int h = 0; h < HEADS; ++h) HIP_CHECK(hipStreamWaitEvent(main_s, w.ev_head[h], 0));

  launch_gemm(main_s, ctx, make_mat(W.wo.get(), DMODEL, DMODEL, DMODEL, 1), y1, SEQ, DMODEL, DMODEL, EpiResidual{x});
  layernorm_kernel<<<SEQ, BLOCK, 0, main_s>>>(y1, y1n, W.g2.get(), W.b2.get(), A.mv2.get() + b * SEQ, DMODEL);
  HIP_CHECK(hipGetLastError());
  launch_gemm(main_s, y1n, make_mat(W.w1.get(), DMODEL, DFF, DFF, 1), hid, SEQ, DFF, DMODEL, EpiGelu{});
  launch_gemm(main_s, hid, make_mat(W.w2.get(), DFF, DMODEL, DMODEL, 1), out, SEQ, DMODEL, DFF, EpiResidual{y1});
  checksum_kernel<float><<<(unsigned)((SD + BLOCK - 1) / BLOCK), BLOCK, 0, main_s>>>(out.p, SD, &st->checksum);
  HIP_CHECK(hipGetLastError());

  HIP_CHECK(hipEventRecord(w.ev_main, main_s));
  HIP_CHECK(hipStreamWaitEvent(w.tail, w.ev_main, 0));
  HIP_CHECK(hipLaunchHostFunc(w.tail, on_batch_done, cb));
}

static void worker_main(int id, int dev, int nworkers, int B, const Weights& W, Acts& A,
                        std::vector<CbCtx>& cbs) {
  HIP_CHECK(hipSetDevice(dev));
  Worker w;
  w.id = id;
  w.dev = dev;
  for (int h = 0; h < HEADS; ++h) {
    HIP_CHECK(hipStreamCreateWithFlags(&w.heads[h], hipStreamNonBlocking));
    HIP_CHECK(hipEventCreateWithFlags(&w.ev_head[h], hipEventDisableTiming));
    HIP_CHECK(hipMalloc(reinterpret_cast<void**>(&w.scratch[h]), (size_t)SEQ * SEQ * sizeof(float)));
  }
  HIP_CHECK(hipStreamCreateWithFlags(&w.tail, hipStreamNonBlocking));
  HIP_CHECK(hipEventCreateWithFlags(&w.ev_ln, hipEventDisableTiming));
  HIP_CHECK(hipEventCreateWithFlags(&w.ev_main, hipEventDisableTiming));

  for (int b = id; b < B; b += nworkers) run_batch(w, W, A, &cbs[b], b);

  HIP_CHECK(hipStreamSynchronize(w.tail));  // also guarantees callbacks have run
  for (int h = 0; h < HEADS; ++h) HIP_CHECK(hipStreamSynchronize(w.heads[h]));
  HIP_CHECK(hipStreamSynchronize(nullptr));  // this thread's default stream

  for (int h = 0; h < HEADS; ++h) {
    HIP_CHECK(hipStreamDestroy(w.heads[h]));
    HIP_CHECK(hipEventDestroy(w.ev_head[h]));
    HIP_CHECK(hipFree(w.scratch[h]));
  }
  HIP_CHECK(hipStreamDestroy(w.tail));
  HIP_CHECK(hipEventDestroy(w.ev_ln));
  HIP_CHECK(hipEventDestroy(w.ev_main));
}

int main(int argc, char** argv) {
  int ndev = 0;
  HIP_CHECK(hipGetDeviceCount(&ndev));
  if (ndev < 1) {
    std::fprintf(stderr, "no HIP device\n");
    return 1;
  }
  const int nworkers = argc > 1 ? std::max(1, std::atoi(argv[1])) : std::max(2, ndev);
  const int B = std::max(4, 2 * nworkers);
  std::printf("transformer: %d device(s), %d worker thread(s), %d batch elements\n", ndev, nworkers, B);

  Weights W;
  Acts A(B);
  // initialisation on the default stream of the main thread
  launch_init(A.x.get(), A.x.size(), 1, 0.f, 1.0f);
  launch_init(W.wq.get(), W.wq.size(), 2, 0.f, 0.1f);
  launch_init(W.wk.get(), W.wk.size(), 3, 0.f, 0.1f);
  launch_init(W.wv.get(), W.wv.size(), 4, 0.f, 0.1f);
  launch_init(W.wo.get(), W.wo.size(), 5, 0.f, 0.1f);
  launch_init(W.w1.get(), W.w1.size(), 6, 0.f, 0.1f);
  launch_init(W.w2.get(), W.w2.size(), 7, 0.f, 0.1f);
  launch_init(W.g1.get(), W.g1.size(), 8, 1.f, 0.1f);
  launch_init(W.g2.get(), W.g2.size(), 9, 1.f, 0.1f);
  launch_init(W.b1.get(), W.b1.size(), 10, 0.f, 0.1f);
  launch_init(W.b2.get(), W.b2.size(), 11, 0.f, 0.1f);
  init_stats<<<(unsigned)((B + 63) / 64), 64, 0, nullptr>>>(A.stats.get(), B);
  HIP_CHECK(hipGetLastError());
  HIP_CHECK(hipStreamSynchronize(nullptr));

  std::vector<CbCtx> cbs(B);
  for (int b = 0; b < B; ++b) cbs[b] = CbCtx{A.stats.get() + b, A.mv2.get() + (size_t)b * SEQ, b};

  std::vector<std::thread> threads;
  for (int w = 0; w < nworkers; ++w)
    threads.emplace_back(worker_main, w, w % ndev, nworkers, B, std::cref(W), std::ref(A), std::ref(cbs));
  for (auto& t : threads) t.join();

  // host-side verification (reads UM written by the GPUs)
  size_t bad = 0;
  const float* out = A.out.get();
  for (size_t i = 0; i < A.out.size(); ++i) {
    float v = out[i];  // UM: R=1
    if (!std::isfinite(v)) ++bad;
  }
  long long total = 0;
  unsigned done = 0;
  for (int b = 0; b < B; ++b) {
    total += static_cast<long long>(A.stats.get()[b].checksum);  // UM: R=1
    done += A.stats.get()[b].done;                               // UM: R=1
  }
  std::printf("total checksum=%.3f (deterministic), callbacks=%u/%d, non-finite=%zu\n", total / 1024.0,
              done, B, bad);
  return (bad == 0 && done == (unsigned)B) ? 0 : 1;
}
