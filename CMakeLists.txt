cmake_minimum_required(VERSION 3.22)

project(MercaMiner
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.10.5 REQUIRED)

add_library(mercahash
    src/crypto/mercahash.cpp
    src/crypto/sha3.cpp
)

target_include_directories(mercahash
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(mercaminer_core
    src/header.cpp
    src/target.cpp
    src/uint256.cpp
)

target_include_directories(mercaminer_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(mercaminer_rpc
    src/rpc.cpp
)

target_include_directories(mercaminer_rpc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_rpc
    PUBLIC
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(mercahash_vectors
    tests/mercahash_vectors.cpp
)

target_link_libraries(mercahash_vectors
    PRIVATE
        mercahash
)

add_executable(header_tests
    tests/header_tests.cpp
)

target_link_libraries(header_tests
    PRIVATE
        mercaminer_core
        mercahash
)

add_executable(target_tests
    tests/target_tests.cpp
)

target_link_libraries(target_tests
    PRIVATE
        mercaminer_core
)

add_executable(rpc_tests
    tests/rpc_tests.cpp
)

target_link_libraries(rpc_tests
    PRIVATE
        mercaminer_rpc
)

enable_testing()

add_test(
    NAME rpc_tests
    COMMAND rpc_tests
)

add_test(
    NAME mercahash_vectors
    COMMAND mercahash_vectors
)

add_test(
    NAME header_tests
    COMMAND header_tests
)

add_test(
    NAME target_tests
    COMMAND target_tests
)
