#include "tavernmx/client.h"

using namespace tavernmx::ssl;

namespace tavernmx::client
{
    ServerConnection::ServerConnection(const std::string &host_name, int32_t host_port)
            : host_name{host_name}, host_port{host_port} {
        this->ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_client_method()));
        SSL_CTX_set_min_proto_version(this->ctx.get(), TLS1_2_VERSION);
        if (SSL_CTX_set_default_verify_paths(this->ctx.get()) != 1) {
            print_errors_and_exit("Error loading trust store");
        }
    }

    void ServerConnection::load_certificate(const std::string &cert_path) {
        if (SSL_CTX_load_verify_locations(this->ctx.get(), cert_path.c_str(), nullptr) != 1) {
            print_errors_and_exit("Error loading server cert");
        }
    }

    void ServerConnection::connect() {
        std::string host = this->host_name + ":" + std::to_string(this->host_port);
        this->bio = ssl_unique_ptr<BIO>(BIO_new_connect(host.c_str()));
        BIO_set_nbio(this->bio.get(), 1);
        if (BIO_do_connect_retry(this->bio.get(), 3, 100) != 1) {
            print_errors_and_exit("Error in BIO_do_connect");
        }
        this->bio = std::move(this->bio) | ssl_unique_ptr<BIO>(BIO_new_ssl(this->ctx.get(), NEWSSL_CLIENT));
        SSL_set_tlsext_host_name(get_ssl(this->bio.get()), this->host_name.c_str());
        SSL_set1_host(get_ssl(this->bio.get()), this->host_name.c_str());
        handshake_retry:
        if (BIO_do_handshake(this->bio.get()) <= 0) {
            if (BIO_should_retry(this->bio.get())) {
                goto handshake_retry;
            }
            print_errors_and_exit("Error in TLS handshake");
        }
        verify_certificate(get_ssl(this->bio.get()), false, this->host_name);
    }

    void ServerConnection::send_message(const messaging::MessageBlock &block) {
        ssl::send_message(this->bio.get(), block);
    }

    std::optional<messaging::MessageBlock> ServerConnection::receive_message()
    {
        return ssl::receive_message(get_ssl(this->bio.get()), this->bio.get());
    }
}