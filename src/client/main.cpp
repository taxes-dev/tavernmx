#include <csignal>
#include <iostream>
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

namespace
{
    constexpr int32_t WIN_WIDTH = 1280;
    constexpr int32_t WIN_HEIGHT = 720;

    std::binary_semaphore thread_signal{ 0 };

    std::string generate_random_username() {
        const uint32_t numbers = std::time(nullptr) & 0xfff;
        return "jdoe"s + std::to_string(numbers);
    };

    void begin_chat(std::unique_ptr<ServerConnection> connection, ChatWindowScreen* screen);
}

int main(int argv, char** argc) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
#ifndef TMX_WINDOWS
    std::signal(SIGPIPE, SIG_IGN);
#endif

    try {
        ClientConfiguration config{ "client-config.json" };

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
            return 1;
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
        auto client_ui = std::make_shared<ClientUi>();

        // create initial UI screen
        std::unique_ptr<ServerConnection> connection{ nullptr };
        auto connect_screen = std::make_unique<ConnectUiScreen>();
        connect_screen->user_name = generate_random_username();
        connect_screen->host_name = config.host_name;
        connect_screen->host_port = std::to_string(config.host_port);
        connect_screen->add_handler(ConnectUiScreen::MSG_CONNECT_BUTTON,
            [&connection,&config](ClientUi* ui, ClientUiScreen* screen) {
                TMX_INFO("Connect button pressed.");

                // setup "Connecting" screen
                auto connecting_screen = std::make_unique<ConnectingUiScreen>();
                connecting_screen->add_handler(ConnectingUiScreen::MSG_UPDATE,
                    [&connection](ClientUi* ui, ClientUiScreen* screen) {
                        // Poll to see if connection attempt is resolved
                        if (thread_signal.try_acquire()) {
                            const auto conn_screen = dynamic_cast<ConnectingUiScreen*>(screen);
                            ui->pop_screen();
                            if (conn_screen->is_cancelled()) {
                                // for simplicity, cancellation is handled here rather
                                // than trying to abort the connection thread
                                connection.reset();
                                ui->set_error("Connection cancelled."s);
                                TMX_INFO("Connection cancelled by user.");
                            } else if (connection && connection->is_connected()) {
                                TMX_INFO("Connected.");
                                auto chat_screen = std::make_unique<ChatWindowScreen>(connection->get_host_name());
                                begin_chat(std::move(connection), chat_screen.get());
                                ui->push_screen(std::move(chat_screen));
                            } else {
                                ui->set_error("Unable to connect to server."s);
                                TMX_ERR("Unable to connect to server.");
                            }
                        }
                    });
                ui->push_screen(std::move(connecting_screen));

                // Start trying to connect
                try {
                    const auto conn_screen = dynamic_cast<ConnectUiScreen*>(screen);
                    const int32_t host_port = std::stoi(conn_screen->host_port);
                    TMX_INFO("Connecting to {}:{} ...", conn_screen->host_name, host_port);
                    connection = std::make_unique<ServerConnection>(conn_screen->host_name, host_port);
                    for (auto& cert : config.custom_certificates) {
                        connection->load_certificate(cert);
                    }
                    // Connect on background thread so it doesn't block UI
                    std::thread connection_thread{
                        [&connection, user_name = conn_screen->user_name]() {
                            try {
                                connection->connect();
                                connection->send_message(create_hello(user_name));

                                if (!connection->wait_for(MessageType::ACK)) {
                                    TMX_ERR("Server did not acknowledge HELLO");
                                    connection->shutdown();
                                }

                                thread_signal.release();
                            } catch (std::exception& ex) {
                                TMX_ERR("connection_thread error: {}", ex.what());
                                thread_signal.release();
                            }
                        }
                    };
                    connection_thread.detach();
                } catch (std::exception& ex) {
                    connection.reset();
                    ui->pop_screen();
                    ui->set_error(std::string{ ex.what() });
                }

            });
        client_ui->push_screen(std::move(connect_screen));

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
                        client_ui->set_viewport_resized();
                    }
                    break;
                }
            }

            // Start ImGui frame
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Define ImGui controls
            bool show_demo_window = true;
            ImGui::ShowDemoWindow(&show_demo_window);

            client_ui->render();

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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

