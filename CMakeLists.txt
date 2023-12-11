cmake_minimum_required(VERSION 3.26)
project(tavernmx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_subdirectory(src/shared)
add_subdirectory(src/server)
add_subdirectory(src/client)