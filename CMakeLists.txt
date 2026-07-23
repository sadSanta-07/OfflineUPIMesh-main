# Root build configuration script

cmake_minimum_required(VERSION 3.20)
project(upi_mesh_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL and nlohmann_json
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include path for project headers
include_directories(include)

# Cryptography Core Source Files
add_library(upi_crypto STATIC
    src/crypto/hybrid_crypto.cpp
)

target_link_libraries(upi_crypto PUBLIC OpenSSL::Crypto nlohmann_json::nlohmann_json)

# Executable for running quick sanity checks / tests
add_executable(crypto_test tests/test_crypto.cpp)
target_link_libraries(crypto_test PRIVATE upi_crypto)