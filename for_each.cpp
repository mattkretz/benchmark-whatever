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

namespace stdx = vir::stdx;

constexpr long smallest = 2;
constexpr long largest = 4 << 20;

//using type = float;
//#define OP(x) x = x * 0.9f + x

using type = int;
#define OP(x) x += 1

alignas(4096) std::array<char, largest * sizeof(type) * 2> s_buffer;
std::pmr::monotonic_buffer_resource s_memory {s_buffer.data(), s_buffer.size()};

auto make_data(std::size_t n)
{
  s_memory.release();
  std::pmr::vector<type> v(n, &s_memory);
  std::generate(v.begin(), v.end(), [] { return std::rand() % 2 ? 1 : -1; });
  if (n != v.size())
    std::abort();
  return v;
}

template <auto pol>
  [[gnu::always_inline]]
  void
  do_benchmark(benchmark::State& state, auto& v)
  {
    if constexpr (std::is_same_v<decltype(pol), decltype(std::execution::seq)>)
      std::for_each(v.begin(), v.end(), [](auto& x) { OP(x); });
    else
      std::for_each(pol, v.begin(), v.end(), [](auto&... x) {
        ((OP(x)), ...);
      });
    for (auto _ : state)
      {
        asm volatile("");
        if constexpr (std::is_same_v<decltype(pol), decltype(std::execution::seq)>)
          std::for_each(v.begin(), v.end(), [](auto& x) { OP(x); });
        else
          std::for_each(pol, v.begin(), v.end(), [](auto&... x) {
            ((OP(x)), ...);
          });
        vir::fake_read(v.data());
        asm volatile("");
      }
    add_throughput_counters<void>(state);
  }

enum Variant
{
  Aligned,
  Misaligned
};

template <auto pol, Variant var = Aligned>
  void
  foreach(benchmark::State& state)
  {
    if constexpr (var == Aligned)
      {
        auto v = make_data(state.range(0));
        do_benchmark<pol>(state, v);
      }
    else
      {
        auto aligned = make_data(state.range(0) + 1);
        std::span v(aligned.begin() + 1, aligned.end());
        do_benchmark<pol>(state, v);
      }
  }

template <auto pol, Variant var = Aligned>
  [[gnu::optimize("-O3"), gnu::flatten]]
  void
  foreach_O3(benchmark::State& state)
  { foreach<pol, var>(state); }

static void
MyRange(benchmark::internal::Benchmark* b)
{
  for (long i = smallest; i <= largest; i += i)
    b->Args({i - 1});
  for (long i = smallest; i <= largest; i += i)
    b->Args({i});
}

BENCHMARK(foreach<vir::execution::simd>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.unroll_by<2>()>)->Apply(MyRange);
BENCHMARK(foreach<vir::execution::simd.unroll_by<4>()>)->Apply(MyRange);
BENCHMARK(foreach<vir::execution::simd.unroll_by<8>()>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned().unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_unaligned().unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned()>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned().unroll_by<4>()>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned().unroll_by<8>()>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd, Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.unroll_by<8>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned(), Misaligned>)->Apply(MyRange);
//BENCHMARK(foreach<vir::execution::simd.prefer_aligned().unroll_by<8>(), Misaligned>)->Apply(MyRange);
BENCHMARK(foreach<std::execution::unseq>)->Apply(MyRange);
BENCHMARK(foreach_O3<std::execution::unseq>)->Apply(MyRange);
BENCHMARK(foreach<std::execution::seq>)->Apply(MyRange);
BENCHMARK(foreach_O3<std::execution::seq>)->Apply(MyRange);
