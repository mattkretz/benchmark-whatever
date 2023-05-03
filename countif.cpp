/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */
#include <benchmark/benchmark.h>
#include <vir/simd.h>
#include <vir/simd_benchmarking.h>
#include <vir/simd_cvt.h>

#include <algorithm>
#include <boost/align/aligned_allocator.hpp>
#include <execution>
#include <ranges>
#include <vector>

#include "simd_for_each.h"

template <typename T>
using simd_aligned_vector =
    std::vector<T, boost::alignment::aligned_allocator<
                       T, stdx::memory_alignment_v<stdx::native_simd<T>>>>;

void add_columns(benchmark::State& state, std::size_t bytes_per_iteration) {
  state.counters["throughput"] = {double(bytes_per_iteration),
                                  benchmark::Counter::kIsIterationInvariantRate,
                                  benchmark::Counter::kIs1024};
}

void simple_count_if_O2(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  for (auto _ : state) {
    vir::fake_read(std::count_if(v.begin(), v.end(), [](float x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

[[gnu::optimize("-O3")]]
void simple_count_if_O3(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  for (auto _ : state) {
    vir::fake_read(std::count_if(v.begin(), v.end(), [](float x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

void unseq_O2(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  for (auto _ : state) {
    vir::fake_read(std::count_if(std::execution::unseq, v.begin(), v.end(),
                                 [](float x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

[[gnu::optimize("-O3")]]
void unseq_O3(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  for (auto _ : state) {
    vir::fake_read(std::count_if(std::execution::unseq, v.begin(), v.end(),
                                 [](float x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

void sorted(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  std::sort(v.begin(), v.end());
  for (auto _ : state) {
    vir::fake_read(std::count_if(v.begin(), v.end(), [](float x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

namespace vir
{
template <std::ranges::contiguous_range R>
[[gnu::noinline]] constexpr int count_if(auto pol, R const& v, auto&& fun)
{
  using T = std::ranges::range_value_t<R>;
  int count = 0;
  stdx::rebind_simd_t<int, stdx::native_simd<T>> countv = 0;
  for_each(pol, v, [&](auto... x) {
    if constexpr (sizeof...(x) == 1) {
      if constexpr ((x.size(), ...) == countv.size()) {
        ++where(vir::cvt(fun(x...)), countv);
      } else {
        count += popcount(fun(x...));
      }
    } else {
      ((++where(vir::cvt(fun(x)), countv)), ...);
    }
  });
  return count + reduce(countv);
}
}  // namespace vir

template <typename ExecutionPolicy>
void simd(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  for (auto _ : state) {
    vir::fake_read(vir::count_if(ExecutionPolicy{}, v, [](auto x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

template <typename ExecutionPolicy>
void simd_misaligned(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  simd_aligned_vector<float> v(n + 3);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  std::span misaligned(v.begin() + 3, v.end());
  for (auto _ : state) {
    vir::fake_read(
        vir::count_if(ExecutionPolicy{}, misaligned, [](auto x) { return x > 0; }));
  }
  add_columns(state, n * sizeof(float));
}

constexpr std::size_t smallest = 64;
constexpr auto largest = smallest << 19;

#define MYRANGE RangeMultiplier(2)->Range(smallest, largest)

BENCHMARK(simple_count_if_O2)->MYRANGE;
BENCHMARK(simple_count_if_O3)->MYRANGE;
BENCHMARK(unseq_O2)->MYRANGE;
BENCHMARK(unseq_O3)->MYRANGE;
BENCHMARK(sorted)->MYRANGE;
BENCHMARK(simd<decltype(execution::simd)>)->MYRANGE;
BENCHMARK(simd<decltype(execution::simd.unroll_by<4>())>)->MYRANGE;
BENCHMARK(simd<decltype(execution::simd.unroll_by<8>())>)->MYRANGE;
/*BENCHMARK(simd<decltype(execution::simd.prefer_aligned())>)->MYRANGE;
BENCHMARK(simd<decltype(execution::simd.prefer_aligned().unroll_by<4>())>)->MYRANGE;
BENCHMARK(simd<decltype(execution::simd.prefer_aligned().unroll_by<8>())>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd)>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd.unroll_by<4>())>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd.unroll_by<8>())>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd.prefer_aligned())>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd.prefer_aligned().unroll_by<4>())>)->MYRANGE;
BENCHMARK(simd_misaligned<decltype(execution::simd.prefer_aligned().unroll_by<8>())>)->MYRANGE;*/

BENCHMARK_MAIN();
