add_library(tavernmx-shared STATIC logging.cpp messaging.cpp ssl.cpp)
target_link_libraries(tavernmx-shared PRIVATE OpenSSL::SSL OpenSSL::Crypto spdlog::spdlog)
target_include_directories(tavernmx-shared PRIVATE
        "${PROJECT_SOURCE_DIR}/include")