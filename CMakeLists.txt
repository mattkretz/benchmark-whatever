cmake_minimum_required(VERSION 3.5)
include(CheckCXXSourceCompiles)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
   message(FATAL_ERROR "You don't want to configure in the source directory!")
endif()
project(BenchmarkExercises)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(PFM)
find_package(benchmark 1.4 REQUIRED)

CHECK_CXX_SOURCE_COMPILES("#include <cxxabi.h>
int main() { return 0; }" cxx_abi_header_works)
if(cxx_abi_header_works)
   add_definitions(-DHAVE_CXX_ABI_H)
endif()

find_path(Vir_INCLUDE_DIR "vir/simd.h" HINTS "../vir-simd")

find_path(SIMD_PROTOTYPE_INCLUDE_DIR "simd_reductions.h" HINTS "../simd-prototyping")

MACRO(add_benchmark title)
    add_executable(${title} ${title}.cpp)
    target_include_directories(${title} PRIVATE "${Vir_INCLUDE_DIR}" "${SIMD_PROTOTYPE_INCLUDE_DIR}")
    target_compile_options(${title} PRIVATE "-std=gnu++2b;-march=native")
    set_target_properties(${title} PROPERTIES LINK_FLAGS -pthread)
    target_link_libraries(${title} benchmark::benchmark)
    add_custom_target(run_${title}
      ${title} --benchmark_counters_tabular=true --benchmark_perf_counters=CYCLES,INSTRUCTIONS
        --benchmark_out=${title}.csv --benchmark_out_format=csv
       DEPENDS ${title}
       COMMENT "Execute ${title} benchmark"
       VERBATIM)
endmacro()

add_benchmark(countif)
add_benchmark(for_each)
add_benchmark(image)
add_benchmark(nearestneighbor)
add_benchmark(nearestneighbor3d)
add_benchmark(peakflop)
add_benchmark(peakflop-stdsimd)
add_benchmark(transform_reduce)
