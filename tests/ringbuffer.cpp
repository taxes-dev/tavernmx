#include <array>
#include <utility>
#include <catch.hpp>
#include "tavernmx/ringbuffer.h"

using Catch::Matchers::RangeEquals;
using tavernmx::RingBuffer;

TEST_CASE("RingBuffer: construct/capacity/empty/size check") {
	RingBuffer<int32_t, 100> buffer{};
	REQUIRE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.capacity(), 100));
	REQUIRE(buffer.empty());
	REQUIRE_FALSE(buffer.full());
}

TEST_CASE("RingBuffer: insert to less than capacity") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	REQUIRE_FALSE(buffer.full());
	for (int32_t i = 0; std::cmp_less(i, 5); i++) {
		buffer.insert(i);
	}
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 5));
	REQUIRE(std::cmp_equal(*buffer.tail(), 0));
}

TEST_CASE("RingBuffer: insert to capacity without wrap") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	REQUIRE_FALSE(buffer.full());
	for (int32_t i = 0; std::cmp_less(i, buffer.capacity() - 1); ++i) {
		buffer.insert(i);
	}
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 10));
	REQUIRE(std::cmp_equal(*buffer.tail(), 0));
}

TEST_CASE("RingBuffer: insert to capacity with wrap") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	REQUIRE_FALSE(buffer.full());
	for (int32_t i = 0; std::cmp_less(i, 15); ++i) {
		buffer.insert(i);
	}

	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(buffer.full());
	REQUIRE(std::cmp_equal(buffer.size(), 10));
	// container should look like this at this point:
	// { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
	//                head^     ^tail
	REQUIRE(std::cmp_equal(*buffer.tail(), 6));
}

TEST_CASE("RingBuffer: forward iteration, empty") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	for (const int32_t& elem : buffer) {
		FAIL();
	}
}

TEST_CASE("RingBuffer: forward iteration, some elements") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	for (int32_t i = 0; std::cmp_less(i, 5); ++i) {
		buffer.insert(i);
	}
	REQUIRE_FALSE(buffer.full());
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 5));
	//REQUIRE_THAT(buffer, RangeEquals<std::vector<int32_t>>({ 0, 1, 2, 3, 4 }));
    REQUIRE_THAT(buffer, RangeEquals(std::to_array<int32_t>({ 0, 1, 2, 3, 4 })));
}

TEST_CASE("RingBuffer: forward iteration, wrapped") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	for (int32_t i = 0; std::cmp_less(i, 15); ++i) {
		buffer.insert(i);
	}
	REQUIRE(buffer.full());
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 10));
	// container should look like this at this point:
	// { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
	//                head^     ^tail
	//REQUIRE_THAT(buffer, RangeEquals<std::vector<int32_t>>({ 6, 7, 8, 9, 10, 11, 12, 13, 14 }));
    REQUIRE_THAT(buffer, RangeEquals(std::to_array<int32_t>({ 6, 7, 8, 9, 10, 11, 12, 13, 14 })));
}

TEST_CASE("RingBuffer: reverse iteration, some elements") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	for (int32_t i = 0; std::cmp_less(i, 5); ++i) {
		buffer.insert(i);
	}
	REQUIRE_FALSE(buffer.full());
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 5));
	int32_t countdown = 5;
	for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
		REQUIRE(--countdown == *it);
	}
}

TEST_CASE("RingBuffer: reverse iteration, wrapped") {
	RingBuffer<int32_t, 10> buffer{};
	REQUIRE(std::cmp_equal(buffer.capacity(), 10));
	REQUIRE(buffer.empty());
	for (int32_t i = 0; std::cmp_less(i, 15); ++i) {
		buffer.insert(i);
	}
	REQUIRE(buffer.full());
	REQUIRE_FALSE(buffer.empty());
	REQUIRE(std::cmp_equal(buffer.size(), 10));
	// container should look like this at this point:
	// { 10, 11, 12, 13, 14, 5, 6, 7, 8, 9 }
	//                head^     ^tail
	int32_t countdown = 15;
	for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
		REQUIRE(--countdown == *it);
	}
}
