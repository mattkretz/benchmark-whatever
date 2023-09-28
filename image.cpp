#include <execution>
#include "benchmark.h"
#include <../simd-prototyping/simd.h>
#include <../simd-prototyping/simd_reductions.h>
#include <vir/simd_benchmarking.h>

struct Scalar
{
  struct Pixel
  {
    std::uint8_t b, g, r, a;
  };

  using Image = std::vector<Pixel>;

  static void
  to_gray(Image& img)
  {
    for (auto& pixel : img)
      {
        const auto gray = (pixel.r * 11 + pixel.g * 16 + pixel.b * 5) / 32;
        pixel.r = pixel.g = pixel.b = gray;
      }
  }
};

struct Unseq : Scalar
{
  static void
  to_gray(Image& img)
  {
    std::for_each(std::execution::unseq, img.begin(), img.end(), [](auto& pixel) {
      const auto gray = (pixel.r * 11 + pixel.g * 16 + pixel.b * 5) / 32;
      pixel.r = pixel.g = pixel.b = gray;
    });
  }
};

struct SimdPixel
{
  using Pixel = std::simd<std::uint8_t, 4>; // 0:b 1:g 2:r 3:a
  using Pixel32 = std::rebind_simd_t<std::uint32_t, Pixel>;
  using Pixel32Mask = Pixel32::mask_type;

  using Image = std::vector<Pixel>;

  static constexpr unsigned gray_coeff[4] = {11, 16, 5, 1};

  static void
  to_gray(auto& img)
  {
    constexpr Pixel32Mask mask([](auto i) { return i < 3; });
    for (auto& p8 : img)
      {
        const Pixel32 pixel = p8;
        const std::uint32_t gray = std::reduce(pixel * Pixel32(gray_coeff), mask) / 32u;
        p8 = static_cast<Pixel>(std::simd_select(mask, gray, pixel));
      }
  }
};

template <int ILP>
struct DataParallel
{
  using Pixel = std::uint32_t; // 0xAARRGGBB

  using Image = std::vector<Pixel>;

  using PixelV = std::simd<Pixel, std::simd<Pixel>::size() * ILP>;

  static void
  to_gray(Image& img)
  {
    for (auto it = img.begin(); it < img.end(); it += PixelV::size())
      {
        PixelV pixels(it);
        const auto a =  pixels >> 24;
        const auto r = (pixels >> 16) & 0xffU;
        const auto g = (pixels >> 8) & 0xffU;
        const auto b =  pixels & 0xffU;
        const auto gray = (r * 11u + g * 16u + b * 5u) / 32u;
        pixels = gray | (gray << 8) | (gray << 16) | (a << 24);
        pixels.copy_to(it);
      }
  }
};

template <typename Image>
  Image
  make_image(std::size_t size)
  { return Image(size); }

template <typename Variant>
  void
  bench(benchmark::State &state)
  {
    auto img = make_image<typename Variant::Image>(state.range(0));
    asm("" : "+m"(img));
    for (auto _ : state) {
      Variant::to_gray(img);
      asm("" :: "m"(img));
    }
    add_throughput_counters<typename Variant::Pixel>(state);
  }

template <typename var>
  [[gnu::optimize("-O2"), gnu::flatten]]
  void
  bench_O2(benchmark::State &state)
  { bench<var>(state); }

template <typename var>
  [[gnu::optimize("-O3"), gnu::flatten]]
  void
  bench_O3(benchmark::State &state)
  { bench<var>(state); }

constexpr long smallest = 32 * 32;
constexpr long largest = 16 << 20;

static void
MyRange(benchmark::internal::Benchmark* b)
{
  for (long i = smallest; i <= largest; i += i)
    b->Args({i});
}

// Register the function as a benchmark
BENCHMARK(bench_O2<DataParallel<1>>)->Apply(MyRange);
BENCHMARK(bench_O2<DataParallel<2>>)->Apply(MyRange);
BENCHMARK(bench_O2<DataParallel<4>>)->Apply(MyRange);
BENCHMARK(bench_O2<SimdPixel>)->Apply(MyRange);
BENCHMARK(bench_O2<Scalar>)->Apply(MyRange);
BENCHMARK(bench_O2<Unseq>)->Apply(MyRange);
BENCHMARK(bench_O3<DataParallel<1>>)->Apply(MyRange);
BENCHMARK(bench_O3<SimdPixel>)->Apply(MyRange);
BENCHMARK(bench_O3<Scalar>)->Apply(MyRange);
BENCHMARK(bench_O3<Unseq>)->Apply(MyRange);
