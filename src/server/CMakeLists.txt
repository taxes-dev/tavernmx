add_executable(tavernmx main.cpp clientconnection.cpp serverconfiguration.cpp)
target_link_libraries(tavernmx PRIVATE tavernmx-shared OpenSSL::SSL OpenSSL::Crypto spdlog::spdlog)
target_include_directories(tavernmx PRIVATE
    "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx tavernmx-shared)
if(WIN32)
target_link_libraries(tavernmx-shared PRIVATE ws2_32)
endif()