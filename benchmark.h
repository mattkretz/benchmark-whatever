/*
Copyright © 2016-2018 Matthias Kretz <kretz@kde.org>
Copyright © 2016 Björn Gaier

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the names of contributing organizations nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/
#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <benchmark/benchmark.h>
#include <memory>
#include "typetostring.h"

struct TemplateWrapper {
  std::vector<benchmark::internal::Benchmark *> benchmarks;

  void append(benchmark::internal::Benchmark *ptr) { benchmarks.push_back(ptr); }

  TemplateWrapper *operator->() { return this; }

  TemplateWrapper &Arg(int x) {
    for (auto &p : benchmarks) {
      p->Arg(x);
    }

    return *this;
  }

  TemplateWrapper &Range(int start, int limit) {
    for (auto &p : benchmarks) {
      p->Range(start, limit);
    }

    return *this;
  }

  TemplateWrapper &DenseRange(int start, int limit) {
    for (auto &p : benchmarks) {
      p->DenseRange(start, limit);
    }

    return *this;
  }

  TemplateWrapper &ArgPair(int x, int y) {
    for (auto &p : benchmarks) {
      p->ArgPair(x, y);
    }

    return *this;
  }

  TemplateWrapper &RangePair(int lo1, int hi1, int lo2, int hi2) {
    for (auto &p : benchmarks) {
      p->RangePair(lo1, hi1, lo2, hi2);
    }

    return *this;
  }

  template <typename... Ts> TemplateWrapper &Apply(Ts &&... args) {
    for (auto &p : benchmarks) {
      p->Apply(args...);
    }

    return *this;
  }

  TemplateWrapper &MinTime(double t) {
    for (auto &p : benchmarks) {
      p->MinTime(t);
    }

    return *this;
  }

  TemplateWrapper &UseRealTime() {
    for (auto &p : benchmarks) {
      p->UseRealTime();
    }

    return *this;
  }

  TemplateWrapper &Threads(int t) {
    for (auto &p : benchmarks) {
      p->Threads(t);
    }

    return *this;
  }

  TemplateWrapper &ThreadRange(int min_threads, int max_threads) {
    for (auto &p : benchmarks) {
      p->ThreadRange(min_threads, max_threads);
    }

    return *this;
  }

  TemplateWrapper &ThreadPerCpu() {
    for (auto &p : benchmarks) {
      p->ThreadPerCpu();
    }

    return *this;
  }

  operator int() { return 0; }
};

#define SIMD_BENCHMARK_TEMPLATE(n_, ...)                                                   \
  template <unsigned int N>                                                              \
  TemplateWrapper BENCHMARK_PRIVATE_CONCAT(typeListFunc, n_, __LINE__)() {               \
    using TArg = __VA_ARGS__::at<N>;                                                     \
    constexpr auto *fptr = n_<TArg>;                                                     \
    static std::string name = #n_"<" + typeToString<TArg>() + '>';                       \
    TemplateWrapper wrapper =                                                            \
        BENCHMARK_PRIVATE_CONCAT(typeListFunc, n_, __LINE__)<N - 1>();                   \
    wrapper.append(benchmark::RegisterBenchmark(name.c_str(), fptr));                    \
    return wrapper;                                                                      \
  }                                                                                      \
                                                                                         \
  template <>                                                                            \
  TemplateWrapper BENCHMARK_PRIVATE_CONCAT(typeListFunc, n_, __LINE__)<0u>() {           \
    using TArg = __VA_ARGS__::at<0>;                                                     \
    constexpr auto *fptr = n_<TArg>;                                                     \
    static std::string name = #n_"<" + typeToString<TArg>() + '>';                       \
    TemplateWrapper wrapper;                                                             \
    wrapper.append(benchmark::RegisterBenchmark(name.c_str(), fptr));                    \
    return wrapper;                                                                      \
  }                                                                                      \
  int BENCHMARK_PRIVATE_CONCAT(variable, n_, __LINE__) =                                 \
      BENCHMARK_PRIVATE_CONCAT(typeListFunc, n_, __LINE__)<__VA_ARGS__::size() - 1>()

///////////////////////////////////////////////////////////////////////////////
// element_count<T>
template <class T> struct element_count : std::integral_constant<std::size_t, 1> {};

template <typename T>
  concept has_ic_size
    = requires { T::size.value; } and T::size.value == T::size() and T::size() == T::size;

template <typename T>
  concept has_subscript_operator
    = requires(const T &x) { sizeof(x[0]); };

template <class T>
requires stdx::is_simd_v<T>
struct element_count<T> : std::integral_constant<std::size_t, T::size()> {
};

template <has_ic_size T>
  struct element_count<T> : decltype(T::size)
  {};

template <has_subscript_operator T>
  requires (not stdx::is_simd_v<T> and not has_ic_size<T>)
  struct element_count<T>
    : std::integral_constant<std::size_t,
                             sizeof(T) / sizeof(std::declval<const T &>()[0])>
  {};

///////////////////////////////////////////////////////////////////////////////
// add_*_counters
static void
add_flop_counters(benchmark::State &state, int flop_per_iteration)
{
  state.counters["FLOP"] = {static_cast<double>(flop_per_iteration),
                            benchmark::Counter::kIsIterationInvariantRate};
  if (state.counters.contains("CYCLES"))
    state.counters["FLOP/cycle"] = {flop_per_iteration / state.counters["CYCLES"],
                                    benchmark::Counter::kIsIterationInvariant};
}

template <typename T = float>
  static void
  add_throughput_counters(benchmark::State& state)
  {
    if constexpr (std::is_same_v<T, void>)
      {
        const double values_per_iteration = state.range(0);
        state.counters["throughput / (values per s)"] = {values_per_iteration,
                                                   benchmark::Counter::kIsIterationInvariantRate,
                                                   benchmark::Counter::kIs1024};

        if (state.counters.contains("CYCLES"))
          {
            state.counters["throughput / (values per cycle)"] = {
              values_per_iteration / state.counters["CYCLES"],
              benchmark::Counter::kIsIterationInvariant
            };
          }
      }
    else
      {
        const double bytes_per_iteration = state.range(0) * sizeof(T);
        state.counters["throughput / (Byte/s)"] = {double(bytes_per_iteration),
                                                   benchmark::Counter::kIsIterationInvariantRate,
                                                   benchmark::Counter::kIs1024};

        if (state.counters.contains("CYCLES"))
          {
            state.counters["throughput / (Bytes per cycle)"] = {
              double(bytes_per_iteration) / state.counters["CYCLES"],
              benchmark::Counter::kIsIterationInvariant
            };
          }
      }

    if (state.counters.contains("INSTRUCTIONS"))
      {
        state.counters["asm efficiency / (instructions per value)"] = {
          double(state.range(0)) / state.counters["INSTRUCTIONS"],
          benchmark::Counter::kIsIterationInvariant | benchmark::Counter::kInvert
        };
      }
  }

BENCHMARK_MAIN();
#endif // BENCHMARK_H
