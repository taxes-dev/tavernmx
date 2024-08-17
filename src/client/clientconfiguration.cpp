#include <fstream>
#include <nlohmann/json.hpp>
#include "tavernmx/client.h"

using json = nlohmann::json;

namespace tavernmx::client
{
    ClientConfiguration::ClientConfiguration(std::string_view config_path) {
        std::ifstream config_file{ config_path };
        if (!config_file.good()) {
            throw ClientError{ "Unable to open config file" };
        }
        try {
            const json config_data = json::parse(config_file);
            if (!config_data["server_host_name"].is_string()) {
                throw ClientError{ "server_host_name is required" };
            }
            this->host_name = config_data["server_host_name"];
            this->host_port = config_data.value("server_host_port", 8080);
            this->log_level = config_data.value("log_level", "warn"s);
            std::string log_file = config_data.value("log_file", ""s);
            if (!log_file.empty()) {
                this->log_file = {std::move(log_file)};
            }
            if (config_data["custom_certificates"].is_array()) {
                for (const json& cert : config_data["custom_certificates"]) {
                    this->custom_certificates.push_back(cert);
                }
            } else if (config_data["custom_certificates"].is_string()) {
                this->custom_certificates.push_back(config_data["custom_certificates"]);
            }
            if (config_data["custom_font"].is_object()) {
                const json& font_data = config_data["custom_font"];
                this->custom_font.font_size = font_data.value("font_size", 12u);
                this->custom_font.en = font_data.value("en", ""s);
                this->custom_font.jp = font_data.value("jp", ""s);
                this->custom_font.kr = font_data.value("kr", ""s);
                this->custom_font.cn = font_data.value("cn", ""s);
            }
        } catch (json::parse_error& ex) {
            throw ClientError{ "Unable to parse config file", ex };
        }
    }

}
