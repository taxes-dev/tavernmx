#pragma once

#include <concepts>
#include <exception>
#include <iterator>
#include <optional>
#include <string>
#include <vector>
#include "ssl.h"

namespace tavernmx::client {
    /**
     * @brief Exception for client-related errors.
     */
    class ClientError : public std::exception {
    public:
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         */
        explicit ClientError(std::string what) noexcept : what_str{std::move(what)} {
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         */
        explicit ClientError(const char* what) noexcept : what_str{what} {
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        ClientError(std::string what, const std::exception& inner) noexcept : what_str{std::move(what)} {
            this->what_str += std::string{", caused by: "} + inner.what();
        };
        /**
         * @brief Create a new ClientError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        ClientError(const char* what, const std::exception& inner) noexcept : what_str{what} {
            this->what_str += std::string{", caused by: "} + inner.what();
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
    class ClientConfiguration {
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
    class ServerConnection {
    public:
        /**
         * @brief Creates a ServerConnection that will connect to \p host_name on TCP port \p host_port.
         * @param host_name
         * @param host_port
         * @throws ClientError if openssl can't be initialized
         */
        ServerConnection(std::string host_name, int32_t host_port);

        /**
         * @brief Destructor. Attempts to cleanly shutdown the connection.
         */
        ~ServerConnection();

        /**
         * @brief Loads a custom server certificate into the certificate store. This should be called
         * prior to connect().
         * @param cert_path file system path to the certificate
         * @throws ClientError if the certificate can't be loaded
         */
        void load_certificate(const std::string& cert_path);

        /**
         * @brief Attempts to connect to the server. Does nothing if the connection is already established.
         * @throws ClientError if the connection can't be established
         */
        void connect();

        /**
         * @brief Get the connection host name.
         * @return std::string
         */
        std::string get_host_name() const { return this->host_name; }

        /**
         * @brief Attempts to read a message from the server, if one is waiting.
         * @return a tavernmx::messaging::MessageBlock if a well-formed message block was read, otherwise empty
         * @throws ClientError if a network error occurs
         */
        std::optional<messaging::MessageBlock> receive_message();

        /**
         * @brief Attempts to send a message to the server.
         * @param block block of data to send
         * @throws ClientError if a network error occurs
         */
        void send_message(const messaging::MessageBlock& block);

        /**
         * @brief Attempts to send zero or more messages to the server.
         * @param start start of range pointing to MessageBlock& values
         * @param end end of range pointing to MessageBlock& values
         * @throws ClientError if a network error occurs
         */
        template<class Iterator>
            requires std::forward_iterator<Iterator> && std::same_as<typename Iterator::reference,
                         messaging::MessageBlock &>
        void send_messages(Iterator start, Iterator end) {
            for (auto it = start; it < end; ++it) {
                this->send_message(*it);
            }
        };

        /**
         * @brief Tests if the connection to the server is active.
         * @return true if the socket is connected to the server, otherwise false
         */
        bool is_connected() const;

        /**
         * @brief Attempts to cleanly shutdown the connection.
         */
        void shutdown() noexcept;

        /**
         * @brief Blocks, waiting for a message of type \p message_type.
         * @param message_type tavernmx::messaging::MessageType expected
         * @param milliseconds maximum number of milliseconds to wait, default is the value of
         * tavernmx::ssl::SSL_TIMEOUT_MILLISECONDS
         * @return a tavernmx::messaging::Message if a well-formed message of type
         * \p message_type is received, otherwise empty
         * @note This is used when a certain specific message is expected from the client. Note
         * that any other messages received while waiting will be discarded.
         */
        std::optional<messaging::Message> wait_for(messaging::MessageType message_type,
                                                   ssl::Milliseconds milliseconds = ssl::SSL_TIMEOUT_MILLISECONDS);

    private:
        std::string host_name{};
        int32_t host_port{};
        ssl::ssl_unique_ptr<SSL_CTX> ctx{nullptr};
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };
}
