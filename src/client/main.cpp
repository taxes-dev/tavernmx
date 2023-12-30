#include <csignal>
#include <iostream>
#include <memory>
#include <semaphore>
#include <thread>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL.h>
#include "tavernmx/client.h"
#include "tavernmx/client-ui.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace std::string_literals;

namespace {
    constexpr int32_t WIN_WIDTH = 1280;
    constexpr int32_t WIN_HEIGHT = 720;

    std::binary_semaphore thread_signal{0};

    void setup_handlers(ClientUi&);

    std::string generate_random_username() {
        const uint32_t numbers = std::time(nullptr) & 0xfff;
        return "jdoe"s + std::to_string(numbers);
    };
}

int main() {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<ServerConnection> connection{nullptr};

    std::signal(SIGPIPE, SIG_IGN);

    try {
        ClientConfiguration config{"client-config.json"};

        spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);
        TMX_INFO("Client starting.");

        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            TMX_ERR("SDL Error: {}", SDL_GetError());
            return 1;
        }
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

        // Create window with SDL_Renderer graphics context
        window = SDL_CreateWindow("tavernmx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (window == nullptr) {
            TMX_ERR("SDL Error creating SDL_Window: {}", SDL_GetError());
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            TMX_ERR("SDL Error creating SDL_Renderer: {}", SDL_GetError());
            return 1;
        }
        SDL_RendererInfo info{};
        SDL_GetRendererInfo(renderer, &info);
        TMX_INFO("Current SDL_Renderer: {}", info.name);

        // Create ImGUI context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // setup UI handlers
        bool done = false;
        ClientUi ui{};
        setup_handlers(ui);

        // insert initial state
        ui[ClientUiState::Connect]["user"] = generate_random_username();
        ui[ClientUiState::Connect]["host"] = config.host_name;
        ui[ClientUiState::Connect]["port"] = std::to_string(config.host_port);

        // Main loop
        while (!done) {
            // Poll events
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                switch (event.type) {
                    case SDL_QUIT:
                        done = true;
                        break;
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                            done = true;
                        }
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            ui.set_viewport_resized();
                        }
                        break;
                }
            }

            // Handle current UI state
            if (ui.get_state() == ClientUiState::Connect && connection) {
                // Drop current connection if it exists
                connection.reset();
            } else if (ui.get_state() == ClientUiState::Connecting && !connection) {
                // Start trying to connect
                try {
                    std::string user_name = ui[ClientUiState::Connect]["user"];
                    std::string host_name = ui[ClientUiState::Connect]["host"];
                    int32_t host_port = std::stoi(ui[ClientUiState::Connect]["port"]);
                    TMX_INFO("Connecting to {}:{} ...", host_name, host_port);
                    connection = std::make_unique<ServerConnection>(host_name, host_port);
                    for (auto& cert: config.custom_certificates) {
                        connection->load_certificate(cert);
                    }
                    // Connect on background thread so it doesn't block UI
                    std::thread connection_thread{
                        [&ui, &connection, &user_name]() {
                            try {
                                connection->connect();

                                Message msg = create_hello(user_name);
                                auto msgs = pack_messages({msg});
                                connection->send_messages(std::begin(msgs), std::end(msgs));

                                if (!connection->wait_for(MessageType::ACK)) {
                                    TMX_ERR("Server did not acknowledge HELLO");
                                    connection->shutdown();
                                }

                                thread_signal.release();
                            } catch (std::exception& ex) {
                                TMX_ERR("connection_thread error: {}", ex.what());
                                connection.reset();
                                ui.set_state(ClientUiState::Connect);
                                ui.set_error("Unable to connect: "s + ex.what());
                                thread_signal.release();
                            }
                        }
                    };
                    connection_thread.detach();
                } catch (std::exception& ex) {
                    ui.set_state(ClientUiState::Connect);
                    connection.reset();
                    ui.set_error(std::string{ex.what()});
                }
            } else if (ui.get_state() == ClientUiState::Connecting) {
                // Poll to see if connection attempt is resolved
                if (thread_signal.try_acquire()) {
                    if (!ui[ClientUiState::Connecting]["cancelled"].empty()) {
                        connection.reset();
                        ui.set_error("Connection cancelled."s);
                        ui.set_state(ClientUiState::Connect);
                    } else if (connection->is_connected()) {
                        ui[ClientUiState::ChatWindow]["host"] = connection->get_host_name();
                        ui.set_state(ClientUiState::ChatWindow);
                    } else {
                        connection.reset();
                        ui.set_error("Unable to connect to server."s);
                        ui.set_state(ClientUiState::Connect);
                    }
                }
            } else if (ui.get_state() == ClientUiState::ChatWindow) {
                // Check for messages
                if (auto block = connection->receive_message()) {
                    for (auto& msg: unpack_messages(block.value())) {
                        TMX_INFO("Received message: {}", static_cast<int32_t>(msg.message_type));
                        // TODO
                    }
                }
            }

            // Start ImGui frame
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Define ImGui controls
            bool show_demo_window = true;
            ImGui::ShowDemoWindow(&show_demo_window);

            ui.render();

            // Render and present
            ImGui::Render();
            SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer);
        }
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Client shutdown unexpectedly.");
        return 1;
    }

    // Shutdown
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();

    return 0;
}

namespace {
    void connect_connectbutton(ClientUi* ui) {
        TMX_INFO("Connect button pressed.");
        ui->set_state(ClientUiState::Connecting);
        (*ui)[ClientUiState::Connecting]["cancelled"] = ""s;
    }

    void connecting_cancelbutton(ClientUi* ui) {
        TMX_INFO("Cancel connecting button pressed.");
        (*ui)[ClientUiState::Connecting]["cancelled"] = "1"s;
    }

    void setup_handlers(ClientUi& ui) {
        ui.add_handler(ClientUiMessage::Connect_ConnectButton, connect_connectbutton);
        ui.add_handler(ClientUiMessage::Connecting_CancelButton, connecting_cancelbutton);
    }
}
