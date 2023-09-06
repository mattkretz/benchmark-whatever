#include <vir/simd.h>
#include <vir/simd_benchmarking.h>

#include <random>
#include <iostream>

#include "benchmark.h"

void fail(auto&&... info) {
  (std::cerr << ... << info) << '\n';
  std::abort();
}

namespace stdx = vir::stdx;

using floatv = stdx::native_simd<float>;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> rnd0_10(0.f, 10.f);

template <typename T>
void linear_search(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  assert(n % floatv::size() == 0);
  std::vector<float> data(n);
  for (auto& x : data) {
    x = rnd0_10(gen);
  }
  const float to_find = rnd0_10(gen);

  std::size_t idx = 0;
  for (auto _ : state) {
    float best = std::numeric_limits<float>::max();
    idx = 0;
    std::size_t i = 0;
    if constexpr (stdx::is_simd_v<T>) {
      for (std::size_t i = 0; i < n; i += T::size()) {
        const auto d = abs(T(&data[i], stdx::element_aligned) - to_find);
        if (any_of(d < best)) {
          best = hmin(d);
          idx = i + find_first_set(d == best);
        }
      }
    } else {
      for (float x : data) {
        const auto d = std::abs(x - to_find);
        if (d < best) {
          best = d;
          idx = i;
        }
        ++i;
      }
    }
    vir::fake_read(idx);
  }
  state.SetBytesProcessed(state.iterations() * data.size() * sizeof(float));

  const auto best = std::abs(data[idx] - to_find);
  for (float x : data) {
    if (std::abs(x - to_find) < best) {
      fail("wrong. found ", data[idx], " at ", idx, " and ", x, " is closer to ", to_find);
    }
  }
}

template <typename T>
[[gnu::optimize("-O3")]] void linear_search_O3(benchmark::State& state)
{
  linear_search<T>(state);
}

constexpr std::size_t smallest = 1 << 6;
constexpr auto largest = 1 << 23;

#define MYRANGE RangeMultiplier(4)->Range(smallest, largest)

BENCHMARK(linear_search<float>)->MYRANGE;
BENCHMARK(linear_search<floatv>)->MYRANGE;
BENCHMARK(linear_search_O3<float>)->MYRANGE;
