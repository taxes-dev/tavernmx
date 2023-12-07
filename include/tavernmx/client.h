#pragma once

#include <cstddef>
#include <string>
#include "ssl.h"

namespace tavernmx::client
{
    class ServerConnection {
    public:
        ServerConnection(const std::string &host_name, int32_t host_port);

        void load_certificate(const std::string &cert_path);

        void connect();

        void send_message(const messaging::MessageBlock &block);

    private:
        std::string host_name{};
        int32_t host_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };
}