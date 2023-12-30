#include <chrono>
#include "tavernmx/connection.h"

using namespace tavernmx::messaging;

namespace tavernmx {
    void BaseConnection::send_message_block(const MessageBlock& block) {
        if (!this->is_connected()) {
            throw TransportError{"Connection lost"};
        }
        try {
            ssl::send_message(this->bio.get(), block);
        } catch (ssl::SslError& ex) {
            throw TransportError{"send_message_block failed", ex};
        }
    }

    void BaseConnection::send_message(const Message& message) {
        auto block = pack_message(message);
        this->send_message_block(block);
    }

    std::optional<MessageBlock> BaseConnection::receive_message() {
        if (!this->is_connected()) {
            throw TransportError{"Connection lost"};
        }
        try {
            return ssl::receive_message(this->bio.get());
        } catch (ssl::SslError& ex) {
            throw TransportError{"receive_message failed", ex};
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
        const auto start = std::chrono::high_resolution_clock::now();
        ssl::Milliseconds elapsed = 0;

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
