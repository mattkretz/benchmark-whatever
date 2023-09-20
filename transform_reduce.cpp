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

template <typename T>
  struct Point
  {
    T x, y, z;

    friend constexpr Point
    operator+(Point a, Point b)
    { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

    friend constexpr T
    operator*(Point a, Point b)
    { return a.x * b.x + a.y * b.y + a.z * b.z; }
  };

using type = Point<float>;

alignas(4096) std::array<char, largest * sizeof(type) * 3> s_buffer;
std::pmr::monotonic_buffer_resource s_memory {s_buffer.data(), s_buffer.size()};

auto make_data(std::size_t n)
{
  s_memory.release();
  using Vec = std::pmr::vector<type>;
  std::array<Vec, 2> v { Vec(n, &s_memory), Vec(n, &s_memory) };
  std::generate(v[0].begin(), v[0].end(), [] {
    return type{std::rand() % 1 ? 1.f : -1,
                std::rand() % 1 ? 1.f : -1,
                std::rand() % 1 ? 1.f : -1}; });
  std::generate(v[1].begin(), v[1].end(), [] {
    return type{std::rand() % 1 ? 1.f : -1,
                std::rand() % 1 ? 1.f : -1,
                std::rand() % 1 ? 1.f : -1}; });
  if (n != v[0].size())
    std::abort();
  return v;
}

enum Variant
{
  Aligned,
  Misaligned,
  OrderedReduction
};

struct mult
{
  constexpr auto
  operator()(const auto& __x, const auto& __y) const
  { return __x * __y; }
};

template <auto pol, Variant var>
  [[gnu::always_inline]]
  void
  do_benchmark(benchmark::State& state, auto const& v0, auto const& v1)
  {
    if constexpr (not std::is_same_v<decltype(pol), decltype(std::execution::seq)>)
      vir::fake_read(std::transform_reduce(pol, v0.begin(), v0.end(), v1.begin(), 0.f));
    else if constexpr (var == OrderedReduction)
      vir::fake_read(std::inner_product(v0.begin(), v0.end(), v1.begin(), 0.f));
    else
      vir::fake_read(std::transform_reduce(v0.begin(), v0.end(), v1.begin(), 0.f));
    for (auto _ : state)
      {
        asm volatile("");
        if constexpr (not std::is_same_v<decltype(pol), decltype(std::execution::seq)>)
          vir::fake_read(std::transform_reduce(pol, v0.begin(), v0.end(), v1.begin(), 0.f));
        else if constexpr (var == OrderedReduction)
          vir::fake_read(std::inner_product(v0.begin(), v0.end(), v1.begin(), 0.f));
        else
          vir::fake_read(std::transform_reduce(v0.begin(), v0.end(), v1.begin(), 0.f));
        asm volatile("");
      }
    add_throughput_counters<void>(state);
  }

template <auto pol, Variant var = Aligned>
  void
  innerproduct(benchmark::State& state)
  {
    if constexpr (var == Aligned)
      {
        auto v = make_data(state.range(0));
        do_benchmark<pol, var>(state, v[0], v[1]);
      }
    else
      {
        auto aligned = make_data(state.range(0) + 1);
        std::span v0(aligned[0].begin() + 1, aligned[0].end());
        std::span v1(aligned[1].begin() + 1, aligned[1].end());
        do_benchmark<pol, var>(state, v0, v1);
      }
  }

template <auto pol, Variant var = Aligned>
  [[gnu::optimize("-O3"), gnu::flatten]]
  void
  innerproduct_O3(benchmark::State& state)
  { innerproduct<pol, var>(state); }

static void
MyRange(benchmark::internal::Benchmark* b)
{
  for (long i = smallest; i <= largest; i += i)
    b->Args({i - 1});
  for (long i = smallest; i <= largest; i += i)
    b->Args({i});
}

BENCHMARK(innerproduct<vir::execution::simd>)->Apply(MyRange);
BENCHMARK(innerproduct<vir::execution::simd.unroll_by<2>()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.unroll_by<4>()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.unroll_by<8>()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned().unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_unaligned().unroll_by<4>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned().unroll_by<4>()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned().unroll_by<8>()>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd, Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.unroll_by<8>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<vir::execution::simd.prefer_aligned().unroll_by<8>(), Misaligned>)->Apply(MyRange);
//BENCHMARK(innerproduct<std::execution::unseq>)->Apply(MyRange);
//BENCHMARK(innerproduct_O3<std::execution::unseq>)->Apply(MyRange);
BENCHMARK(innerproduct<std::execution::seq>)->Apply(MyRange);
//BENCHMARK(innerproduct_O3<std::execution::seq>)->Apply(MyRange);
//BENCHMARK(innerproduct<std::execution::seq, OrderedReduction>)->Apply(MyRange);
//BENCHMARK(innerproduct_O3<std::execution::seq, OrderedReduction>)->Apply(MyRange);
