#include "benchmark.h"
#include <vir/simd.h>
#include <vir/simd_benchmarking.h>

namespace stdx = vir::stdx;

void peak(benchmark::State &state)
{
  constexpr int N = 8;
  using V = stdx::native_simd<float>;
  V x[N] = {};
  for (auto& v : x) {
    vir::fake_modify(v);
  }
  for (auto _ : state) {
    for (auto& v : x) {
      v = v * 3 + 1;
    }
  }
  for (auto& v : x) {
    vir::fake_read(v);
  }
  // compute FLOP/s and FLOP/cycle
  add_flop_counters(state, 2 * N * element_count<V>::value);
}

// Register the function as a benchmark
BENCHMARK(peak);
