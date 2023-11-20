/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#include "benchmark.h"
#include <simd.h>
#include <mask_reductions.h>
#include <numeric>
#include <iostream>

template <typename T, typename Abi>
  std::ostream& operator<<(std::ostream& s, const std::basic_simd<T, Abi>& x)
  {
    s << '[' << x[0];
    for (int i = 1; i < x.size(); ++i)
      s << ", " << x[i];
    return s << ']';
  }

void
find_scalar(benchmark::State &state)
{
  const int N = state.range(0);
  std::vector<int> data(N);
  std::iota(data.rbegin(), data.rend(), 0);
  for (auto _ : state) {
    auto it = std::ranges::find(data, 0);
    if (std::distance(data.begin(), it) != N - 1)
      {
        std::cout << std::distance(data.begin(), it) << std::endl;
        std::abort();
      }
  }
  add_throughput_counters(state);
}

template <int ILP>
  void
  find_if_simd(benchmark::State &state)
  {
    const int N = state.range(0);
    using V = std::simd<int, std::simd<int>::size * ILP>;
    std::vector<V> data(N / V::size());
    V init([&](int x) { return N - 1 - x; });
    for (auto& x : data) {
      x = init;
      init -= init.size();
    }
    for (auto _ : state) {
      auto it = std::ranges::find_if(data, [](auto chunk) { return any_of(chunk == 0); });
      const int offset = std::distance(data.begin(), it) * V::size() + reduce_min_index(*it == 0);
      if (offset != N - 1)
        {
          std::cout << std::distance(data.begin(), it) << ' ' << *it << std::endl;
          std::abort();
        }
    }
    add_throughput_counters(state);
  }

void
chunk_view(benchmark::State& state)
{
  constexpr int ILP = 1;
  const int N = state.range(0);
  using V = std::simd<int, std::simd<int>::size * ILP>;
  std::vector<int> data(N);
  std::iota(data.rbegin(), data.rend(), 0);
  for (auto _ : state) {
    auto&& chunked = data | std::views::chunk(V::size());
    auto it = std::ranges::find_if(chunked, [](V chunk) {
                return any_of(chunk == 0);
              });
    const int offset = std::distance(chunked.begin(), it) * V::size()
                         + reduce_min_index(V(*it) == 0);
    if (offset != N - 1)
      {
        std::cout << std::distance(chunked.begin(), it) << " != " << N - 1 << std::endl;
        std::abort();
      }
  }
  add_throughput_counters(state);
}

void
simd_loads(benchmark::State& state)
{
  constexpr int ILP = 1;
  const int N = state.range(0);
  using V = std::simd<int, std::simd<int>::size * ILP>;
  std::vector<int> data(N);
  std::iota(data.rbegin(), data.rend(), 0);
  for (auto _ : state) {
    auto it = data.begin();
    auto end = data.end() - V::size();
    for (; it <= end; it += V::size())
      {
        V chunk(it);
        if (any_of(chunk == 0))
          break;
      }
    const int offset = std::distance(data.begin(), it) + reduce_min_index(V(it) == 0);
    if (offset != N - 1)
      {
        std::cout << std::distance(data.begin(), it) << " != " << N - 1 << std::endl;
        std::abort();
      }
  }
  add_throughput_counters(state);
}

static void
MyRange(benchmark::internal::Benchmark* b)
{
  for (long i = 1 << 10; i <= 1 << 20; i += i)
    b->Args({i});
}

// Register the function as a benchmark
BENCHMARK(simd_loads)->Apply(MyRange);
BENCHMARK(chunk_view)->Apply(MyRange);
BENCHMARK(find_if_simd<4>)->Apply(MyRange);
BENCHMARK(find_if_simd<1>)->Apply(MyRange);
BENCHMARK(find_scalar)->Apply(MyRange);
