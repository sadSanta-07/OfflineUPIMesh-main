cmake_minimum_required(VERSION 3.20)
project(upi_mesh_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

include_directories(include)

# Cryptography Core
add_library(upi_crypto STATIC
    src/crypto/hybrid_crypto.cpp
)
target_link_libraries(upi_crypto PUBLIC OpenSSL::Crypto nlohmann_json::nlohmann_json)

# Services Core
add_library(upi_services STATIC
    src/service/idempotency.cpp
)
target_link_libraries(upi_services PUBLIC upi_crypto Threads::Threads)

# Test Targets
add_executable(crypto_test tests/test_crypto.cpp)
target_link_libraries(crypto_test PRIVATE upi_crypto Threads::Threads)

add_executable(idempotency_test tests/test_idempotency.cpp)
target_link_libraries(idempotency_test PRIVATE upi_services Threads::Threads)