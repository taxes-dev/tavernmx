cmake_minimum_required(VERSION 3.26)
project(tavernmx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
if(WIN32)
add_compile_options(/utf-8)
endif()

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_subdirectory(src/shared)
add_subdirectory(src/server)
add_subdirectory(src/client)
add_subdirectory(tests)