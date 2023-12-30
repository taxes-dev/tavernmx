#include <chrono>
#include <thread>
#include <libc.h>
#include "tavernmx/server.h"

using namespace tavernmx::ssl;

namespace
{
    tavernmx::server::ServerError ssl_errors_to_exception(const char* message) {
        char buffer[256];
        std::string msg{ message };
        while (const unsigned long err = ERR_get_error() != 0) {
            ERR_error_string_n(err, buffer, sizeof(buffer));
            msg += ", ";
            msg += buffer;
        }
        return tavernmx::server::ServerError{ msg };
    }
}

namespace tavernmx::server
{
    ClientConnectionManager::ClientConnectionManager(int32_t accept_port)
        : accept_port{ accept_port } {
        SSL_load_error_strings();
        this->ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_method()));
        SSL_CTX_set_min_proto_version(this->ctx.get(), TLS1_2_VERSION);
        SSL_CTX_set_mode(this->ctx.get(), SSL_MODE_AUTO_RETRY);
    }

    ClientConnectionManager::~ClientConnectionManager() {
        this->shutdown();
    }


    void ClientConnectionManager::load_certificate(const std::string& cert_path, const std::string& private_key_path) {
        if (SSL_CTX_use_certificate_file(this->ctx.get(), cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw ssl_errors_to_exception("Error loading server certificate");
        }
        if (SSL_CTX_use_PrivateKey_file(this->ctx.get(), private_key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw ssl_errors_to_exception("Error loading server private key");
        }
    }

    void ClientConnectionManager::begin_accept() {
        if (this->accept_bio == nullptr) {
            const std::string host_port = std::to_string(this->accept_port);
            this->accept_bio = ssl_unique_ptr<BIO>(BIO_new_accept(host_port.c_str()));
            BIO_set_nbio(this->accept_bio.get(), 1);
            BIO_set_nbio_accept(this->accept_bio.get(), 1);
            if (BIO_do_accept(this->accept_bio.get()) != 1) {
                throw ssl_errors_to_exception("Error in BIO_do_accept");
            }
        }
    }

    std::optional<std::shared_ptr<ClientConnection>> ClientConnectionManager::await_next_connection() {
        this->begin_accept();

        auto bio = accept_new_tcp_connection(this->accept_bio.get());
        if (bio == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds{ SSL_RETRY_MILLISECONDS });
            return {};
        }
        bio = std::move(bio)
              | ssl_unique_ptr<BIO>(BIO_new_ssl(this->ctx.get(), NEWSSL_SERVER));

        this->cleanup_connections();

        auto connection = std::make_shared<ClientConnection>(std::move(bio)); {
            std::lock_guard guard{ this->active_connections_mutex };

            this->active_connections.push_back(connection);
        }
        return connection;
    }

    void ClientConnectionManager::shutdown() noexcept {
        std::lock_guard guard{ this->active_connections_mutex };
        for (auto& connection : this->active_connections) {
            connection->shutdown();
        }
        this->active_connections.clear();
        if (this->accept_bio) {
            const long fd = BIO_get_fd(this->accept_bio.get(), nullptr);
            close(static_cast<int32_t>(fd));
            this->accept_bio.reset();
        }
    }

    std::vector<std::shared_ptr<ClientConnection>> ClientConnectionManager::get_active_connections() {
        std::lock_guard guard{ this->active_connections_mutex };
        // copy is intentional here
        return this->active_connections;
    }

    bool ClientConnectionManager::is_accepting_connections() {
        return this->accept_bio != nullptr;
    }

    void ClientConnectionManager::cleanup_connections() {
        std::lock_guard guard{ this->active_connections_mutex };
        std::erase_if(this->active_connections, [](const std::shared_ptr<ClientConnection>& connection) {
            return !connection->is_connected();
        });
    }

}
