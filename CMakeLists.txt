cmake_minimum_required(VERSION 2.8)
project(markov)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include_directories(include)

add_library(markov_chain SHARED src/markov_chain.cpp src/node.cpp src/base.cpp)

add_executable(chain_creator src/creator.cpp)
add_executable(chain_executor src/executor.cpp)

target_link_libraries(chain_creator markov_chain)
target_link_libraries(chain_executor markov_chain)