add_executable(tavernmx-client main.cpp serverconnection.cpp clientconfiguration.cpp ui.cpp)
target_link_libraries(tavernmx-client PRIVATE tavernmx-shared imgui::imgui nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto
    SDL2::SDL2main SDL2::SDL2-static)
target_include_directories(tavernmx-client PRIVATE
        "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx-client tavernmx-shared)