#include "tavernmx/messaging.h"
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <cassert>

namespace tavernmx::messaging {
    size_t
    apply_buffer_to_block(const unsigned char *buffer, size_t bufsize, MessageBlock &block, size_t payload_offset) {
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
            size_t bytes_to_copy = std::min(remaining, static_cast<size_t>(block.payload_size));
            std::memcpy(block.payload.data(), header1, bytes_to_copy);
            return bytes_to_copy;
        } else if (payload_offset < block.payload_size) {
            assert(payload_offset < block.payload.size());
            size_t bytes_to_copy = std::min(block.payload_size - payload_offset, bufsize);
            std::memcpy(block.payload.data() + payload_offset, buffer, bytes_to_copy);
            return bytes_to_copy;
        }
        return 0;
    }

    std::vector<unsigned char> pack_block(const MessageBlock &block) {
        std::vector<unsigned char> block_data{};
        block_data.reserve(sizeof(block.HEADER) + sizeof(block.payload_size) + block.payload.size());
        block_data.insert(std::end(block_data), std::begin(block.HEADER), std::end(block.HEADER));
        int32_t block_size = htonl(block.payload_size);
        auto block_size_bytes = reinterpret_cast<const unsigned char *>(&block_size);
        for (int32_t i = 0; i < sizeof(block.payload_size); i++) {
            block_data.push_back(block_size_bytes[i]);
        }
        block_data.insert(std::end(block_data), std::begin(block.payload), std::end(block.payload));
        return block_data;
    }
}