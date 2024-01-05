#pragma once

#include <exception>
#include <string>
#include <vector>
#include "shared.h"

using namespace std::string_literals;

namespace tavernmx::client
{
    /**
     * @brief Exception for client-related errors.
     */
    class ClientError : public std::exception
    {
    public:
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         */
        explicit ClientError(std::string what) noexcept
            : what_str{ std::move(what) } {
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         */
        explicit ClientError(const char* what) noexcept
            : what_str{ what } {
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        ClientError(std::string what, const std::exception& inner) noexcept
            : what_str{ std::move(what) } {
            this->what_str += std::string{ ", caused by: " } + inner.what();
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        ClientError(const char* what, const std::exception& inner) noexcept
            : what_str{ what } {
            this->what_str += std::string{ ", caused by: " } + inner.what();
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
     * @brief Parses and contains the client configuration data.
     */
    class ClientConfiguration
    {
    public:
        /**
         * @brief Initializes ClientConfiguration by loading the .json file stored at \p config_path.
         * @param config_path file system path to the client configuration .json
         * @throws ClientError if the client configuration file is not found or invalid
         */
        explicit ClientConfiguration(const std::string& config_path);

        /**
         * @brief The host name of the server to connect to.
         */
        std::string host_name{};
        /**
         * @brief The host port of the server to connect to.
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
         * @brief Contains zero or more custom server certificates to recognize when connecting.
         */
        std::vector<std::string> custom_certificates{};
    };

    /**
     * @brief Manages the connection to the tavernmx server.
     */
    class ServerConnection : public BaseConnection
    {
    public:
        std::shared_ptr<ThreadSafeQueue<messaging::Message>> messages_in;
        std::shared_ptr<ThreadSafeQueue<messaging::Message>> messages_out;

        /**
         * @brief Creates a ServerConnection that will connect to \p host_name on TCP port \p host_port.
         * @param host_name
         * @param host_port
         * @throws TransportError if openssl can't be initialized
         */
        ServerConnection(std::string host_name, int32_t host_port);

        ServerConnection(const ServerConnection&) = delete;

        ServerConnection& operator=(const ServerConnection& other) = delete;

        ServerConnection(ServerConnection&&) = default;

        ServerConnection& operator=(ServerConnection&&) = default;

        /**
         * @brief Loads a custom server certificate into the certificate store. This should be called
         * prior to connect().
         * @param cert_path file system path to the certificate
         * @throws TransportError if the certificate can't be loaded
         */
        void load_certificate(const std::string& cert_path);

        /**
         * @brief Attempts to connect to the server. Does nothing if the connection is already established.
         * @throws TransportError if the connection can't be established
         */
        void connect();

        /**
         * @brief Get the connection host name.
         * @return std::string
         */
        const std::string& get_host_name() const { return this->host_name; }

    private:
        std::string host_name{};
        int32_t host_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{ nullptr };
    };
}

namespace tavernmx::rooms
{
    /**
     * @brief Represents a chat room managed by the client.
     */
    class ClientRoom : public Room
    {
    public:
        /// Room events received from the server.
        std::vector<RoomEvent> events{};

        /**
         * @brief Create a ClientRoom.
         * @param room_name The room's unique name.
         */
        explicit ClientRoom(std::string room_name)
            : Room{ std::move(room_name) } {
        };

        ClientRoom(const ClientRoom&) = delete;

        ClientRoom(ClientRoom&&) = default;

        ClientRoom& operator=(const ClientRoom&) = delete;

        ClientRoom& operator=(ClientRoom&&) = default;
    };

}
