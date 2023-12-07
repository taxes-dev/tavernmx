#include <fstream>
#include <nlohmann/json.hpp>
#include "tavernmx/server.h"

using json = nlohmann::json;
using namespace std::string_literals;

namespace tavernmx::server {
    ServerConfiguration::ServerConfiguration(const std::string &config_path) {
        std::ifstream config_file{config_path};
        if (!config_file.good()) {
            throw ServerError{"Unable to open config file"};
        }
        try {
            auto config_data = json::parse(config_file);
            this->host_port = config_data.value("host_port", 8080);
            if (!config_data["host_certificate"].is_string()) {
                throw ServerError{"host_certificate is required"};
            }
            this->host_certificate_path = config_data["host_certificate"];
            if (!config_data["host_private_key"].is_string()) {
                throw ServerError{"host_private_key is required"};
            }
            this->host_private_key_path = config_data["host_private_key"];
        }
        catch (json::parse_error &ex) {
            throw ServerError{"Unable to parse config file", ex};
        }
    }

}