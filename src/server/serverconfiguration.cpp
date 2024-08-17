#include <fstream>
#include <nlohmann/json.hpp>
#include "tavernmx/server.h"

using json = nlohmann::json;

namespace tavernmx::server
{
	ServerConfiguration::ServerConfiguration(std::string_view config_path) {
		std::ifstream config_file{ config_path };
		if (!config_file.good()) {
			throw ServerError{ "Unable to open config file" };
		}
		try {
			const json config_data = json::parse(config_file);
			this->host_port = config_data.value("host_port", 8080);
			this->log_level = config_data.value("log_level", "warn"s);
			std::string log_file = config_data.value("log_file", ""s);
			if (!log_file.empty()) {
				this->log_file = { std::move(log_file) };
			}
			if (!config_data["host_certificate"].is_string()) {
				throw ServerError{ "host_certificate is required" };
			}
			this->host_certificate_path = config_data["host_certificate"];
			if (!config_data["host_private_key"].is_string()) {
				throw ServerError{ "host_private_key is required" };
			}
			this->host_private_key_path = config_data["host_private_key"];
			this->max_clients = config_data.value("max_clients", 10);
			if (config_data["initial_rooms"].is_array()) {
				for (const auto& room : config_data["initial_rooms"].items()) {
					this->initial_rooms.push_back(room.value());
				}
			}
		} catch (json::parse_error& ex) {
			throw ServerError{ "Unable to parse config file", ex };
		}
	}

}
