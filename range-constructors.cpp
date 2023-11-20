/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#include "benchmark.h"
#include <simd.h>

void
iterator_ctor(benchmark::State &state)
{
  using V = std::simd<float>;
  std::vector<float> data(state.range(0), 1.f);
  for (auto _ : state) {
    auto it = data.begin();
    auto end = data.end() - V::size();
    for (; it <= end; it += V::size())
      {
        V x = V(it) + 0.1f;
        x.copy_to(it);
      }
    for (; it < data.end(); ++it)
      {
        *it += 0.1f;
      }
  }
  add_throughput_counters(state);
}

void
naive_range_ctor(benchmark::State &state)
{
  using V = std::simd<float>;
  std::vector<float> data(state.range(0), 1.f);
  for (auto _ : state) {
    const auto end = data.end();
    for (auto it = data.begin(); it < end; it += V::size())
      {
        auto chunk = std::ranges::subrange(it, end);
        V x = V(chunk) + 0.1f;
        x.copy_to(chunk);
      }
  }
  add_throughput_counters(state);
}

void
smart_range_ctor(benchmark::State &state)
{
  using V = std::simd<float>;
  std::vector<float> data(state.range(0), 1.f);
  for (auto _ : state) {
    auto it = data.begin();
    const auto end = data.end();
    for (; it <= end - V::size(); it += V::size())
      {
        auto chunk = std::ranges::subrange(it, it + V::size());
        V x = V(chunk) + 0.1f;
        x.copy_to(chunk);
      }
    auto chunk = std::ranges::subrange(it, end);
    V x = V(chunk) + 0.1f;
    x.copy_to(chunk);
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
BENCHMARK(iterator_ctor)->Apply(MyRange);
BENCHMARK(naive_range_ctor)->Apply(MyRange);
BENCHMARK(smart_range_ctor)->Apply(MyRange);
