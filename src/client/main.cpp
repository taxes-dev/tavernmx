#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include "tavernmx/client.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace std::string_literals;
namespace {
    inline const std::string HOST_NAME{"localhost"};
    inline const int32_t HOST_PORT = 8080;
}

int main() {
    ServerConnection connection{HOST_NAME, HOST_PORT};
    connection.load_certificate("server-certificate.pem");
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