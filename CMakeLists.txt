cmake_minimum_required(VERSION 3.18)
project(cv_pipeline_profiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warnings
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0 -fsanitize=address,undefined>
)

add_link_options(
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# OpenCV (required)
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")

# Threads (required for std::thread, mutex, etc.)
find_package(Threads REQUIRED)

# Header-only profiler library
# All core, stages, metrics, reporting headers are included directly.
# Phase 3+ will introduce compiled .cpp files here.

add_library(cvprofiler INTERFACE)
target_include_directories(cvprofiler INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(cvprofiler INTERFACE
    ${OpenCV_LIBS}
    Threads::Threads
)

# Mode 1: Pipeline engine owns execution
add_executable(cvprofiler_demo1
    app/demo_mode1.cpp
)
target_link_libraries(cvprofiler_demo1 PRIVATE cvprofiler)

# Mode 2: Probe injection into existing app
add_executable(cvprofiler_demo2
    app/demo_mode2.cpp
)
target_link_libraries(cvprofiler_demo2 PRIVATE cvprofiler)

# Tests 
enable_testing()

add_executable(test_frame tests/test_frame.cpp)
target_link_libraries(test_frame PRIVATE cvprofiler)
add_test(NAME test_frame COMMAND test_frame)

add_executable(test_metrics tests/test_metrics.cpp)
target_link_libraries(test_metrics PRIVATE cvprofiler)
add_test(NAME test_metrics COMMAND test_metrics)

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE cvprofiler)
add_test(NAME test_pipeline COMMAND test_pipeline)

# Install targets (for Docker / packaging)
install(TARGETS cvprofiler_demo1 cvprofiler_demo2
    RUNTIME DESTINATION bin
)
install(DIRECTORY core/ metrics/ stages/ reporting/
    DESTINATION include/cvprofiler
    FILES_MATCHING PATTERN "*.hpp"
)

# -----------------------------------------------------------------------
# Print build summary
# -----------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== CV Pipeline Profiler Build Config ===")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std    : ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV     : ${OpenCV_VERSION}")
message(STATUS "  Targets    : cvprofiler_demo1  cvprofiler_demo2")
message(STATUS "  Tests      : test_frame  test_metrics  test_pipeline")
message(STATUS "")
