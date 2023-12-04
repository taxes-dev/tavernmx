//
// Created by Taxes on 2023-12-04.
//

#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace tavernmx::messaging
{
    struct MessageBlock
    {
        const unsigned char HEADER[4] = {'t', 'm', 'x', 0x02};
        int32_t block_size = 0;
        std::vector<unsigned char> payload{};
    };

    size_t apply_buffer_to_block(const unsigned char * buffer, size_t bufsize, MessageBlock & block, size_t payload_offset = 0);

    std::vector<unsigned char> pack_block(const MessageBlock & block);
}