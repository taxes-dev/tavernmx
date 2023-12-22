#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <cassert>
#include <iostream>
#include "nlohmann/json.hpp"
#include "tavernmx/messaging.h"

using json = nlohmann::json;

namespace {
    constexpr size_t TARGET_BLOCK_SIZE = 1400;
}

namespace tavernmx::messaging {
    size_t
    apply_buffer_to_block(const unsigned char* buffer, size_t bufsize, MessageBlock& block, size_t payload_offset) {
        if (buffer == nullptr || bufsize < 1) {
            return 0;
        }

        if (payload_offset == 0) {
            // look for the start of a message block
            auto header1 = static_cast<const unsigned char *>(std::memchr(buffer, static_cast<int>(block.HEADER[0]),
                                                                          bufsize));
            if (header1 == nullptr ||
                std::memcmp(header1, block.HEADER, sizeof(block.HEADER)) != 0) {
                return 0;
            }
            // found, extract the block size
            header1 += sizeof(block.HEADER);
            size_t remaining = bufsize - (header1 - buffer);
            if (remaining < sizeof(block.payload_size)) {
                return 0;
            }
            std::memcpy(&block.payload_size, header1, sizeof(block.payload_size));
            block.payload_size = ntohl(block.payload_size);
            header1 += sizeof(block.payload_size);
            remaining -= sizeof(block.payload_size);
            // resize and read as many bytes into the payload as possible
            block.payload.resize(block.payload_size);
            const size_t bytes_to_copy = std::min(remaining, static_cast<size_t>(block.payload_size));
            std::memcpy(block.payload.data(), header1, bytes_to_copy);
            return bytes_to_copy;
        } else if (payload_offset < block.payload_size) {
            assert(payload_offset < block.payload.size());
            const size_t bytes_to_copy = std::min(block.payload_size - payload_offset, bufsize);
            std::memcpy(block.payload.data() + payload_offset, buffer, bytes_to_copy);
            return bytes_to_copy;
        }
        return 0;
    }

    std::vector<unsigned char> pack_block(const MessageBlock& block) {
        std::vector<unsigned char> block_data{};
        block_data.reserve(sizeof(block.HEADER) + sizeof(block.payload_size) + block.payload.size());
        block_data.insert(std::end(block_data), std::begin(block.HEADER), std::end(block.HEADER));
        const int32_t block_size = htonl(block.payload_size);
        const auto block_size_bytes = reinterpret_cast<const unsigned char *>(&block_size);
        for (int32_t i = 0; i < sizeof(block.payload_size); i++) {
            block_data.push_back(block_size_bytes[i]);
        }
        block_data.insert(std::end(block_data), std::begin(block.payload), std::end(block.payload));
        return block_data;
    }

    std::vector<MessageBlock> pack_messages(const std::vector<Message>& messages) {
        std::vector<MessageBlock> blocks{};
        if (messages.empty()) {
            return blocks;
        }

        json group_json = json::array();
        for (auto& message: messages) {
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
            group_json.push_back(std::move(message_json));
        }
        std::cout << group_json << std::endl;

        //TODO: split blocks by size
        MessageBlock block{};
        block.payload = json::to_msgpack(group_json);
        block.payload_size = static_cast<int32_t>(block.payload.size());
        blocks.push_back(std::move(block));

        return blocks;
    }

    std::vector<Message> unpack_messages(const MessageBlock &block) {
        std::vector<Message> messages{};
        if (block.payload_size < 1) {
            return messages;
        }

        auto group_json = json::from_msgpack(block.payload);
        if (group_json.is_array()) {
            for (auto &message_json : group_json) {
                Message message{};
                message.message_type = message_json["message_type"].get<MessageType>();
                if (message_json["values"].is_object()) {
                    for (auto &values_json : message_json["values"].items()) {
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
