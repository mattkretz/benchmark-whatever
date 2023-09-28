#include "benchmark.h"
#include <../simd-prototyping/simd.h>
#include <vir/simd_benchmarking.h>

namespace stdx = vir::stdx;

using V = std::simd<float, std::simd<float>::size() * 8>;

void do_one(V& v)
{
  v = v * 3.f + 1.f;
}


void peak(benchmark::State &state)
{
  constexpr int N = 1;
  V x = {};
  asm("" : "+m"(x));
  for (auto _ : state) {
    x = x * 3.f + 1.f;
  }
  asm("" :: "m"(x));
  // compute FLOP/s and FLOP/cycle
  add_flop_counters(state, 2 * N * element_count<V>::value);
}

// Register the function as a benchmark
BENCHMARK(peak);
