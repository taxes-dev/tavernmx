#pragma once
#include <string>
#include "ssl.h"

namespace tavernmx::server
{
    class ClientConnection
    {
    public:
        ClientConnection(ssl::ssl_unique_ptr<BIO> && client_bio)
        : bio{std::move(client_bio)} {};
        std::optional<messaging::MessageBlock> receive_message();
    private:
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };

    class ClientConnectionManager
    {
    public:
        ClientConnectionManager(int32_t accept_port);
        void load_certificate(const std::string& cert_path, const std::string& private_key_path);
        std::optional<ClientConnection> await_next_connection();
    private:
        int32_t accept_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> accept_bio{nullptr};
    };
}