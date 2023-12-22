#include <iostream>
#include <thread>
#include <vector>

#include "tavernmx/logging.h"
#include "tavernmx/server.h"

using namespace tavernmx::messaging;
using namespace tavernmx::server;
using namespace std::string_literals;

void client_worker(std::shared_ptr<ClientConnection>&& client) {
    try {
        // Expect client to send HELLO as the first message
        auto hello = client->wait_for(MessageType::HELLO);
        if (hello) {
            std::cout << "Client connected: " << std::get<std::string>(hello.value().values["user_name"s]) << std::endl;
            // TODO: simplify message creation/packing
            auto ack = pack_messages({create_ack()});
            client->send_message(ack[0]);
        } else {
            std::cout << "No HELLO sent by client, disconnecting" << std::endl;
            return;
        }

        // Echo responses
        MessageBlock response{};
        while (auto block = client->receive_message()) {
            std::cout << "Got message block: " << block->payload_size << std::endl;
            std::string message{
                reinterpret_cast<char *>(block->payload.data()), static_cast<size_t>(block->payload_size)
            };
            std::cout << "Line: " << message << std::endl;

            std::string response_text{"You sent "};
            response_text += std::to_string(message.length());
            response_text += " characters";
            response.payload_size = static_cast<int32_t>(response_text.length());
            response.payload.resize(response_text.length());
            std::copy(std::begin(response_text), std::end(response_text), std::begin(response.payload));
            client->send_message(response);
        }
        std::cout << "Worker exiting." << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "Worker exited with exception:" << std::endl;
        std::cout << ex.what() << std::endl;
    }
}

int main() {
    try {
        signal(SIGPIPE, SIG_IGN);

        tavernmx::configure_logging(spdlog::level::warn, {});
        ServerConfiguration config{"server-config.json"};
        spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);
        TMX_INFO("Server starting.");

        ClientConnectionManager connections{config.host_port};
        connections.load_certificate(config.host_certificate_path, config.host_private_key_path);
        static auto sigint_handler = [&connections]() {
            TMX_WARN("Interrupt received.");
            connections.shutdown();
        };
        signal(SIGINT, [](int32_t) { sigint_handler(); });

        std::vector<std::thread> threads{};
        while (auto client = connections.await_next_connection()) {
            threads.emplace_back(client_worker, std::move(client.value()));

            // TODO: clean up dead threads
        }
        TMX_INFO("Server shutdown.");
        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Server shutdown unexpectedly.");
        return 1;
    }
}
