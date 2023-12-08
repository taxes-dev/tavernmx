#include <iostream>
#include <thread>
#include <vector>
#include "tavernmx/server.h"

using namespace tavernmx::messaging;
using namespace tavernmx::server;

void client_worker(ClientConnection client)
{
    try {
        MessageBlock response{};
        while (auto block = client.receive_message()) {
            std::cout << "Got message block: " << block->payload_size << std::endl;
            std::string message{reinterpret_cast<char *>(block->payload.data()), static_cast<size_t>(block->payload_size)};
            std::cout << "Line: " << message << std::endl;

            std::string response_text{"You sent "};
            response_text += std::to_string(message.length());
            response_text += " characters";
            response.payload_size = static_cast<int32_t>(response_text.length());
            response.payload.resize(response_text.length());
            std::copy(std::begin(response_text), std::end(response_text), std::begin(response.payload));
            client.send_message(response);
        }
    } catch (const std::exception &ex) {
        std::cout << "Worker exited with exception:" << std::endl;
        std::cout << ex.what() << std::endl;
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    ServerConfiguration config{"server-config.json"};

    ClientConnectionManager connections{config.host_port};
    connections.load_certificate(config.host_certificate_path, config.host_private_key_path);

    std::vector<std::thread> threads{};
    while (auto client = connections.await_next_connection()) {
        threads.emplace_back(client_worker, std::move(client.value()));

        // TODO: clean up dead threads
    }
    std::cout << "Exit!" << std::endl;
}

