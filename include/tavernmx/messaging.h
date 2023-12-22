#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tavernmx::messaging {
    /**
     * @brief Structure for sending messages over the network.
     */
    struct MessageBlock {
        /// Header, for locating the start of a MessageBlock.
        const unsigned char HEADER[4] = {'t', 'm', 'x', 0x02};
        /// Size in bytes of payload.
        int32_t payload_size = 0;
        /// Payload data.
        std::vector<uint8_t> payload{};
    };

    /**
     * @brief Specific messages understood by client and server.
     */
    enum class MessageType : int32_t {
        Invalid         = 0,

        // Basic messages
        ACK             = 0x1000,
        NAK             = 0x1001,

        // Connection-related messages
        HELLO           = 0x2000,
    };

    /**
     * @brief Structure for an individual message.
     * @note Prefer the create_* functions instead of instantiating directly.
     */
    struct Message {
        /// The type of message sent.
        MessageType message_type{MessageType::Invalid};

        /// Arbitray map of parameters associated with the message. Each key can contain string, int, or bool.
        std::unordered_map<std::string, std::variant<std::string, int32_t, bool>> values{};
    };

    /**
     * @brief Takes an arbitrary set of bytes and attempts to construct a MessageBlock from it.
     * @param buffer The bytes of data to parse.
     * @param bufsize Size of \p buffer.
     * @param block A MessageBlock struct to insert data into.
     * @param payload_offset See notes.
     * @return The number of bytes of payload actually inserted into \p block. See notes.
     * @note MessageBlock data can potentially be broken up among several chunks. On the first call,
     * \p payload_offset should be set to 0. The return is the number of bytes of the payload processed.
     * If the accumulated return value is not equal to the payload_size in \p block, then keep calling this
     * method with the accumulated return value passed back into \p payload_offset until that condition is satisfied,
     * i.e. the sum of the return values is equal to payload_size, or the return value is 0 (no bytes processed).
     */
    size_t
    apply_buffer_to_block(const unsigned char *buffer, size_t bufsize, MessageBlock &block, size_t payload_offset = 0);

    /**
     * @brief Converts \p block into a set of bytes.
     * @param block MessageBlock
     * @return std::vector<unsigned char>
     */
    std::vector<unsigned char> pack_block(const MessageBlock &block);

    /**
     * @brief Packs zero or more \p messages into one or more MessageBlock structs.
     * @param messages std::vector<Message>
     * @return std::vector<MessageBlock>
     */
    std::vector<MessageBlock> pack_messages(const std::vector<Message> & messages);

    /**
     * @brief Unpacks zero or more messages from \p block.
     * @param block MessageBlock
     * @return std::vector<Message>
     */
    std::vector<Message> unpack_messages(const MessageBlock &block);

    /**
     * @brief Create an ACK Message struct.
     * @return Message
     */
    inline Message create_ack() {
        return Message { .message_type =  MessageType::ACK };
    }

    /**
     * @brief Creates a NAK Message struct.
     * @param error An optional error message
     * @return Message
     */
    inline Message create_nak(const std::string& error = std::string{}) {
        return Message {
            .message_type = MessageType::NAK,
            .values =  {{"error", error}}
        };
    }

    /**
     * @brief Create a HELLO Message struct.
     * @param user_name User name
     * @return Message
     */
    inline Message create_hello(const std::string& user_name) {
        return {
            .message_type =  MessageType::HELLO,
            .values = {{"user_name", user_name}}
        };
    }
}