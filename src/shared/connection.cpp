#include <chrono>
#include "tavernmx/connection.h"

using namespace tavernmx::messaging;

namespace tavernmx
{
    void BaseConnection::send_message_block(const MessageBlock& block) {
        if (!this->is_connected()) {
            throw TransportError{ "Connection lost" };
        }
        try {
            ssl::send_message(this->bio.get(), block);
        } catch (ssl::SslError& ex) {
            throw TransportError{ "send_message_block failed", ex };
        }
    }

    void BaseConnection::send_message(const Message& message) {
        const MessageBlock block = pack_message(message);
        this->send_message_block(block);
    }

    std::optional<MessageBlock> BaseConnection::receive_message() {
        if (!this->is_connected()) {
            throw TransportError{ "Connection lost" };
        }
        try {
            return ssl::receive_message(this->bio.get());
        } catch (ssl::SslError& ex) {
            throw TransportError{ "receive_message failed", ex };
        }
    }

    bool BaseConnection::is_connected() const {
        return ssl::is_connected(this->bio.get());
    }

    void BaseConnection::shutdown() noexcept {
        if (this->bio) {
            BIO_ssl_shutdown(this->bio.get());
            this->bio.reset();
        }
    }

    std::optional<Message> BaseConnection::wait_for(MessageType message_type,
        ssl::Milliseconds milliseconds) {
        const std::chrono::time_point<std::chrono::high_resolution_clock> start =
            std::chrono::high_resolution_clock::now();
        ssl::Milliseconds elapsed = 0;

        do {
            if (std::optional<MessageBlock> message_block = this->receive_message()) {
                for (Message& message : unpack_messages(message_block.value())) {
                    if (message.message_type == message_type) {
                        return std::move(message);
                    }
                }
            }

            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count();
        } while (elapsed < milliseconds);

        return {};
    }

    std::optional<Message> BaseConnection::wait_for_ack_or_nak(ssl::Milliseconds milliseconds) {
        const std::chrono::time_point<std::chrono::high_resolution_clock> start =
            std::chrono::high_resolution_clock::now();
        ssl::Milliseconds elapsed = 0;

        do {
            if (const std::optional<MessageBlock> message_block = this->receive_message()) {
                for (Message& message : unpack_messages(message_block.value())) {
                    if (message.message_type == MessageType::ACK || message.message_type == MessageType::NAK) {
                        return std::move(message);
                    }
                }
            }
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count();
        } while (elapsed < milliseconds);

        return {};
    }

}
