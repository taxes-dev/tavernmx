add_library(tavernmx-shared STATIC ssl.cpp)
target_link_libraries(tavernmx-shared PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(tavernmx-shared PRIVATE
        "${PROJECT_SOURCE_DIR}/include")