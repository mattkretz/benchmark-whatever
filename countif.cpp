/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */
#include "benchmark.h"
#include <vir/simd.h>
#include <vir/simd_benchmarking.h>
#include <vir/simd_cvt.h>
#include <vir/simd_execution.h>

#include <algorithm>
#include <execution>
#include <memory_resource>
#include <ranges>
#include <vector>

//#include "simd_for_each.h"

namespace stdx = vir::stdx;

constexpr long smallest = 32;
constexpr long largest = smallest << 17;

alignas(4096) std::array<char, largest * sizeof(float) * 2> s_buffer;
std::pmr::monotonic_buffer_resource s_memory {s_buffer.data(), s_buffer.size()};

template <typename T = float>
void
add_columns(benchmark::State& state)
{
  const double bytes_per_iteration = state.range(0) * sizeof(T);
  state.counters["throughput / (Byte/s)"] = {double(bytes_per_iteration),
                                        benchmark::Counter::kIsIterationInvariantRate,
                                        benchmark::Counter::kIs1024};

  if (state.counters.contains("CYCLES")) {
    state.counters["throughput / (Bytes per cycle)"] = {
      double(bytes_per_iteration) / state.counters["CYCLES"],
      benchmark::Counter::kIsIterationInvariant
    };
  }

  if (state.counters.contains("INSTRUCTIONS")) {
    state.counters["asm efficiency / (instructions per value)"] = {
      double(state.range(0)) / state.counters["INSTRUCTIONS"],
      benchmark::Counter::kIsIterationInvariant | benchmark::Counter::kInvert
    };
  }
}

auto
make_data(std::size_t n)
{
  s_memory.release();
  std::pmr::vector<float> v(n, &s_memory);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  return v;
}

template <auto ExecutionPolicy>
  [[gnu::always_inline]]
  void
  do_benchmark(benchmark::State& state, const auto& v)
  {
    if (state.range(0) != v.size())
      std::abort();

    for (auto _ : state)
      {
        if constexpr (std::is_same_v<decltype(ExecutionPolicy), decltype(std::execution::seq)>)
          vir::fake_read(std::count_if(v.begin(), v.end(), [](auto x) { return x > 0; }));
        else
          vir::fake_read(std::count_if(ExecutionPolicy, v.begin(), v.end(),
                                       [](auto x) { return x > 0; }));
      }
    add_throughput_counters<float>(state);
  }

enum Variant
{
  Aligned,
  Sorted,
  Misaligned
};

template <auto pol, Variant var = Aligned>
  void
  count_if_O2(benchmark::State& state)
  {
    if constexpr (var == Misaligned)
      {
        auto v = make_data(state.range(0) + 1);
        std::span misaligned(v.begin() + 1, v.end());
        do_benchmark<pol>(state, misaligned);
      }
    else
      {
        auto v = make_data(state.range(0));
        if constexpr (var == Sorted)
          std::sort(v.begin(), v.end());
        do_benchmark<pol>(state, v);
      }
  }

template <auto pol, Variant var = Aligned>
  [[gnu::optimize("-O3"),gnu::flatten]]
  void
  count_if_O3(benchmark::State& state)
  { count_if_O2<pol, var>(state); }

static void
MyRange(benchmark::internal::Benchmark* b)
{
  for (long i = smallest; i <= largest; i += i)
    b->Args({i - 1});
  for (long i = smallest; i <= largest; i += i)
    b->Args({i});
}

BENCHMARK(count_if_O2<std::execution::seq>)->Apply(MyRange);
BENCHMARK(count_if_O3<std::execution::seq>)->Apply(MyRange);
BENCHMARK(count_if_O2<std::execution::unseq>)->Apply(MyRange);
BENCHMARK(count_if_O3<std::execution::unseq>)->Apply(MyRange);
BENCHMARK(count_if_O2<std::execution::seq, Sorted>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.unroll_by<4>()>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.unroll_by<8>()>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.unroll_by<4>(), Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned().unroll_by<4>(), Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.auto_prologue().unroll_by<4>(), Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned()>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned().unroll_by<4>()>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned().unroll_by<8>()>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd, Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.unroll_by<8>(), Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned(), Misaligned>)->Apply(MyRange);
BENCHMARK(count_if_O2<vir::execution::simd.prefer_aligned().unroll_by<8>(), Misaligned>)->Apply(MyRange);
