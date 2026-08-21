cmake_minimum_required(VERSION 3.20)
project(itch_lob CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core compile flags: optimise + warn aggressively
add_compile_options(-O3 -Wall -Wextra -Wpedantic)

# Header-only library target -  lets test/ and future src/ files include cleanly
add_library(itch_headers INTERFACE)
target_include_directories(itch_headers INTERFACE ${CMAKE_SOURCE_DIR}/src)

# Placeholder - parser executable will go here
add_executable(parser src/parser.cpp)
target_link_libraries(parser PRIVATE itch_headers)

# Matching Engine unit test - independent from the ITCH parser
add_executable(test_matching_engine test/test_matching_engine.cpp)
target_link_libraries(test_matching_engine PRIVATE itch_headers)

# Matching Engine BenchMark
add_executable(bench_matching_engine bench/bench_matching_engine.cpp)
target_link_libraries(bench_matching_engine PRIVATE itch_headers)