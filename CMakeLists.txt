cmake_minimum_required(VERSION 3.22)

project(EvChargingPlatform VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network Sql Widgets Test)

enable_testing()

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/user_client)
add_subdirectory(src/admin_client)
add_subdirectory(tests)
