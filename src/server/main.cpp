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
    std::binary_semaphore server_ready_signal{0};
    std::binary_semaphore server_shutdown_signal{0};

    void server_worker(const ServerConfiguration& config) {
        try {
            RoomManager rooms{};
            TMX_INFO("Server worker starting.");

            TMX_INFO("Creating initial rooms ...");
            for (auto& room_name: config.initial_rooms) {
                TMX_INFO("#{}", room_name);
                rooms.create_room(room_name);
            }
            TMX_INFO("All rooms created.");

            server_ready_signal.release();

            //server_shutdown_signal.release();
            TMX_INFO("Server worker exiting.");
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

            // Echo responses
            while (client->is_connected()) {
                if (auto block = client->receive_message()) {
                    TMX_INFO("Got message block: {} bytes", block->payload_size);
                }
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
        std::thread server_thread{server_worker, config};

        ClientConnectionManager connections{config.host_port};
        connections.load_certificate(config.host_certificate_path, config.host_private_key_path);
        static auto sigint_handler = [&connections]() {
            TMX_WARN("Interrupt received.");
            connections.shutdown();
        };
        std::signal(SIGINT, [](int32_t) { sigint_handler(); });

        // wait for server worker to be ready
        server_ready_signal.acquire();

        TMX_INFO("Accepting connections ...");
        std::vector<std::thread> threads{};
        while (!server_shutdown_signal.try_acquire()) {
            if (auto client = connections.await_next_connection()) {
                threads.emplace_back(client_worker, std::move(client.value()));

                // TODO: clean up dead threads
            }
        }
        TMX_INFO("Server shutdown.");
        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Server shutdown unexpectedly.");
        return 1;
    }
}
