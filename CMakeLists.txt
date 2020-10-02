cmake_minimum_required(VERSION 3.16)
project(FlowRender)

set(CMAKE_CXX_STANDARD 17)

include_directories(..)
add_executable(FlowRender main.cpp ListPosition.h)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.66.0 COMPONENTS system filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(FlowRender ${Boost_LIBRARIES})
