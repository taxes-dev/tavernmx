#include <libc.h>
#include "tavernmx/server.h"

using namespace tavernmx::ssl;

namespace {
    tavernmx::server::ServerError ssl_errors_to_exception(const char *message) {
        char buffer[256];
        std::string msg{message};
        while (const unsigned long err = ERR_get_error() != 0) {
            ERR_error_string_n(err, buffer, sizeof(buffer));
            msg += ", ";
            msg += buffer;
        }
        return tavernmx::server::ServerError{msg};
    }

}

namespace tavernmx::server
{
    ClientConnectionManager::ClientConnectionManager(int32_t accept_port)
    : accept_port{accept_port}
    {
        SSL_load_error_strings();
        this->ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_method()));
        SSL_CTX_set_min_proto_version(this->ctx.get(), TLS1_2_VERSION);
    }

    void ClientConnectionManager::load_certificate(const std::string& cert_path, const std::string& private_key_path)
    {
        if (SSL_CTX_use_certificate_file(this->ctx.get(), cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw ssl_errors_to_exception("Error loading server certificate");
        }
        if (SSL_CTX_use_PrivateKey_file(this->ctx.get(), private_key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw ssl_errors_to_exception("Error loading server private key");
        }
    }

    std::optional<ClientConnection> ClientConnectionManager::await_next_connection()
    {
        if (this->accept_bio == nullptr)
        {
            std::string host_port = std::to_string(this->accept_port);
            this->accept_bio = ssl_unique_ptr<BIO>(BIO_new_accept(host_port.c_str()));
            BIO_set_nbio(this->accept_bio.get(), 1);
            if (BIO_do_accept(this->accept_bio.get()) != 1) {
                throw ssl_errors_to_exception("Error in BIO_do_accept");
            }
        }

        auto bio = accept_new_tcp_connection(this->accept_bio.get());
        if (bio == nullptr)
        {
            return {};
        }
        bio = std::move(bio)
              | ssl_unique_ptr<BIO>(BIO_new_ssl(this->ctx.get(), NEWSSL_SERVER));

        return ClientConnection{std::move(bio)};
    }

    void ClientConnectionManager::shutdown() {
        const long fd = BIO_get_fd(this->accept_bio.get(), nullptr);
        close(static_cast<int32_t>(fd));
    }

    std::optional<messaging::MessageBlock> ClientConnection::receive_message()
    {
        return ssl::receive_message(this->bio.get());
    }

    void ClientConnection::send_message(const messaging::MessageBlock &block)
    {
        ssl::send_message(this->bio.get(), block);
    }

}