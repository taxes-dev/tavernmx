add_executable(tavernmx main.cpp clientconnection.cpp)
target_link_libraries(tavernmx PRIVATE tavernmx-shared OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(tavernmx PRIVATE
    "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx tavernmx-shared)