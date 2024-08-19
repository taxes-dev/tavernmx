#include <algorithm>
#include <bit>
#include <cassert>
#include <functional>
#include <utility>
#include "tavernmx/messaging.h"
#include "tavernmx/platform.h"

#ifdef TMX_WINDOWS
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace tavernmx::messaging
{
    json message_to_json(const Message& message) {
        json message_json = json::object();
        message_json["message_type"] = message.message_type;
        message_json["values"] = message.values;
        return message_json;
    }

    size_t apply_buffer_to_block(const std::span<CharType>& buffer, MessageBlock& block, size_t payload_offset) {
        if (buffer.empty()) {
            return 0;
        }

        if (payload_offset == 0) {
            // look for the start of a message block
            auto header_it = std::find(std::cbegin(buffer), std::cend(buffer), block.HEADER[0]);

            if (header_it == std::cend(buffer) ||
                (header_it + sizeof(block.HEADER)) > std::cend(buffer) ||
                !std::equal(std::cbegin(block.HEADER), std::cend(block.HEADER), header_it,
                    header_it + sizeof(block.HEADER))) {
                return 0;
            }
            // found, extract the block size
            header_it += sizeof(block.HEADER);

            if (const size_t remaining = buffer.size() - (header_it - std::cbegin(buffer));
                std::cmp_less(remaining, sizeof(block.payload_size))) {
                return 0;
            }
            CharType payload_size_bytes[sizeof(block.payload_size)];
            std::copy_n(header_it, sizeof(block.payload_size), std::begin(payload_size_bytes));
            block.payload_size = std::bit_cast<decltype(block.payload_size), decltype(payload_size_bytes)>(
                payload_size_bytes);
            block.payload_size = htonl(block.payload_size);
            header_it += sizeof(block.payload_size);
            // reserve and read as many bytes into the payload as possible
            block.payload.reserve(block.payload_size);
            block.payload.insert(std::end(block.payload), header_it, std::cend(buffer));
            return block.payload.size();
        }

        if (std::cmp_less(payload_offset, block.payload_size)) {
            block.payload.insert(std::end(block.payload), std::cbegin(buffer), std::cend(buffer));
            return block.payload.size() - payload_offset;
        }

        return 0;
    }

    std::vector<CharType> pack_block(const MessageBlock& block) {
        std::vector<CharType> block_data{};
        block_data.reserve(sizeof(block.HEADER) + sizeof(block.payload_size) + block.payload.size());
        block_data.insert(std::end(block_data), std::cbegin(block.HEADER), std::cend(block.HEADER));
        const auto payload_size_bytes = std::bit_cast<std::array<CharType, sizeof(block.payload_size)>, decltype(
            block.payload_size)>(
            ntohl(block.payload_size));
        block_data.insert(std::end(block_data), std::cbegin(payload_size_bytes), std::cend(payload_size_bytes));
        block_data.insert(std::end(block_data), std::cbegin(block.payload), std::cend(block.payload));
        return block_data;
    }

    MessageBlock pack_message(const Message& message) {
        MessageBlock block{};
        json group_json = json::array();
        group_json.push_back(message_to_json(message));
        block.set_payload(json::to_msgpack(group_json));
        return block;
    }

    std::vector<MessageBlock> pack_messages(const std::vector<Message>& messages) {
        std::vector<MessageBlock> blocks{};
        if (messages.empty()) {
            return blocks;
        }

        json group_json = json::array();
        for (const Message& message : messages) {
            group_json.push_back(message_to_json(message));
        }

        MessageBlock block{};
        block.set_payload(json::to_msgpack(group_json));
        blocks.push_back(std::move(block));

        return blocks;
    }

    std::vector<Message> unpack_messages(const MessageBlock& block) {
        std::vector<Message> messages{};
        if (std::cmp_less(block.payload_size, 1)) {
            return messages;
        }

        const json group_json = json::from_msgpack(block.payload);
        if (group_json.is_array()) {
            for (const json& message_json : group_json) {
                Message message{};
                message.message_type = message_json["message_type"].get<MessageType>();
                message.values = message_json["values"];
                messages.push_back(std::move(message));
            }
        }

        return messages;
    }

    int32_t add_room_history_event(Message& room_history_message,
        int32_t timestamp, std::string_view origin_user_name, std::string_view text) {
        assert(room_history_message.message_type == MessageType::ROOM_HISTORY);
        if (!room_history_message.values.contains("events")) {
            room_history_message.values["events"] = nlohmann::json::array();
        }
        assert(room_history_message.values["events"].is_array());

        json event_json = json::object();
        event_json["timestamp"] = timestamp;
        event_json["user_name"] = std::string{origin_user_name};
        event_json["text"] = std::string{text};
        room_history_message.values["events"].push_back(std::move(event_json));

        int32_t event_count = room_history_message.values.value("event_count", 0);
        ++event_count;
        assert(std::cmp_less_equal(event_count, ROOM_HISTORY_MAX_ENTRIES));
        room_history_message.values["event_count"] = event_count;
        return event_count;
    }
}
