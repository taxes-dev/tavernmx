#pragma once

#include <mutex>
#include <optional>
#include <string>
#include "connection.h"
#include "queue.h"

namespace tavernmx::server {
    /**
     * @brief Exception for server-related errors.
     */
    class ServerError : public std::exception {
    public:
        /**
         * @brief Create a new ServerError.
         * @param what description of the error
         */
        explicit ServerError(std::string what) noexcept: what_str{std::move(what)} {
        };

        /**
         * @brief Create a new ServerError.
         * @param what description of the error
         */
        explicit ServerError(const char* what) noexcept: what_str{what} {
        };

        /**
         * @brief Create a new ServerError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        ServerError(std::string what, const std::exception& inner) noexcept: what_str{
            std::move(what)
        } { this->what_str += std::string{", caused by: "} + inner.what(); };

        /**
         * @brief Create a new ServerError.
         * @param what description of the error
         * @param inner exception that cause this error
         */
        ServerError(const char* what, const std::exception& inner) noexcept: what_str{what} {
            this->what_str +=
                    std::string{
                        ", caused by: "
                    } +
                    inner.what();
        }

        /**
         * @brief Returns an explanatory string.
         * @return pointer to a NULL-terminated string
         */
        const char* what() const noexcept override { return this->what_str.c_str(); }

    private:
        std::string what_str{};
    };

    /**
     * @brief Parses and contains the server configuration data.
     */
    class ServerConfiguration {
    public:
        /**
         * @brief Initializes ServerConfiguration by loading the .json file stored at \p config_path.
         * @param config_path file system path to the server configuration .json
         * @throws ServerError if the server configuration file is not found or invalid
         */
        explicit ServerConfiguration(const std::string& config_path);

        /**
         * @brief The host port to accept incoming connections on. Defaults to 8080.
         */
        int32_t host_port{};
        /**
         * @brief The maximum log level for logging ("off", "info", "warn", or "err"). Defaults to "warn".
         */
        std::string log_level{};
        /**
         * @brief If specified, a path to a writable location where logging will be written to file.
         */
        std::string log_file{};
        /**
         * @brief File system path to the server's SSL certificate.
         */
        std::string host_certificate_path{};
        /**
         * @brief File system path to the server's SSL private key.
         */
        std::string host_private_key_path{};
        /**
         * @brief Max number of simultaneous client connections to support. Defaults to 10.
         */
        std::int32_t max_clients{};
        /**
         * @brief Set of chat rooms to create at startup.
         */
        std::vector<std::string> initial_rooms{};
    };

    /**
     * @brief Manages an individual connection to a tavernmx client.
     */
    class ClientConnection : public BaseConnection {
    public:
        ThreadSafeQueue<messaging::Message> messages_in{};
        ThreadSafeQueue<messaging::Message> messages_out{};

        /**
         * @brief Creates a ClientConnection representing the given \p client_bio.
         * @param client_bio An active BIO generated by the server's accept BIO.
         */
        explicit ClientConnection(ssl::ssl_unique_ptr<BIO>&& client_bio) {
            this->bio = std::move(client_bio);
        };

        ClientConnection(const ClientConnection&) = delete;

        ClientConnection& operator=(const ClientConnection& other) = delete;

        ClientConnection(ClientConnection&&) = default;

        ClientConnection& operator=(ClientConnection&&) = default;
    };

    /**
     * @brief Accepts new client connections to the server and manages them.
     */
    class ClientConnectionManager {
    public:
        /**
         * @brief Create a new ClientConnectionManager that will accept connections
         * on the given TCP \p accept_port.
         * @param accept_port
         */
        explicit ClientConnectionManager(int32_t accept_port);

        ClientConnectionManager(const ClientConnectionManager&) = delete;

        ClientConnectionManager& operator=(const ClientConnectionManager&) = delete;

        ClientConnectionManager(ClientConnectionManager&& other) noexcept {
            *this = std::move(other);
        };

        ClientConnectionManager& operator=(ClientConnectionManager&& other) noexcept {
            std::lock_guard guard1{this->active_connections_mutex};
            std::lock_guard guard2{other.active_connections_mutex};
            this->accept_port = other.accept_port;
            this->ctx = std::move(other.ctx);
            this->accept_bio = std::move(other.accept_bio);
            this->active_connections = std::move(other.active_connections);
            return *this;
        };

        /**
         * @brief Destructor. Attempts to cleanly shutdown all connections.
         */
        ~ClientConnectionManager();

        /**
         * @brief Loads the server SSL certificate. Should be called prior to await_next_connection().
         * @param cert_path File system path to the server SSL certificate.
         * @param private_key_path File system path to the server SSL private key.
         * @throws ServerError if the certificate/private key pair cannot be loaded
         */
        void load_certificate(const std::string& cert_path, const std::string& private_key_path);

        /**
         * @brief Explicitly creates the accept port. Does nothing if connections are already being accepted.
         * @throws ServerError if an SSL error occurs
         */
        void begin_accept();

        /**
         * @brief Blocks until a new client connects to the server. If the accept port is not created yet,
         * it will be created the first time this method is called.
         * @return A std::shared_ptr<ClientConnection> for the newly connected client. If the accept port
         * is no longer listening, it will return empty.
         * @note The returned ClientConnection should be passed to a worker thread for further processing.
         * If no connection is waiting, this will sleep for tavernmx::ssl::SSL_RETRY_MILLISECONDS to avoid
         * tight loops.
         */
        std::optional<std::shared_ptr<ClientConnection>> await_next_connection();

        /**
         * @brief Attempts to shutdown the accept port and all active client connections.
         */
        void shutdown() noexcept;

        /**
         * @brief Retrieves all of the active client connections.
         * @return std::vector containing zero or more pointers to ClientConnection.
         * @note Thread safe.
         */
        std::vector<std::shared_ptr<ClientConnection>> get_active_connections();

        /**
         * @brief Determines if this ClientConnectionManager's accept socket is active.
         * @return true if the manager is currently accepting connections, otherwise false.
         */
        bool is_accepting_connections();

    private:
        int32_t accept_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> accept_bio{nullptr};
        std::vector<std::shared_ptr<ClientConnection>> active_connections{};
        mutable std::mutex active_connections_mutex{};

        /**
         * @brief Removes client connections that are no longer active.
         */
        void cleanup_connections();
    };
}
