#include <iostream>
#include <algorithm>
#include "tavernmx/client.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace std::string_literals;

int main() {
    ClientConfiguration config{"client-config.json"};

    ServerConnection connection{config.host_name, config.host_port};
    for (auto &cert: config.custom_certificates) {
        connection.load_certificate(cert);
    }
    connection.connect();

    std::string input{};
    std::string output{};
    MessageBlock block{};
    while (input != "/quit"s)
    {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (!connection.is_connected()) {
            break;
        }
        if (input.length() > 0) {
            block.payload_size = static_cast<int32_t>(input.length());
            block.payload.resize(input.length());
            std::copy(std::begin(input), std::end(input), std::begin(block.payload));
            connection.send_message(block);
        }
        auto rcvd = connection.receive_message();
        if (rcvd.has_value())
        {
            output.resize(rcvd->payload_size);
            std::copy(std::begin(rcvd->payload), std::end(rcvd->payload), std::begin(output));
            std::cout << "Server: " << output << std::endl;
        }
    }
    if (!connection.is_connected()) {
        std::cout << "Connection to server lost" << std::endl;
    }

    return 0;
}