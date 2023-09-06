/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2014-2023 Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VC_TO_STRING_H
#define VC_TO_STRING_H
#include <sstream>
#include "typelist.h"
#include <experimental/simd>
#ifdef HAVE_CXX_ABI_H
#include <cxxabi.h>
#endif

// typeToString
template <typename T> inline std::string typeToString();

// std::array<T, N>
template <typename T, std::size_t N>
inline std::string typeToString_impl(std::array<T, N> const &) {
  std::stringstream s;
  s << "array<" << typeToString<T>() << ", " << N << '>';
  return s.str();
}

// std::vector<T>
template <typename T> inline std::string typeToString_impl(std::vector<T> const &) {
  std::stringstream s;
  s << "vector<" << typeToString<T>() << '>';
  return s.str();
}

// std::integral_constant<T, N>
template <typename T, T N>
inline std::string typeToString_impl(std::integral_constant<T, N> const &) {
  std::stringstream s;
  s << "integral_constant<" << N << '>';
  return s.str();
}

// template parameter pack to a comma separated string
template <typename T0, typename... Ts>
std::string typeToString_impl(Typelist<T0, Ts...> const &) {
  std::stringstream s;
  s << '{' << typeToString<T0>();
  auto &&x = {(s << ", " << typeToString<Ts>(), 0)...};
  if (&x == nullptr) {
  } // avoid warning about unused 'x'
  s << '}';
  return s.str();
}

inline std::string
typeToString_impl(stdx::simd_abi::scalar const&) {
  return "scalar";
}

template <int N>
inline std::string
typeToString_impl(stdx::simd_abi::fixed_size<N> const&) {
  std::stringstream s;
  s << "fixed_size<" << N << '>';
  return s.str();
}

template <int N>
inline std::string
typeToString_impl(stdx::simd_abi::_VecBuiltin<N> const&) {
  std::stringstream s;
  s << "_VecBuiltin<" << N << '>';
  return s.str();
}

template <int N>
inline std::string
typeToString_impl(stdx::simd_abi::_VecBltnBtmsk<N> const&) {
  std::stringstream s;
  s << "_VecBltnBtmsk<" << N << '>';
  return s.str();
}

template <typename V>
requires stdx::is_simd_v<V>
inline std::string
typeToString_impl(V const &) {
  using T = typename V::value_type;
  using A = typename V::abi_type;
  std::stringstream s;
  s << "simd<" << typeToString<T>() << ", " << typeToString<A>() << '>';
  return s.str();
}

template <typename V>
requires stdx::is_simd_mask_v<V>
inline std::string
typeToString_impl(V const &) {
  using T = typename V::simd_type::value_type;
  using A = typename V::abi_type;
  std::stringstream s;
  s << "simd_mask<" << typeToString<T>() << ", " << typeToString<A>() << '>';
  return s.str();
}

// generic fallback (typeid::name)
template <typename T>
requires (not stdx::is_simd_v<T> and not stdx::is_simd_mask_v<T>)
inline std::string typeToString_impl( T const &) {
#ifdef HAVE_CXX_ABI_H
  char buf[1024];
  size_t size = 1024;
  abi::__cxa_demangle(typeid(T).name(), buf, &size, nullptr);
  return std::string{buf};
#else
  return typeid(T).name();
#endif
}

// typeToString specializations
template <typename T> inline std::string typeToString() { return typeToString_impl(T()); }
template <> inline std::string typeToString<void>() { return ""; }

template <> inline std::string typeToString<long double>() { return "long double"; }
template <> inline std::string typeToString<double>() { return "double"; }
template <> inline std::string typeToString<float>() { return " float"; }
template <> inline std::string typeToString<long long>() { return " llong"; }
template <> inline std::string typeToString<unsigned long long>() { return "ullong"; }
template <> inline std::string typeToString<long>() { return "  long"; }
template <> inline std::string typeToString<unsigned long>() { return " ulong"; }
template <> inline std::string typeToString<int>() { return "   int"; }
template <> inline std::string typeToString<unsigned int>() { return "  uint"; }
template <> inline std::string typeToString<short>() { return " short"; }
template <> inline std::string typeToString<unsigned short>() { return "ushort"; }
template <> inline std::string typeToString<char>() { return "  char"; }
template <> inline std::string typeToString<unsigned char>() { return " uchar"; }
template <> inline std::string typeToString<signed char>() { return " schar"; }
#endif
