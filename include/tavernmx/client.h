#pragma once

#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <vector>
#include "ssl.h"

namespace tavernmx::client
{
    class ClientError : public std::exception {
    public:
        explicit ClientError(std::string what) noexcept : what_str{std::move(what)} {};
        explicit ClientError(const char * what) noexcept : what_str{what} {};
        ClientError(std::string what, const std::exception &inner) noexcept : what_str{std::move(what)} { this->what_str += std::string{", caused by: "} + inner.what(); };
        ClientError(const char * what, const std::exception &inner) noexcept : what_str{what} { this->what_str += std::string{", caused by: "} + inner.what(); }

        const char * what() const noexcept { return this->what_str.c_str(); }
    private:
        std::string what_str{};
    };

    class ClientConfiguration {
    public:
        explicit ClientConfiguration(const std::string& config_path);
        std::string host_name{};
        int32_t host_port{};
        std::vector<std::string> custom_certificates{};
    };

    class ServerConnection {
    public:
        ServerConnection(const std::string &host_name, int32_t host_port);

        void load_certificate(const std::string &cert_path);

        void connect();

        std::optional<messaging::MessageBlock> receive_message();

        void send_message(const messaging::MessageBlock &block);

    private:
        std::string host_name{};
        int32_t host_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };
}