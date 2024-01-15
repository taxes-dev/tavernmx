#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "nlohmann/json.hpp"

namespace tavernmx::messaging
{
    /// Data type for transport characters
    using CharType = uint8_t;

    /// Shorthand for json type dependency.
    using json = nlohmann::json;

    /**
     * @brief Structure for sending messages over the network.
     */
    struct MessageBlock
    {
        /// Header, for locating the start of a MessageBlock.
        const CharType HEADER[4] = { 't', 'm', 'x', 0x02 };
        /// Size in bytes of payload.
        uint32_t payload_size{ 0 };
        /// Payload data.
        std::vector<CharType> payload{};

        /**
         * @brief Set the payload to contain \p value. Also updates the value of payload_size.
         * @param value std::vector<CharType>
         */
        void set_payload(std::vector<CharType> value) {
            this->payload = std::move(value);
            this->payload_size = static_cast<int32_t>(this->payload.size());
        }
    };

    /**
     * @brief Specific messages understood by client and server.
     */
    enum class MessageType : int32_t
    {
        /// Default, not processible
        Invalid = 0,

        // Basic messages
        /// Acknowledged
        ACK = 0x1000,
        /// Not acknowledged
        NAK = 0x1001,

        // Connection-related messages
        /// Sent by client to server with auth info (can be responded with ACK or NAK)
        HELLO = 0x2000,
        /// Sent by client or server to check if the other is alive (should be responded with ACK)
        HEARTBEAT = 0x2001,

        // Room-related messages
        /// Sent by client to request list of rooms, sent by server to respond with list of rooms.
        ROOM_LIST = 0x3000,
        /// Sent by client to request creation of a new room, sent by server to notify of a new room.
        ROOM_CREATE = 0x3001,
        /// Sent by client to join a room.
        ROOM_JOIN = 0x3002,
        /// Sent by client to request deletion of a room, sent by server to notify of a deleted room.
        ROOM_DESTROY = 0x3003,
        /// Sent by client to request the chat history for a room, sent by server to deliver the history payload.
        ROOM_HISTORY = 0x3004,

        // Chat-related messages
        /// Client sending a single line of chat to a room.
        CHAT_SEND = 0x4000,
        /// Server echoing a single line of chat to a room.
        CHAT_ECHO = 0x4001,
    };

    /// Maximum number of entries that can be retrieved as part of MessageType::ROOM_HISTORY.
    constexpr int32_t ROOM_HISTORY_MAX_ENTRIES = 100;

    /**
     * @brief Structure for an individual message.
     * @note Prefer the create_* functions instead of instantiating directly.
     */
    struct Message
    {
        /// The type of message sent.
        MessageType message_type{ MessageType::Invalid };

        /// JSON map for arbitrary values.
        nlohmann::json values{};
    };

    /**
     * @brief Check if \p message contains a root-level value specified by \p key.
     * @param message Message
     * @param key std::string
     * @return true if \p key is present in the values collection of \p message, otherwise false.
     */
    inline bool message_has_value(const Message& message, const std::string& key) {
        return message.values.is_object() && message.values.contains(key);
    }

    /**
     * @brief Attempt to retrieve the root-level value specified by \p key in \p message.
     * @tparam T A type supported by the JSON library. Must be default constructible.
     * @param message Message
     * @param key std::string
     * @param default_value A default value to return if \p key isn't found.
     * @return The value associated with \p key if it is present, otherwise \p default_value.
     */
    template <typename T>
        requires std::is_default_constructible_v<T>
    T message_value_or(const Message& message, const std::string& key, const T& default_value = T{}) {
        if (message.values.is_object() &&
            message.values.contains(key) &&
            message.values[key].is_primitive() &&
            !message.values[key].is_null()) {
            return message.values[key].get<T>();
        }
        return default_value;
    }

    /**
     * @brief Takes an arbitrary set of bytes and attempts to construct a MessageBlock from it.
     * @param buffer The bytes of data to parse.
     * @param block A MessageBlock struct to insert data into.
     * @param payload_offset See notes.
     * @return The number of bytes of payload actually inserted into \p block. See notes.
     * @note MessageBlock data can potentially be broken up among several chunks. On the first call,
     * \p payload_offset should be set to 0. The return is the number of bytes of the payload processed.
     * If the accumulated return value is not equal to the payload_size in \p block, then keep calling this
     * method with the accumulated return value passed back into \p payload_offset until that condition is satisfied,
     * i.e. the sum of the return values is equal to payload_size, or the return value is 0 (no bytes processed).
     */
    size_t apply_buffer_to_block(const std::span<CharType>& buffer, MessageBlock& block, size_t payload_offset = 0);

    /**
     * @brief Converts a Message struct into a JSON representation.
     * @param message Message
     * @return nlohmann::json
     */
    json message_to_json(const Message& message);

    /**
     * @brief Converts \p block into a set of bytes.
     * @param block MessageBlock
     * @return std::vector<CharType>
     */
    std::vector<CharType> pack_block(const MessageBlock& block);

    /**
     * @brief Packs a \p message into a MessageBlock struct.
     * @param message Message
     * @return MessageBlock
     */
    MessageBlock pack_message(const Message& message);

    /**
     * @brief Packs zero or more \p messages into a MessageBlock struct.
     * @tparam Iterator Forward iterator of Message structs.
     * @param begin Beginning of the range of Message structs to pack.
     * @param end End of the range of Message structs to pack.
     * @return MessageBlock
     */
    template <class Iterator>
        requires std::forward_iterator<Iterator> && std::same_as<std::iter_value_t<Iterator>, Message>
    MessageBlock pack_messages(Iterator begin, Iterator end) {
        json group_json = json::array();
        std::for_each(begin, end, [&group_json](typename Iterator::reference message) {
            group_json.push_back(message_to_json(message));
        });

        MessageBlock block{};
        block.set_payload(json::to_msgpack(group_json));
        return block;
    }

