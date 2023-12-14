#include <iostream>
#include "tavernmx/client.h"
#include "tavernmx/logging.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace std::string_literals;

int main() {
    try {
        signal(SIGPIPE, SIG_IGN);

        ClientConfiguration config{"client-config.json"};

        spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);
        TMX_INFO("Client starting.");

        TMX_INFO("Connecting to {}:{} ...", config.host_name, config.host_port);
        ServerConnection connection{config.host_name, config.host_port};
        for (auto& cert: config.custom_certificates) {
            connection.load_certificate(cert);
        }
        connection.connect();
        static auto sigint_handler = [&connection]() {
            TMX_WARN("Interrupt received.");
            connection.shutdown();
        };
        signal(SIGINT, [](int32_t) { sigint_handler(); });

        TMX_INFO("Beginning chat loop.");
        std::string input{};
        std::string output{};
        MessageBlock block{};
        while (input != "/quit"s) {
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
            if (rcvd.has_value()) {
                output.resize(rcvd->payload_size);
                std::copy(std::begin(rcvd->payload), std::end(rcvd->payload), std::begin(output));
                std::cout << "Server: " << output << std::endl;
            }
        }
        if (!connection.is_connected()) {
            std::cout << "Connection to server lost" << std::endl;
        }

        TMX_INFO("Client shutdown.");

        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Client shutdown unexpectedly.");
        return 1;
    }
}
