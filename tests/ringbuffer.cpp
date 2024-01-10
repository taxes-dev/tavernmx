#include <catch.hpp>
#include "tavernmx/ringbuffer.h"

using Catch::Matchers::RangeEquals;
using tavernmx::RingBuffer;

TEST_CASE("RingBuffer: construct/capacity/empty/size check") {
    RingBuffer<int32_t, 100> buffer{};
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.capacity() == 100);
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
}

TEST_CASE("RingBuffer: insert to less than capacity") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
    for (int32_t i = 0; i < 5; i++) {
        buffer.insert(i);
    }
    REQUIRE(buffer.capacity() == 10);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 5);
    REQUIRE(*buffer.tail() == 0);
}

TEST_CASE("RingBuffer: insert to capacity without wrap") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
    for (int32_t i = 0; i < buffer.capacity() - 1; ++i) {
        buffer.insert(i);
    }
    REQUIRE(buffer.capacity() == 10);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 10);
    REQUIRE(*buffer.tail() == 0);
}

TEST_CASE("RingBuffer: insert to capacity with wrap") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
    for (int32_t i = 0; i < 15; ++i) {
        buffer.insert(i);
    }

    REQUIRE(buffer.capacity() == 10);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.full());
    REQUIRE(buffer.size() == 10);
    // container should look like this at this point:
    // { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
    //                head^     ^tail
    REQUIRE(*buffer.tail() == 6);
}

TEST_CASE("RingBuffer: forward iteration, empty") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    for (auto& elem : buffer) {
        FAIL();
    }
}

TEST_CASE("RingBuffer: forward iteration, some elements") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    for (int32_t i = 0; i < 5; ++i) {
        buffer.insert(i);
    }
    REQUIRE_FALSE(buffer.full());
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 5);
    REQUIRE_THAT(buffer, RangeEquals<std::vector<int32_t>>({0, 1, 2, 3, 4}));
}

TEST_CASE("RingBuffer: forward iteration, wrapped") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    for (int32_t i = 0; i < 15; ++i) {
        buffer.insert(i);
    }
    REQUIRE(buffer.full());
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 10);
    // container should look like this at this point:
    // { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
    //                head^     ^tail
    REQUIRE_THAT(buffer, RangeEquals<std::vector<int32_t>>({6, 7, 8, 9, 10, 11, 12, 13, 14}));
}

TEST_CASE("RingBuffer: reverse iteration, some elements") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    for (int32_t i = 0; i < 5; ++i) {
        buffer.insert(i);
    }
    REQUIRE_FALSE(buffer.full());
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 5);
    int32_t countdown = 5;
    for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
        REQUIRE(--countdown == *it);
    }
}

TEST_CASE("RingBuffer: reverse iteration, wrapped") {
    RingBuffer<int32_t, 10> buffer{};
    REQUIRE(buffer.capacity() == 10);
    REQUIRE(buffer.empty());
    for (int32_t i = 0; i < 15; ++i) {
        buffer.insert(i);
    }
    REQUIRE(buffer.full());
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 10);
    // container should look like this at this point:
    // { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
    //                head^     ^tail
    int32_t countdown = 15;
    for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
        REQUIRE(--countdown == *it);
    }
}
