#include <chrono>
#include "tavernmx/client.h"
#include "tavernmx/logging.h"

using namespace tavernmx::ssl;

namespace {
    constexpr time_t WAIT_SECONDS = 3;
    constexpr uint32_t NAP_MILLISECONDS = 250;

    tavernmx::client::ClientError ssl_errors_to_exception(const char* message) {
        char buffer[256];
        std::string msg{message};
        while (const unsigned long err = ERR_get_error() != 0) {
            ERR_error_string_n(err, buffer, sizeof(buffer));
            msg += ", ";
            msg += buffer;
        }
        return tavernmx::client::ClientError{msg};
    }
}

namespace tavernmx::client {
    ServerConnection::ServerConnection(std::string host_name, int32_t host_port)
        : host_name{std::move(host_name)}, host_port{host_port} {
        SSL_load_error_strings();
        this->ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_client_method()));
        SSL_CTX_set_min_proto_version(this->ctx.get(), TLS1_2_VERSION);
        if (SSL_CTX_set_default_verify_paths(this->ctx.get()) != 1) {
            throw ssl_errors_to_exception("Error loading trust store");
        }
    }

    ServerConnection::~ServerConnection() {
        this->shutdown();
    }


    void ServerConnection::load_certificate(const std::string& cert_path) {
        if (SSL_CTX_load_verify_locations(this->ctx.get(), cert_path.c_str(), nullptr) != 1) {
            throw ssl_errors_to_exception("Error loading server cert");
        }
    }

    void ServerConnection::connect() {
        if (this->is_connected()) {
            return;
        }
        const std::string host = this->host_name + ":" + std::to_string(this->host_port);
        this->bio = ssl_unique_ptr<BIO>(BIO_new_connect(host.c_str()));
        BIO_set_nbio(this->bio.get(), 1);
        if (BIO_do_connect_retry(this->bio.get(), WAIT_SECONDS, NAP_MILLISECONDS) != 1) {
            throw ssl_errors_to_exception("BIO_do_connect failed");
        }
        this->bio = std::move(this->bio) | ssl_unique_ptr<BIO>(BIO_new_ssl(this->ctx.get(), NEWSSL_CLIENT));
        SSL_set_tlsext_host_name(get_ssl(this->bio.get()), this->host_name.c_str());
        SSL_set1_host(get_ssl(this->bio.get()), this->host_name.c_str());
        while (BIO_do_handshake(this->bio.get()) <= 0) {
            if (BIO_should_retry(this->bio.get())) {
                continue;
            }
            throw ssl_errors_to_exception("TLS handshake failed");
        }
        verify_certificate(get_ssl(this->bio.get()), false, this->host_name);
    }

    void ServerConnection::send_message(const messaging::MessageBlock& block) {
        if (!this->is_connected()) {
            throw ClientError{"Connection lost"};
        }
        try {
            ssl::send_message(this->bio.get(), block);
        } catch (SslError& ex) {
            throw ClientError{"send_message failed", ex};
        }
    }

    std::optional<messaging::MessageBlock> ServerConnection::receive_message() {
        if (!this->is_connected()) {
            throw ClientError{"Connection lost"};
        }
        try {
            return ssl::receive_message(this->bio.get());
        } catch (SslError& ex) {
            throw ClientError{"receive_message failed", ex};
        }
    }

    bool ServerConnection::is_connected() const {
        return ssl::is_connected(this->bio.get());
    }

    void ServerConnection::shutdown() noexcept {
        if (this->bio) {
            BIO_ssl_shutdown(this->bio.get());
            this->bio.reset();
        }
    }

    std::optional<messaging::Message> ServerConnection::wait_for(messaging::MessageType message_type,
                                                                 Milliseconds milliseconds) {
        const auto start = std::chrono::high_resolution_clock::now();
        Milliseconds elapsed = 0;

        while (elapsed < milliseconds) {
            if (auto message_block = this->receive_message()) {
                auto messages = unpack_messages(message_block.value());
                for (auto& message: messages) {
                    if (message.message_type == message_type) {
                        return message;
                    }
                }
            }

            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count();
        }

        return {};
    }


}
