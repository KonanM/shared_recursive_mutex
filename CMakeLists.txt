cmake_minimum_required(VERSION 3.8)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)
project(shared_recursive_mutex VERSION "0.1" LANGUAGES CXX)

option(shared_recursive_mutex_OPT_BUILD_EXAMPLES "Build shared_recursive_mutex examples" ON)
option(shared_recursive_mutex_OPT_BUILD_TESTS "Build and perform shared_recursive_mutex tests" ON)
set(HEADER $<INSTALL_INTERFACE:include/shared_recursive_mutex/shared_recursive_mutex.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/shared_recursive_mutex/shared_recursive_mutex.hpp>)

add_library(shared_recursive_mutex INTERFACE)
target_sources(shared_recursive_mutex INTERFACE ${HEADER})
target_include_directories(shared_recursive_mutex INTERFACE $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(shared_recursive_mutex INTERFACE cxx_std_17)

if(MSVC)
    add_custom_target(shared_recursive_mutex.header SOURCES ${HEADER})
endif()

if(shared_recursive_mutex_OPT_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(shared_recursive_mutex_OPT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
