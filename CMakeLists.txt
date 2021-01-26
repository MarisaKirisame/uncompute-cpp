cmake_minimum_required(VERSION 3.10)

# set the project name
project(Uncomputing)

# add the executable
add_executable(Uncomputing main.cpp)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)
# somehow this does not work. add flag manually
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
