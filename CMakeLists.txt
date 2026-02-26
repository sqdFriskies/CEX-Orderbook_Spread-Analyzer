cmake_minimum_required(VERSION 3.16)
project(TestTask_PT)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(orderbook          src/main.cpp)
add_executable(generate_orderbook src/generate_orderbook.cpp)