add_executable(tavernmx-client main.cpp serverconnection.cpp)
target_link_libraries(tavernmx-client PRIVATE tavernmx-shared OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(tavernmx-client PRIVATE
        "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx-client tavernmx-shared)