namespace
{
    constexpr std::chrono::seconds QUIET_TIMEOUT{ 30ll };
    std::binary_semaphore connection_ended_signal{ 0 };
    bool waiting_on_server{ false };

    void connection_worker(std::unique_ptr<ServerConnection> server) {
        try {
            auto last_message_received = std::chrono::system_clock::now();
            std::optional<std::chrono::time_point<std::chrono::system_clock>> heartbeat_sent{};

            while (server->is_connected()) {
                std::vector<Message> send_messages{};

                // 1. Read waiting messages on socket
                if (auto block = server->receive_message()) {
                    TMX_INFO("Receive message block: {} bytes", block->payload_size);
                    for (auto& msg : unpack_messages(block.value())) {
                        TMX_INFO("Receive message: {}", static_cast<int32_t>(msg.message_type));
                        switch (msg.message_type) {
                        case MessageType::HEARTBEAT:
                            // if server requests a HEARTBEAT, we can respond immediately
                            send_messages.push_back(create_ack());
                            break;
                        case MessageType::ACK:
                        case MessageType::NAK:
                            // outside of connection handshake, ACK/NAK can be ignored
                            break;
                        case MessageType::Invalid:
                            // programming error?
                            assert(false && "Received Invalid message type");
                            break;
                        default:
                            // anything else, queue it for processing
                            server->messages_in->push(std::move(msg));
                            break;
                        }
                        server->messages_in->push(std::move(msg));
                    };
                    last_message_received = std::chrono::system_clock::now();
                    heartbeat_sent.reset();
                    waiting_on_server = false;
                }

                // 2. Have we heard from the server lately? If not, send heartbeat.
                if ((std::chrono::system_clock::now() - last_message_received > QUIET_TIMEOUT)) {
                    if (!heartbeat_sent.has_value()) {
                        send_messages.push_back(create_heartbeat());
                        heartbeat_sent = std::chrono::system_clock::now();
                        waiting_on_server = true;
                    } else if (std::chrono::system_clock::now() - heartbeat_sent.value() > QUIET_TIMEOUT) {
                        TMX_INFO("Server did not respond to heartbeat.");
                        break;
                    }
                }

                // 3. Send queued messages to socket
                while (auto msg = server->messages_out->pop()) {
                    TMX_INFO("Send message: {}", static_cast<int32_t>(msg->message_type));
                    send_messages.push_back(std::move(msg.value()));
                }
                server->send_messages(std::cbegin(send_messages), std::cend(send_messages));

                // 3. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{ 100ll });
            }
            TMX_INFO("Connection worker exiting.");
        } catch (std::exception& ex) {
            TMX_ERR("Connection worker exited with exception: {}", ex.what());
        }
        connection_ended_signal.release();
    }

    void begin_chat(std::unique_ptr<ServerConnection> connection, ChatWindowScreen* screen) {
        auto messages_in = connection->messages_in;
        auto messages_out = connection->messages_out;

        // update loop to handle incoming messages
        screen->add_handler(ChatWindowScreen::MSG_UPDATE,
            [messages_in, messages_out](ClientUi* ui, ClientUiScreen* screen) {
                // are we still connected?
                if (connection_ended_signal.try_acquire()) {
                    ui->pop_screen();
                    ui->set_error("Connection to server lost.");
                    return;
                }
                auto chat_screen = dynamic_cast<ChatWindowScreen*>(screen);

                // check server status
                chat_screen->waiting_on_server = waiting_on_server;

                // process incoming messages
                while (auto msg = messages_in->pop()) {
                    TMX_INFO("UI message: {}", static_cast<int32_t>(msg->message_type));
                }
            });

        // push connection to background thread for message handling
        std::thread connection_thread{ connection_worker, std::move(connection) };
        connection_thread.detach();
    }
}
