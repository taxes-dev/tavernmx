add_executable(tavernmx-client main.cpp serverconnection.cpp clientconfiguration.cpp
        client-ui/ui.cpp client-ui/screen.cpp client-ui/connect.cpp client-ui/connecting.cpp
        client-ui/chatwindow.cpp
        workers/serverconnection-worker.cpp workers/chatwindow-worker.cpp workers/connectscreen-worker.cpp)
target_link_libraries(tavernmx-client PRIVATE tavernmx-shared imgui::imgui nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto
    SDL2::SDL2main $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)
target_include_directories(tavernmx-client PRIVATE
        "${PROJECT_SOURCE_DIR}/include")
add_dependencies(tavernmx-client tavernmx-shared)
if(WIN32)
target_link_libraries(tavernmx-shared PRIVATE ws2_32)
endif()
