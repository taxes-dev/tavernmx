add_executable(tavernmx main.cpp
        ../shared/ssl.cpp)
target_link_libraries(tavernmx PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(tavernmx PRIVATE
    "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx tavernmx-shared)