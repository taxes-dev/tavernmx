add_library(tavernmx-shared STATIC connection.cpp logging.cpp messaging.cpp room.cpp ssl.cpp util.cpp)
target_link_libraries(tavernmx-shared PRIVATE OpenSSL::SSL OpenSSL::Crypto spdlog::spdlog)
target_include_directories(tavernmx-shared PRIVATE
        "${PROJECT_SOURCE_DIR}/include")
if(WIN32)
target_link_libraries(tavernmx-shared PRIVATE ws2_32)
endif()