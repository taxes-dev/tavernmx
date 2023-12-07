#include <iostream>
#include <vector>
#include "tavernmx/server.h"

using namespace std::string_literals;
using namespace tavernmx::server;
using namespace tavernmx::messaging;

int main() {
    signal(SIGPIPE, SIG_IGN);

    ClientConnectionManager connections{8080};
    connections.load_certificate("server-certificate.pem"s, "server-private-key.pem"s);

    while (auto client = connections.await_next_connection()) {
        try {
            if (auto block = client->receive_message())
            {
                std::cout << "Got message block: " << block->payload_size << std::endl;
                for (unsigned char c : block->payload)
                {
                    std::cout << "Byte: " << static_cast<int32_t>(c) << std::endl;
                }
            }
            else
            {
                std::cout << "No message block found." << std::endl;
            }
        } catch (const std::exception &ex) {
            std::cout << "Worker exited with exception:" << std::endl;
            std::cout << ex.what() << std::endl;
        }
    }
    std::cout << "Exit!" << std::endl;
}

