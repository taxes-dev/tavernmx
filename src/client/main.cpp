#include <iostream>
#include <memory>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL.h>
#include "tavernmx/client.h"
#include "tavernmx/logging.h"
#include "tavernmx/client-ui.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace std::string_literals;

namespace {
    void setup_handlers(ClientUi&);
}

int main() {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<ServerConnection> connection{nullptr};

    static auto sigint_handler = [&connection]() {
        TMX_WARN("Interrupt received.");
        if (connection) {
            connection->shutdown();
        }
    };
    signal(SIGINT, [](int32_t) { sigint_handler(); });
    signal(SIGPIPE, SIG_IGN);

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
            TMX_ERR("Error: {}", SDL_GetError());
            return 1;
        }
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

        // Create window with SDL_Renderer graphics context
        window = SDL_CreateWindow("tavernmx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            TMX_ERR("Error creating SDL_Renderer!");
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
            if (ui.get_state() == ClientUiState::Connecting && !connection) {
                try {
                    std::string host_name = ui[ClientUiState::Connect]["host"];
                    int32_t host_port = std::stoi(ui[ClientUiState::Connect]["port"]);
                    TMX_INFO("Connecting to {}:{} ...", host_name, host_port);
                    connection = std::make_unique<ServerConnection>(host_name, host_port);
                    for (auto& cert: config.custom_certificates) {
                        connection->load_certificate(cert);
                    }
                    connection->connect();
                } catch (std::exception& ex) {
                    ui.set_state(ClientUiState::Connect);
                    connection.reset();
                    ui.set_error(std::string{ex.what()});
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

int main_old() {
    try {
        signal(SIGPIPE, SIG_IGN);

        ClientConfiguration config{"client-config.json"};

        spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);
        TMX_INFO("Client starting.");

        TMX_INFO("Connecting to {}:{} ...", config.host_name, config.host_port);
        ServerConnection connection{config.host_name, config.host_port};
        for (auto& cert: config.custom_certificates) {
            connection.load_certificate(cert);
        }
        connection.connect();
        static auto sigint_handler = [&connection]() {
            TMX_WARN("Interrupt received.");
            connection.shutdown();
        };
        signal(SIGINT, [](int32_t) { sigint_handler(); });

        TMX_INFO("Beginning chat loop.");
        std::string input{};
        std::string output{};
        MessageBlock block{};
        while (input != "/quit"s) {
            std::cout << "> ";
            std::getline(std::cin, input);
            if (!connection.is_connected()) {
                break;
            }
            if (input.length() > 0) {
                block.payload_size = static_cast<int32_t>(input.length());
                if (block.payload.size() < input.length()) {
                    block.payload.resize(input.length());
                }
                std::copy(std::begin(input), std::end(input), std::begin(block.payload));
                connection.send_message(block);
            }
            auto rcvd = connection.receive_message();
            if (rcvd.has_value()) {
                output.resize(rcvd->payload_size);
                std::copy(std::begin(rcvd->payload), std::end(rcvd->payload), std::begin(output));
                std::cout << "Server: " << output << std::endl;
            }
        }
        if (!connection.is_connected()) {
            std::cout << "Connection to server lost" << std::endl;
        }

        TMX_INFO("Client shutdown.");

        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Client shutdown unexpectedly.");
        return 1;
    }
}

namespace {
    void connect_connectbutton(ClientUi* ui) {
        TMX_INFO("Connect button pressed.");
        ui->set_state(ClientUiState::Connecting);
    }

    void connecting_cancelbutton(ClientUi* ui) {
        TMX_INFO("Cancel connecting button pressed.");
        ui->set_state(ClientUiState::Connect);
    }

    void setup_handlers(ClientUi& ui) {
        ui.add_handler(ClientUiMessage::Connect_ConnectButton, connect_connectbutton);
        ui.add_handler(ClientUiMessage::Connecting_CancelButton, connecting_cancelbutton);
    }
}
