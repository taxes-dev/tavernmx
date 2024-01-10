add_executable(tavernmx-tests main.cpp ringbuffer.cpp util.cpp)
target_link_libraries(tavernmx-tests PRIVATE Catch2::Catch2WithMain tavernmx-shared)
target_include_directories(tavernmx-tests PRIVATE
        "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx-tests tavernmx-shared)