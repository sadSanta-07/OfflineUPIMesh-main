cmake_minimum_required(VERSION 3.20)
project(upi_mesh_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(ASIO_ENABLE_OLD_SERVICES)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(include)

# Fetch Crow HTTP Framework 
include(FetchContent)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
FetchContent_MakeAvailable(Crow)

# 1. Cryptography Core
add_library(upi_crypto STATIC src/crypto/hybrid_crypto.cpp)
target_link_libraries(upi_crypto PUBLIC OpenSSL::Crypto nlohmann_json::nlohmann_json)

# 2. Services Core
add_library(upi_services STATIC src/service/idempotency.cpp)
target_link_libraries(upi_services PUBLIC upi_crypto Threads::Threads)

# 3. Database Core (NEW)
add_library(upi_db STATIC src/db/database.cpp)
target_link_libraries(upi_db PUBLIC SQLite3::SQLite3)

# 4. Web Server Executable
add_executable(upi_mesh_server src/main.cpp)
target_link_libraries(upi_mesh_server PRIVATE upi_services upi_crypto upi_db Crow::Crow Threads::Threads) # <-- Linked upi_db

# Link Windows Socket Libraries
if(WIN32)
    target_link_libraries(upi_mesh_server PRIVATE ws2_32 mswsock)
endif()