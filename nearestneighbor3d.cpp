#include <vir/simd.h>
#include <vir/simd_benchmarking.h>
#include <vir/simd_cvt.h>
#include <vir/simd_iota.h>

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

template <typename T> struct Point {
  T x, y, z;
};

template <typename T> constexpr T sqr(T x) { return x * x; }

template <typename T, typename U> constexpr auto distance(Point<T> a, Point<U> b)
{
  return sqr(a.x - b.x) + sqr(a.y - b.y) + sqr(a.z - b.z);
}

template <typename T>
T generate_random_points(std::size_t n) {
  if constexpr (std::same_as<T, Point<std::vector<float>>>) {
    T points{std::vector<float>(n), std::vector<float>(n), std::vector<float>(n)};
    std::generate(points.x.begin(), points.x.end(), []() { return rnd0_10(gen); });
    std::generate(points.y.begin(), points.y.end(), []() { return rnd0_10(gen); });
    std::generate(points.z.begin(), points.z.end(), []() { return rnd0_10(gen); });
    return points;
  } else if constexpr (std::same_as<T, std::vector<Point<float>>>) {
    T points(n);
    std::generate(points.begin(), points.end(), []() {
      return Point<float>{rnd0_10(gen), rnd0_10(gen), rnd0_10(gen)};
    });
    return points;
  } else if constexpr (std::same_as<T, std::vector<Point<floatv>>>) {
    T points(n / floatv::size());
    std::generate(points.begin(), points.end(), []() {
      return Point<floatv>{floatv([&](int) { return rnd0_10(gen); }),
                           floatv([&](int) { return rnd0_10(gen); }),
                           floatv([&](int) { return rnd0_10(gen); })};
    });
    return points;
  }
}

template <typename T>
std::size_t index_of_nearest(const Point<std::vector<float>>& points,
                             const Point<float> to_find)
{
  float best = std::numeric_limits<float>::max();
  std::size_t idx = 0;
  if constexpr (stdx::is_simd_v<T>) {
    for (std::size_t i = 0; i < points.x.size(); i += T::size()) {
      const Point<T> a{T(&points.x[i], stdx::element_aligned),
                       T(&points.y[i], stdx::element_aligned),
                       T(&points.z[i], stdx::element_aligned)};
      const T d = distance(a, to_find);
      if (any_of(d < best)) {
        best = hmin(d);
        idx = i + find_first_set(d == best);
      }
    }
  } else {
    for (std::size_t i = 0; i < points.x.size(); ++i) {
      const Point<T> a{points.x[i], points.y[i], points.z[i]};
      const T d = distance(a, to_find);
      if (d < best) {
        best = d;
        idx = i;
      }
    }
  }
  return idx;
}

template <typename T>
std::size_t index_of_nearest(const std::vector<Point<float>>& points,
                             const Point<float> to_find)
{
  float best = std::numeric_limits<float>::max();
  std::size_t idx = 0;
  if constexpr (stdx::is_simd_v<T>) {
    for (std::size_t i = 0; i < points.size(); i += T::size()) {
      Point<T> a = {T([&](auto j) { return points[i + j].x; }),
                    T([&](auto j) { return points[i + j].y; }),
                    T([&](auto j) { return points[i + j].z; })};
      const T d = distance(a, to_find);
      if (any_of(d < best)) {
        best = hmin(d);
        idx = i + find_first_set(d == best);
      }
    }
  } else {
    for (std::size_t i = 0; i < points.size(); ++i) {
      const T d = distance(points[i], to_find);
      if (d < best) {
        best = d;
        idx = i;
      }
    }
  }
  return idx;
}

template <typename T>
std::size_t index_of_nearest(const std::vector<Point<T>>& points,
                             const Point<float> to_find)
{
  static_assert(stdx::is_simd_v<T>);
  float best = std::numeric_limits<float>::max();
  std::size_t idx = 0;
  std::size_t outer_idx = 0;
  for (const auto& p : points) {
    const T d = distance(p, to_find);
    if (any_of(d < best)) {
      best = hmin(d);
      idx = outer_idx + find_first_set(d == best);
    }
    outer_idx += T::size();
  }
  return idx;
}

template <typename T>
void verify(const std::vector<Point<T>>& points, const Point<float> to_find,
            const std::size_t idx, const std::size_t n)
{
  auto&& d = [&](std::size_t i) {
    if constexpr (stdx::is_simd_v<T>) {
      const auto j = i % floatv::size();
      const auto& p = points[i / floatv::size()];
      return distance(Point<float>{p.x[j], p.y[j], p.z[j]}, to_find);
    } else {
      return distance(points[i], to_find);
    }
  };
  const auto best = d(idx);
  for (std::size_t i = 0; i < n; ++i) {
    if (d(i) < best) {
      fail("wrong");
    }
  }
}

template <typename T>
void soa(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  assert(n % floatv::size() == 0);
  const auto points = generate_random_points<Point<std::vector<float>>>(n);
  Point<float> to_find = {rnd0_10(gen), rnd0_10(gen), rnd0_10(gen)};
  for (auto _ : state) {
    vir::fake_modify(to_find.x);
    vir::fake_read(index_of_nearest<T>(points, to_find));
  }
  state.SetBytesProcessed(state.iterations() * 3 * n * sizeof(float));
}

template <typename T> [[gnu::optimize("-O3")]] void soa_O3(benchmark::State& state)
{
  return soa<T>(state);
}

template <typename T>
void aos(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  const auto points = generate_random_points<std::vector<Point<float>>>(n);
  Point<float> to_find = {rnd0_10(gen), rnd0_10(gen), rnd0_10(gen)};
  std::size_t idx = 0;
  for (auto _ : state) {
    vir::fake_modify(to_find.x);
    vir::fake_read(idx = index_of_nearest<T>(points, to_find));
  }
  state.SetBytesProcessed(state.iterations() * 3 * n * sizeof(float));
  verify(points, to_find, idx, n);
}

template <typename T> [[gnu::optimize("-O3")]] void aos_O3(benchmark::State& state)
{
  return aos<T>(state);
}

template <typename T>
void aovs(benchmark::State& state)
{
  const std::size_t n = state.range(0);
  assert(n % T::size() == 0);
  const auto points = generate_random_points<std::vector<Point<T>>>(n);
  Point<float> to_find = {rnd0_10(gen), rnd0_10(gen), rnd0_10(gen)};
  std::size_t idx = 0;
  for (auto _ : state) {
    vir::fake_modify(to_find.x);
    vir::fake_read(idx = index_of_nearest(points, to_find));
  }
  state.SetBytesProcessed(state.iterations() * 3 * n * sizeof(float));
  verify(points, to_find, idx, n);
}

constexpr std::size_t smallest = 1 << 6;
constexpr auto largest = 1 << 23;

#define MYRANGE RangeMultiplier(2)->Range(smallest, largest)

BENCHMARK(aos<float>)->MYRANGE;
BENCHMARK(aos_O3<float>)->MYRANGE;
BENCHMARK(aos<floatv>)->MYRANGE;
BENCHMARK(soa<float>)->MYRANGE;
BENCHMARK(soa_O3<float>)->MYRANGE;
BENCHMARK(soa<floatv>)->MYRANGE;
BENCHMARK(aovs<floatv>)->MYRANGE;
