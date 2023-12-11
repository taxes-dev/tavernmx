#pragma once

#include <optional>
#include <string>
#include "ssl.h"

namespace tavernmx::server {
    class ServerError : public std::exception {
    public:
        explicit ServerError(std::string what) noexcept: what_str{std::move(what)} {};

        explicit ServerError(const char *what) noexcept: what_str{what} {};

        ServerError(std::string what, const std::exception &inner) noexcept: what_str{
                std::move(what)} { this->what_str += std::string{", caused by: "} + inner.what(); };

        ServerError(const char *what, const std::exception &inner) noexcept: what_str{what} { this->what_str +=
                                                                                                      std::string{
                                                                                                              ", caused by: "} +
                                                                                                      inner.what();
        }

        const char *what() const noexcept { return this->what_str.c_str(); }

    private:
        std::string what_str{};
    };

    class ServerConfiguration {
    public:
        explicit ServerConfiguration(const std::string &config_path);

        int32_t host_port{};
        std::string log_level{};
        std::string log_file{};
        std::string host_certificate_path{};
        std::string host_private_key_path{};
    };

    class ClientConnection {
    public:
        explicit ClientConnection(ssl::ssl_unique_ptr<BIO> &&client_bio)
                : bio{std::move(client_bio)} {};

        ClientConnection(const ClientConnection &) = delete;

        ClientConnection(ClientConnection &&) = default;

        std::optional<messaging::MessageBlock> receive_message();

        void send_message(const messaging::MessageBlock &block);

    private:
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };

    class ClientConnectionManager {
    public:
        explicit ClientConnectionManager(int32_t accept_port);

        ClientConnectionManager(const ClientConnectionManager &) = delete;

        ClientConnectionManager(ClientConnectionManager &&) = default;

        void load_certificate(const std::string &cert_path, const std::string &private_key_path);

        std::optional<ClientConnection> await_next_connection();

        void shutdown();

    private:
        int32_t accept_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> accept_bio{nullptr};
    };
}