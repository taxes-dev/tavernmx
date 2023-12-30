#include <chrono>
#include <csignal>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "tavernmx/logging.h"
#include "tavernmx/room.h"
#include "tavernmx/server.h"

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;
using namespace tavernmx::server;
using namespace std::string_literals;

namespace {
    /// Signals that the server work thread is ready to receive data.
    std::binary_semaphore server_ready_signal{0};
    /// Signals that the main server thread has started accepting TLS connections.
    std::binary_semaphore server_accept_signal{0};
    /// Signals that the server work thread would like the main server thread to shutdown.
    std::binary_semaphore server_shutdown_signal{0};

    void server_worker(const ServerConfiguration& config, std::shared_ptr<ClientConnectionManager> connections) {
        try {
            RoomManager rooms{};
            TMX_INFO("Server worker starting.");

            TMX_INFO("Creating initial rooms ...");
            for (auto& room_name: config.initial_rooms) {
                TMX_INFO("#{}", room_name);
                rooms.create_room(room_name);
            }
            TMX_INFO("All rooms created.");

            // Server work thread is ready, wait for main thread to start accepting connections.
            server_ready_signal.release();
            server_accept_signal.acquire();

            TMX_INFO("Server work loop starting ...");
            while (connections->is_accepting_connections()) {
                // Step 1a. Gather all messages from clients
                auto clients = connections->get_active_connections();
                // Step 1b. Distribute events to appropriate rooms
                // Step 2a. Gather events from rooms
                // Step 2b. Distribute to clients in those rooms
                for (auto& client: clients) {
                    client->messages_out.push(create_ack());
                }
                // Step 3. Clean up
                // Step 4. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{100ll});
            }

            TMX_INFO("Server worker exiting.");
            server_shutdown_signal.release();
        } catch (std::exception& ex) {
            TMX_ERR("Server worker exited with exception: {}", ex.what());
            server_shutdown_signal.release();
        }
    }

    void client_worker(std::shared_ptr<ClientConnection>&& client) {
        try {
            // Expect client to send HELLO as the first message
            auto hello = client->wait_for(MessageType::HELLO);
            if (hello) {
                TMX_INFO("Client connected: {}", std::get<std::string>(hello.value().values["user_name"s]));
                // TODO: simplify message creation/packing
                auto ack = pack_messages({create_ack()});
                client->send_message(ack[0]);
            } else {
                TMX_INFO("No HELLO sent by client, disconnecting.");
                return;
            }

            // Serialize messages back and forth from client
            while (client->is_connected()) {
                // 1. Read waiting messages on socket
                if (auto block = client->receive_message()) {
                    TMX_INFO("Receive message block: {} bytes", block->payload_size);
                    for (auto& msg: unpack_messages(block.value())) {
                        TMX_INFO("Receive message: {}", static_cast<int32_t>(msg.message_type));
                        client->messages_in.push(std::move(msg));
                    };
                }

                // 2. Send queued messages to socket
                std::vector<Message> send_messages{};
                while (auto msg = client->messages_out.pop()) {
                    TMX_INFO("Send message: {}", static_cast<int32_t>(msg->message_type));
                    send_messages.push_back(std::move(msg.value()));
                }
                // TODO: simplify message creation/packing
                auto send_blocks = pack_messages(send_messages);
                for (auto& block: send_blocks) {
                    TMX_INFO("Send message block: {} bytes", block.payload_size);
                    client->send_message(block);
                }

                // 3. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{100ll});
            }
            TMX_INFO("Client worker exiting.");
        } catch (const std::exception& ex) {
            TMX_ERR("Client worker exited with exception: {}", ex.what());
        }
    }
}

int main() {
    try {
        std::signal(SIGPIPE, SIG_IGN);

        tavernmx::configure_logging(spdlog::level::warn, {});
        TMX_INFO("Loading configuration ...");
        const ServerConfiguration config{"server-config.json"};
        const spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);

        TMX_INFO("Configuration loaded. Server starting ...");

        auto connections = std::make_shared<ClientConnectionManager>(config.host_port);
        connections->load_certificate(config.host_certificate_path, config.host_private_key_path);
        std::weak_ptr wk_connections = connections;
        static auto sigint_handler = [&wk_connections]() {
            TMX_WARN("Interrupt received.");
            if (auto connections = wk_connections.lock()) {
                connections->shutdown();
            }
        };
        std::signal(SIGINT, [](int32_t) { sigint_handler(); });

        // start server worker & wait for it to be ready
        std::thread server_thread{server_worker, config, connections};
        server_ready_signal.acquire();

        connections->begin_accept();
        server_accept_signal.release();

        TMX_INFO("Accepting connections ...");
        std::vector<std::thread> threads{};
        while (!server_shutdown_signal.try_acquire() && connections->is_accepting_connections()) {
            if (auto client = connections->await_next_connection()) {
                threads.emplace_back(client_worker, std::move(client.value()));

                // TODO: clean up dead threads
            }
        }

        TMX_INFO("Waiting for server worker thread ...");
        server_thread.join();

        TMX_INFO("Server shutdown.");
        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Server shutdown unexpectedly.");
        return 1;
    }
}