    /**
     * @brief Unpacks zero or more messages from \p block.
     * @param block MessageBlock
     * @return std::vector<Message>
     */
    std::vector<Message> unpack_messages(const MessageBlock& block);

    /**
     * @brief Create an ACK Message struct.
     * @return Message
     */
    inline Message create_ack() {
        return Message{ .message_type = MessageType::ACK };
    }

    /**
     * @brief Creates a NAK Message struct.
     * @param error (copied) An optional error message
     * @return Message
     */
    inline Message create_nak(std::string error = std::string{}) {
        return Message{
            .message_type = MessageType::NAK,
            .values = { { "error", std::move(error) } }
        };
    }

    /**
     * @brief Create a HELLO Message struct.
     * @param user_name (copied) User name
     * @return Message
     */
    inline Message create_hello(std::string user_name) {
        return {
            .message_type = MessageType::HELLO,
            .values = { { "user_name", std::move(user_name) } }
        };
    }

    /**
     * @brief Create a HEARTBEAT Message struct.
     * @return Message
     */
    inline Message create_heartbeat() {
        return Message{ .message_type = MessageType::HEARTBEAT };
    }


    /**
     * @brief Create a ROOM_LIST Message struct to request the room list.
     * @return Message
     */
    inline Message create_room_list() {
        return Message{ .message_type = MessageType::ROOM_LIST };
    }

    /**
     * @brief Create a ROOM_LIST Message struct to send the room list.
     * @tparam Iterator forward iterator of std::string
     * @param rooms_begin The beginning of the range of room names.
     * @param rooms_end The end of the range of room names.
     * @return Message
     */
    template <class Iterator>
        requires std::forward_iterator<Iterator> && std::same_as<std::iter_value_t<Iterator>,
                     std::string>
    Message create_room_list(Iterator rooms_begin, Iterator rooms_end) {
        Message message{ .message_type = MessageType::ROOM_LIST };
        int32_t i = 0;
        for (auto it = rooms_begin; it < rooms_end; ++it, ++i) {
            message.values[std::to_string(i)] = *it;
        }
        return message;
    }

    /**
     * @brief Create a ROOM_CREATE Message struct to create a new chat room.
     * @param room_name (copied) The room's unique name.
     * @return Message
     */
    inline Message create_room_create(std::string room_name) {
        return Message{ .message_type = MessageType::ROOM_CREATE,
                        .values = { { "room_name", std::move(room_name) } } };
    }

    /**
     * @brief Create a ROOM_JOIN Message struct to join a chat room.
     * @param room_name (copied) The room's unique name.
     * @return Message
     */
    inline Message create_room_join(std::string room_name) {
        return Message{ .message_type = MessageType::ROOM_JOIN,
                        .values = { { "room_name", std::move(room_name) } } };
    }

    /**
     * @brief Create a ROOM_DESTROY Message struct to destroy an existing chat room.
     * @param room_name (copied) The room's unique name.
     * @return Message
     */
    inline Message create_room_destroy(std::string room_name) {
        return Message{ .message_type = MessageType::ROOM_DESTROY,
                        .values = { { "room_name", std::move(room_name) } } };
    }

    /**
     * @brief Create a ROOM_HISTORY Message struct to request or send room history.
     * @param room_name (copied) The room's unique name.
     * @param event_count The maximum number of events to retrieve, or the count of events to send.
     * @return Message
     * @note \p event_count must be between 0 and ROOM_HISTORY_MAX_ENTRIES (inclusive).
     */
    inline Message create_room_history(std::string room_name, int32_t event_count = ROOM_HISTORY_MAX_ENTRIES) {
        assert(event_count >= 0 && event_count <= ROOM_HISTORY_MAX_ENTRIES);
        return Message{ .message_type = MessageType::ROOM_HISTORY,
                        .values = { { "room_name", std::move(room_name) },
                                    { "event_count", event_count } } };
    }

    /**
     * @brief Adds event data to \p room_history_message. Will automatically increment the
     * event count as well.
     * @param room_history_message Message of type MessageType::ROOM_HISTORY.
     * @param timestamp Time of the event, in seconds from epoch.
     * @param origin_user_name (copied) Origin user name.
     * @param text (copied) Line of chat text.
     * @return The number of events in \p room_history_message after inserting the event data.
     */
    int32_t add_room_history_event(Message& room_history_message,
        int32_t timestamp, std::string origin_user_name, std::string text);

    /**
     * @brief Create a CHAT_SEND Message struct to send a chat message to the server.
     * @param room_name (copied) Target room name.
     * @param text (copied) Line of chat text.
     * @return Message
     */
    inline Message create_chat_send(std::string room_name, std::string text) {
        return Message{ .message_type = MessageType::CHAT_SEND,
                        .values = { { "room_name", std::move(room_name) },
                                    { "text", std::move(text) } } };
    }

    /**
     * @brief Create a CHAT_ECHO Message struct to send a chat message to a client.
     * @param room_name (copied) Origin room name.
     * @param text (copied) Line of chat text.
     * @param user_name (copied) Orign user name.
     * @param timestamp Number of seconds since epoch when the event occurred.
     * @return Message
     */
    inline Message create_chat_echo(std::string room_name, std::string text,
        std::string user_name, int32_t timestamp) {
        return Message{ .message_type = MessageType::CHAT_ECHO,
                        .values = {
                            { "room_name", std::move(room_name) },
                            { "text", std::move(text) },
                            { "user_name", std::move(user_name) },
                            { "timestamp", timestamp }
                        } };
    }
}
