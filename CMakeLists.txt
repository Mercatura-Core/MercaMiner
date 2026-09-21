cmake_minimum_required(VERSION 3.22)

project(MercaMiner
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mercahash
    src/crypto/mercahash.cpp
    src/crypto/sha3.cpp
)

target_include_directories(mercahash
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(mercahash_vectors
    tests/mercahash_vectors.cpp
)

target_link_libraries(mercahash_vectors
    PRIVATE
        mercahash
)

enable_testing()

add_test(
    NAME mercahash_vectors
    COMMAND mercahash_vectors
)
