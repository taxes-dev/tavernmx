#include <algorithm>
#include <bit>
#include <cassert>
#include <functional>
#include "nlohmann/json.hpp"
#include "tavernmx/messaging.h"
#include "tavernmx/platform.h"

#ifdef TMX_WINDOWS
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

using json = nlohmann::json;

namespace {
    constexpr size_t TARGET_BLOCK_SIZE = 1400;

    json message_to_json(const tavernmx::messaging::Message& message) {
        json message_json = json::object();
        message_json["message_type"] = message.message_type;
        message_json["values"] = json::object();
        for (auto& data: message.values) {
            switch (data.second.index()) {
                case 0:
                    message_json["values"].push_back({data.first, std::get<std::string>(data.second)});
                    break;
                case 1:
                    message_json["values"].push_back({data.first, std::get<int32_t>(data.second)});
                    break;
                case 2:
                    message_json["values"].push_back({data.first, std::get<bool>(data.second)});
                    break;
                default:
                    assert(false && "Unknown value variant index");
            }
        }
        return message_json;
    }
}

namespace tavernmx::messaging {
    size_t
    apply_buffer_to_block(const std::span<CharType>& buffer, MessageBlock& block, size_t payload_offset) {
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
            const size_t remaining = buffer.size() - (header_it - std::cbegin(buffer));
            if (remaining < sizeof(block.payload_size)) {
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

        if (payload_offset < block.payload_size) {
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
        block.payload = json::to_msgpack(group_json);
        block.payload_size = block.payload.size();
        return block;
    }

    std::vector<MessageBlock> pack_messages(const std::vector<Message>& messages) {
        std::vector<MessageBlock> blocks{};
        if (messages.empty()) {
            return blocks;
        }

        json group_json = json::array();
        for (auto& message: messages) {
            group_json.push_back(message_to_json(message));
        }

        //TODO: split blocks by size
        MessageBlock block{};
        block.payload = json::to_msgpack(group_json);
        block.payload_size = static_cast<int32_t>(block.payload.size());
        blocks.push_back(std::move(block));

        return blocks;
    }

    std::vector<Message> unpack_messages(const MessageBlock& block) {
        std::vector<Message> messages{};
        if (block.payload_size < 1) {
            return messages;
        }

        auto group_json = json::from_msgpack(block.payload);
        if (group_json.is_array()) {
            for (auto& message_json: group_json) {
                Message message{};
                message.message_type = message_json["message_type"].get<MessageType>();
                if (message_json["values"].is_object()) {
                    for (auto& values_json: message_json["values"].items()) {
                        if (values_json.value().is_string()) {
                            message.values[values_json.key()] = values_json.value().get<std::string>();
                        } else if (values_json.value().is_number_integer()) {
                            message.values[values_json.key()] = values_json.value().get<int32_t>();
                        } else if (values_json.value().is_boolean()) {
                            message.values[values_json.key()] = values_json.value().get<bool>();
                        } else {
                            assert(false && "Unexpected type in message values");
                        }
                    }
                }
                messages.push_back(std::move(message));
            }
        }

        return messages;
    }
}
