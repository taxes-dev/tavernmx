#include <catch.hpp>
#include "tavernmx/messaging.h"

using namespace tavernmx::messaging;
using namespace std::string_literals;
using Catch::Matchers::RangeEquals;

namespace tavernmx::messaging
{
    /// Heavyweight comparison, just for testing.
    inline bool operator==(const Message& lhs, const Message& rhs) {
        if (lhs.message_type != rhs.message_type) {
            return false;
        }
        std::vector<uint8_t> lhs_bytes = json::to_msgpack(lhs.values);
        std::vector<uint8_t> rhs_bytes = json::to_msgpack(rhs.values);
        REQUIRE_THAT(lhs_bytes, RangeEquals(rhs_bytes));
        return true;
    }
}

TEST_CASE("Pack and unpack simple single message") {
    const Message message = create_room_join("test"s);
    const MessageBlock block = pack_message(message);
    const std::vector<Message> messages_unpacked = unpack_messages(block);
    REQUIRE(messages_unpacked.size() == 1);
    REQUIRE(messages_unpacked[0] == message);
}

TEST_CASE("Pack and unpack large single message") {
    // create a single large message
    constexpr size_t NUM_ENTRIES = ROOM_HISTORY_MAX_ENTRIES;
    constexpr size_t STRING_SIZE = 1000;

    Message message = create_room_history("test"s, 0);
    for (size_t i = 0; i < NUM_ENTRIES; ++i) {
        add_room_history_event(message, static_cast<int32_t>(std::time(nullptr) + i),
            "test_user", std::string(STRING_SIZE, 'a'));
    }

    // pack into block
    MessageBlock block = pack_message(message);

    // unpack
    std::vector<Message> message_unpacked = unpack_messages(block);

    REQUIRE(message_unpacked.size() == 1);
    REQUIRE(message_unpacked[0].values["event_count"s] == NUM_ENTRIES);
    REQUIRE(message_unpacked[0] == message);
}

TEST_CASE("Pack and unpack multiple messages") {
    // create a set of large messages
    constexpr size_t NUM_MESSAGES = 10;
    constexpr size_t STRING_SIZE = 1000;

    std::vector<Message> messages{};
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
        messages.push_back(
            create_chat_send("test"s,
                std::string(STRING_SIZE, 'a'))
            );
    }

    // pack into a block
    MessageBlock block = pack_messages(std::cbegin(messages), std::cend(messages));

    // unpack
    std::vector<Message> messages_unpacked = unpack_messages(block);

    REQUIRE_THAT(messages_unpacked, RangeEquals(messages));
}
