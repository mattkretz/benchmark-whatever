#include "benchmark.h"
#include <vir/simd.h>
#include <vir/simd_benchmarking.h>

namespace stdx = vir::stdx;

void peak(benchmark::State &state)
{
  constexpr int N = 8;
  using V = stdx::native_simd<float>;
  V x[N] = {};
  for (int i = 0; i < N; ++i) {
    vir::fake_modify(x[i]);
  }
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      vir::fake_read(x[i] = x[i] * 3 + 1);
    }
  }

  // compute FLOP/s and FLOP/cycle
  add_flop_counters(state, 2 * N * element_count<V>::value);
}

// Register the function as a benchmark
BENCHMARK(peak);
