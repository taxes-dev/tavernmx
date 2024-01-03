#include <fstream>
#include <nlohmann/json.hpp>
#include "tavernmx/client.h"

using json = nlohmann::json;

namespace tavernmx::client
{
    ClientConfiguration::ClientConfiguration(const std::string& config_path) {
        std::ifstream config_file{ config_path };
        if (!config_file.good()) {
            throw ClientError{ "Unable to open config file" };
        }
        try {
            auto config_data = json::parse(config_file);
            if (!config_data["server_host_name"].is_string()) {
                throw ClientError{ "server_host_name is required" };
            }
            this->host_name = config_data["server_host_name"];
            this->host_port = config_data.value("server_host_port", 8080);
            this->log_level = config_data.value("log_level", "warn"s);
            this->log_file = config_data.value("log_file", ""s);
            if (config_data["custom_certificates"].is_array()) {
                for (auto& cert : config_data["custom_certificates"]) {
                    this->custom_certificates.push_back(cert);
                }
            } else if (config_data["custom_certificates"].is_string()) {
                this->custom_certificates.push_back(config_data["custom_certificates"]);
            }
        } catch (json::parse_error& ex) {
            throw ClientError{ "Unable to parse config file", ex };
        }
    }

}
