#include <iostream>
#include <algorithm>
#include "tavernmx/client.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;

int main() {
    ClientConfiguration config{"client-config.json"};

    ServerConnection connection{config.host_name, config.host_port};
    for (auto &cert: config.custom_certificates) {
        connection.load_certificate(cert);
    }
    connection.connect();

    std::cout << "Sending request" << std::endl;
    MessageBlock block{};
    block.payload.resize(2400);
    int32_t count = 0;
    std::transform(std::begin(block.payload), std::end(block.payload), std::begin(block.payload),
                   [&count](unsigned char c) {
                       count++;
                       return static_cast<unsigned char>(count % 255);
                   });
    block.payload_size = static_cast<int32_t>(block.payload.size());
    connection.send_message(block);

    return 0;
}