#pragma once

#include "ssl.h"

namespace tavernmx {
    /**
     * @brief Exception for transport errors.
     */
    class TransportError : public std::exception {
    public:
        /**
         * @brief Create a new TransportError.
         * @param what description of the error
         */
        explicit TransportError(std::string what) noexcept : what_str{std::move(what)} {
        };
        /**
         * @brief Create a new TransportError.
         * @param what description of the error
         */
        explicit TransportError(const char* what) noexcept : what_str{what} {
        };
        /**
         * @brief Create a new TransportError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        TransportError(std::string what, const std::exception& inner) noexcept : what_str{std::move(what)} {
            this->what_str += std::string{", caused by: "} + inner.what();
        };
        /**
         * @brief Create a new TransportError.
         * @param what description of the error
         * @param inner exception that caused this error
         */
        TransportError(const char* what, const std::exception& inner) noexcept : what_str{what} {
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

    class BaseConnection {
    public:
        /**
         * @brief Creates a new BaseConnection.
         */
        BaseConnection() = default;

        virtual ~BaseConnection() { this->shutdown(); }

        BaseConnection(const BaseConnection&) = delete;

        BaseConnection& operator=(const BaseConnection& other) = delete;

        BaseConnection(BaseConnection&&) = default;

        BaseConnection& operator=(BaseConnection&&) = default;

        /**
         * @brief Attempts to read a message from the server, if one is waiting.
         * @return a tavernmx::messaging::MessageBlock if a well-formed message block was read, otherwise empty
         * @throws TransportError if a network error occurs
         */
        std::optional<messaging::MessageBlock> receive_message();

        /**
         * @brief Attempts to send a message block to the server.
         * @param block block of data to send
         * @throws TransportError if a network error occurs
         */
        void send_message_block(const messaging::MessageBlock& block);

        /**
         * @brief Attempts to send a single message to the server.
         * @param message tavernmx::messaging::Message
         * @throws TransportError if a network error occurs
         */
        void send_message(const messaging::Message& message);

        /**
         * @brief Attempts to send zero or more messages to the server.
         * @param begin start of range pointing to Message values
         * @param end end of range pointing to Message values
         * @throws TransportError if a network error occurs
         */
        template<typename Iterator>
            requires std::forward_iterator<Iterator> && std::same_as<std::iter_value_t<Iterator>, messaging::Message>
        void send_messages(Iterator begin, Iterator end) {
            // TODO: ditch intermediary container
            const std::vector<messaging::Message> messages{begin, end};
            const auto blocks = messaging::pack_messages(messages);
            this->send_message_blocks(std::cbegin(blocks), std::cend(blocks));
        };

        /**
         * @brief Attempts to send zero or more message blocks to the server.
         * @param begin start of range pointing to MessageBlock& values
         * @param end end of range pointing to MessageBlock& values
         * @throws TransportError if a network error occurs
         */
        template<class Iterator>
            requires std::forward_iterator<Iterator> && std::same_as<std::iter_value_t<Iterator>,
                         messaging::MessageBlock>
        void send_message_blocks(Iterator begin, Iterator end) {
            for (auto it = begin; it != end; ++it) {
                this->send_message_block(*it);
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

    protected:
        ssl::ssl_unique_ptr<BIO> bio{nullptr};
    };
}
