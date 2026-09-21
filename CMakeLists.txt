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

add_library(mercaminer_gbt
    src/gbt.cpp
)

target_include_directories(mercaminer_gbt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_gbt
    PUBLIC
        mercaminer_core
        nlohmann_json::nlohmann_json
)

add_library(mercaminer_network
    src/network.cpp
)

target_include_directories(mercaminer_network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_network
    PUBLIC
        mercaminer_gbt
)

add_executable(mercaminer_probe
    src/node_probe.cpp
)

target_link_libraries(mercaminer_probe
    PRIVATE
        mercaminer_rpc
        mercaminer_gbt
        mercaminer_network
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

add_executable(gbt_tests
    tests/gbt_tests.cpp
)

target_link_libraries(gbt_tests
    PRIVATE
        mercaminer_gbt
)

add_executable(network_tests
    tests/network_tests.cpp
)

target_link_libraries(network_tests
    PRIVATE
        mercaminer_network
)

enable_testing()

add_test(
    NAME network_tests
    COMMAND network_tests
)

add_test(
    NAME gbt_tests
    COMMAND gbt_tests
)

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


add_library(mercaminer_block_primitives
    src/serialization.cpp
    src/hash256.cpp
    src/merkle.cpp
    src/crypto/sha256.cpp
)

target_include_directories(mercaminer_block_primitives
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(mercaminer_block_primitives
    PRIVATE
        DISABLE_OPTIMIZED_SHA256
)

target_link_libraries(mercaminer_block_primitives
    PUBLIC
        mercaminer_core
)

add_executable(block_primitives_tests
    tests/block_primitives_tests.cpp
)

target_link_libraries(block_primitives_tests
    PRIVATE
        mercaminer_block_primitives
)

add_test(
    NAME block_primitives_tests
    COMMAND block_primitives_tests
)


add_library(mercaminer_coinbase
    src/coinbase.cpp
)

target_include_directories(mercaminer_coinbase
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_coinbase
    PUBLIC
        mercaminer_block_primitives
)

add_executable(coinbase_tests
    tests/coinbase_tests.cpp
)

target_link_libraries(coinbase_tests
    PRIVATE
        mercaminer_coinbase
)

add_test(
    NAME coinbase_tests
    COMMAND coinbase_tests
)


add_library(mercaminer_block_builder
    src/block_builder.cpp
)

target_include_directories(mercaminer_block_builder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_block_builder
    PUBLIC
        mercaminer_coinbase
        mercaminer_gbt
        mercaminer_block_primitives
        mercaminer_core
)

add_executable(block_builder_tests
    tests/block_builder_tests.cpp
)

target_link_libraries(block_builder_tests
    PRIVATE
        mercaminer_block_builder
)

add_test(
    NAME block_builder_tests
    COMMAND block_builder_tests
)


add_library(mercaminer_scanner
    src/scanner.cpp
)

target_include_directories(mercaminer_scanner
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_scanner
    PUBLIC
        mercahash
        mercaminer_core
)

add_executable(scanner_tests
    tests/scanner_tests.cpp
)

target_link_libraries(scanner_tests
    PRIVATE
        mercaminer_scanner
)

add_test(
    NAME scanner_tests
    COMMAND scanner_tests
)


add_library(mercaminer_candidate_miner
    src/candidate_miner.cpp
)

target_include_directories(mercaminer_candidate_miner
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mercaminer_candidate_miner
    PUBLIC
        mercaminer_block_builder
        mercaminer_scanner
)

add_executable(candidate_miner_tests
    tests/candidate_miner_tests.cpp
)

target_link_libraries(candidate_miner_tests
    PRIVATE
        mercaminer_candidate_miner
)

add_test(
    NAME candidate_miner_tests
    COMMAND candidate_miner_tests
)


add_executable(mercaminer_mine_once
    src/mine_once.cpp
)

target_link_libraries(mercaminer_mine_once
    PRIVATE
        mercaminer_candidate_miner
        mercaminer_network
        mercaminer_rpc
)
