cmake_minimum_required(VERSION 3.10)
project(factory-simulation)

set(CMAKE_CXX_STANDARD 17)

SET( _SOURCES_
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

add_executable(${PROJECT_NAME} ${_SOURCES_})

target_link_libraries(${PROJECT_NAME} -pthread -latomic)
