#include <semaphore>
#include "tavernmx/client-workers.h"

using namespace tavernmx::messaging;

namespace
{
    /// Signals that the attempt to connect has been completed.
    std::binary_semaphore connect_thread_signal{ 0 };
    /// Error message from the connection thread.
    std::string connect_thread_error{};

    /// Generates a random username for ease of use.
    std::string generate_random_username() {
        const uint32_t numbers = std::time(nullptr) & 0xfff;
        return "jdoe"s + std::to_string(numbers);
    };

    /// Holds on to the server connection until it's passed off to the chat worker.
    std::unique_ptr<tavernmx::client::ServerConnection> connection{ nullptr };
}

namespace tavernmx::client
{
    void connecting_screen_update(ClientUi* ui, ClientUiScreen* screen) {
        // Poll to see if connection attempt is resolved
        if (connect_thread_signal.try_acquire()) {
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
                auto chat_screen = std::make_unique<ChatWindowScreen>(connection->get_host_name(),
                    connection->get_user_name());
                chat_window_worker(std::move(connection), chat_screen.get());
                ui->push_screen(std::move(chat_screen));
            } else {
                if (connect_thread_error.empty()) {
                    ui->set_error("Unable to connect to server."s);
                } else {
                    ui->set_error(connect_thread_error);
                    connect_thread_error.clear();
                }
                TMX_ERR("Unable to connect to server.");
            }
        }
    }

    void connect_screen_worker(ConnectUiScreen* connect_screen, const ClientConfiguration& config) {
        connect_screen->user_name = generate_random_username();
        connect_screen->host_name = config.host_name;
        connect_screen->host_port = std::to_string(config.host_port);
        connect_screen->add_handler(ConnectUiScreen::MSG_CONNECT_BUTTON,
            [&config](ClientUi* ui, ClientUiScreen* screen) {
                TMX_INFO("Connect button pressed.");

                // Start trying to connect
                try {
                    const auto conn_screen = dynamic_cast<ConnectUiScreen*>(screen);
                    const int32_t host_port = std::stoi(conn_screen->host_port);
                    TMX_INFO("Connecting to {}:{} ...", conn_screen->host_name, host_port);
                    connection = std::make_unique<ServerConnection>(conn_screen->host_name, host_port, conn_screen->user_name);
                    for (auto& cert : config.custom_certificates) {
                        connection->load_certificate(cert);
                    }
                    // Connect on background thread so it doesn't block UI
                    std::thread connection_thread{
                        []() {
                            try {
                                connection->connect();
                                connection->send_message(create_hello(connection->get_user_name()));

                                if (const auto acknak = connection->wait_for_ack_or_nak()) {
                                    if (acknak->message_type == MessageType::NAK) {
                                        auto nakmsg = message_value_or<std::string>(*acknak, "error"s);
                                        TMX_WARN("Server denied request to connect: {}", nakmsg);
                                        connect_thread_error = std::move(nakmsg);
                                        connection->shutdown();
                                    }
                                } else {
                                    TMX_ERR("Server did not acknowledge HELLO");
                                    connection->shutdown();
                                }

                                connect_thread_signal.release();
                            } catch (std::exception& ex) {
                                TMX_ERR("connection_thread error: {}", ex.what());
                                connection->shutdown();
                                connect_thread_signal.release();
                            }
                        } };
                    connection_thread.detach();

                    // setup "Connecting" screen
                    auto connecting_screen = std::make_unique<ConnectingUiScreen>();
                    connecting_screen->add_handler(ConnectingUiScreen::MSG_UPDATE,
                        connecting_screen_update);
                    ui->push_screen(std::move(connecting_screen));
                } catch (std::exception& ex) {
                    connection.reset();
                    ui->set_error(std::string{ ex.what() });
                }
            });
    }
}
