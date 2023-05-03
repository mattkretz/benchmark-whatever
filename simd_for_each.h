/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */
#include <vir/simd.h>

#include <ranges>

namespace stdx = vir::stdx;

template <typename T> auto data_or_ptr(T&& r) -> decltype(std::ranges::data(r))
{
  return std::ranges::data(r);
}

template <typename T> T* data_or_ptr(T* ptr) { return ptr; }
template <typename T> const T* data_or_ptr(const T* ptr) { return ptr; }

// Invokes fun(V&) or fun(const V&) with V copied from r at offset i.
// If write_back is true, copy it back to r.
template <typename V, bool write_back, typename Flags, std::size_t... Is>
constexpr void simd_invoke(auto&& fun, auto&& r, std::size_t i, Flags f,
                           std::index_sequence<Is...>)
{
  [&](auto... chunks) {
    std::invoke(fun, chunks...);
    if constexpr (write_back) {
      (chunks.copy_to(data_or_ptr(r) + i + (V::size() * Is), f), ...);
    }
  }(std::conditional_t<write_back, V, const V>(data_or_ptr(r) + i + (V::size() * Is),
                                               f)...);
}

template <class V, bool write_back, std::size_t max_bytes>
constexpr void simd_for_each_prologue(auto&& fun, auto ptr, std::size_t to_process)
{
  if (V::size() & to_process) {
    simd_invoke<V, write_back>(fun, ptr, 0, stdx::vector_aligned,
                               std::make_index_sequence<1>());
    ptr += V::size();
  }
  if constexpr (V::size() * 2 * sizeof(typename V::value_type) < max_bytes) {
    simd_for_each_prologue<stdx::resize_simd_t<V::size() * 2, V>, write_back, max_bytes>(
        fun, ptr, to_process);
  }
}

template <class V0, bool write_back>
constexpr void simd_for_each_epilogue(auto&& fun, auto&& rng, std::size_t i, auto f)
{
  using V = stdx::resize_simd_t<V0::size() / 2, V0>;
  if (i + V::size() <= std::ranges::size(rng)) {
    simd_invoke<V, write_back>(fun, rng, i, f, std::make_index_sequence<1>());
    i += V::size();
  }
  if constexpr (V::size() > 1) {
    simd_for_each_epilogue<V, write_back>(fun, rng, i, f);
  }
}

namespace execution
{
inline constexpr struct simd_policy_prefer_aligned_t {
} simd_policy_prefer_aligned{};

template <int N>
  requires(N > 1)
struct simd_policy_unroll_by_t : std::integral_constant<int, N> {
};

template <int N> inline constexpr simd_policy_unroll_by_t<N> simd_policy_unroll_by{};

template <typename T> struct is_simd_policy_unroll_by : std::integral_constant<int, 0> {
};

template <int N>
struct is_simd_policy_unroll_by<const simd_policy_unroll_by_t<N>>
    : std::integral_constant<int, N> {
};

template <auto... Options> struct simd_policy {
  static constexpr bool _prefers_aligned =
      (false or ... or std::same_as<decltype(Options), const simd_policy_prefer_aligned_t>);

  static constexpr int _unroll_by =
      (0 + ... + is_simd_policy_unroll_by<decltype(Options)>::value);

  static constexpr simd_policy<Options..., simd_policy_prefer_aligned> prefer_aligned()
    requires(not _prefers_aligned)
  { return {}; }

  template <int N>
  static constexpr simd_policy<Options..., simd_policy_unroll_by<N>> unroll_by()
    requires(_unroll_by == 0)
  { return {}; }
};

inline constexpr simd_policy<> simd {};

template <typename T> struct is_simd_policy : std::false_type {
};

template <auto... Options>
struct is_simd_policy<simd_policy<Options...>> : std::true_type {
};
}

template <typename ExecutionPolicy, std::ranges::contiguous_range R, typename F>
  requires execution::is_simd_policy<ExecutionPolicy>::value
constexpr void for_each(ExecutionPolicy, R&& rng, F&& fun)
{
  using V = stdx::native_simd<std::ranges::range_value_t<R>>;
  constexpr bool write_back = std::ranges::output_range<R, typename V::value_type> and
                              std::invocable<F, V&> and not std::invocable<F, V&&>;
  std::size_t i = 0;
  constexpr std::conditional_t<ExecutionPolicy::_prefers_aligned, stdx::vector_aligned_tag,
                               stdx::element_aligned_tag>
      flags{};
  if constexpr (ExecutionPolicy::_prefers_aligned) {
    const auto misaligned_by = reinterpret_cast<std::uintptr_t>(std::ranges::data(rng)) %
                               stdx::memory_alignment_v<V>;
    if (misaligned_by != 0) {
      const auto to_process = stdx::memory_alignment_v<V> - misaligned_by;
      simd_for_each_prologue<stdx::resize_simd_t<1, V>, write_back,
                             stdx::memory_alignment_v<V>>(fun, std::ranges::data(rng),
                                                          to_process);
      i = to_process;
    }
  }
  if constexpr (ExecutionPolicy::_unroll_by > 1) {
    for (; i + V::size() * ExecutionPolicy::_unroll_by <= std::ranges::size(rng);
         i += V::size() * ExecutionPolicy::_unroll_by) {
      simd_invoke<V, write_back>(fun, rng, i, flags,
                                 std::make_index_sequence<ExecutionPolicy::_unroll_by>());
    }
  }
  for (; i + V::size() <= std::ranges::size(rng); i += V::size()) {
    simd_invoke<V, write_back>(fun, rng, i, flags, std::make_index_sequence<1>());
  }
  simd_for_each_epilogue<V, write_back>(fun, rng, i, flags);
}
