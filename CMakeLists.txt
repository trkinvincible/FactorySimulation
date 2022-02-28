cmake_minimum_required(VERSION 3.10)
project(factory-simulation)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(RUN_PROFILE "Enable profiling" OFF)

if(RUN_PROFILE)
    message("profiling enabled")
    add_definitions(-DRUN_PROFILER)
    find_package(benchmark REQUIRED)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wextra")
# somehow santizer fails for valgrind
if(NOT RUN_PROFILE)
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

SET( _SOURCES_

    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

SET( _HEADER_

    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/belt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/worker.h
)

add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})

if (RUN_PROFILE)
    set(PROFILE_FLAGS
        -Wl,--no-as-needed
        -fno-pie
        -fno-omit-frame-pointer
        benchmark::benchmark
        -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free)
endif()

target_link_libraries(${PROJECT_NAME}
    -pthread
    -latomic
    ${PROFILE_FLAGS}
)

