#include <array>
#include <utility>
#include <catch.hpp>
#include "tavernmx/util.h"

using namespace tavernmx;
using namespace std::string_literals;
using Catch::Matchers::RangeEquals;

TEST_CASE("str_tolower") {
	REQUIRE(str_tolower("HELLO") == "hello"s);
	REQUIRE(str_tolower("Hello World") == "hello world"s);
	REQUIRE(str_tolower("hello") == "hello"s);
	REQUIRE(str_tolower("12341234") == "12341234"s);
}

TEST_CASE("tokenize_string") {
	std::vector<std::string> output{};
	tokenize_string("hello", ' ', output);
	REQUIRE(std::cmp_equal(output.size(), 1));
	REQUIRE(output[0] == "hello"s);
	output.clear();

	tokenize_string("hello world this is a string", ' ', output);
	REQUIRE(std::cmp_equal(output.size(), 6));
	//REQUIRE_THAT(output, RangeEquals<decltype(output)>({ "hello"s, "world"s, "this"s, "is"s, "a"s, "string"s }));
	REQUIRE_THAT(output, RangeEquals(std::to_array<std::string>({ "hello"s, "world"s, "this"s, "is"s, "a"s, "string"s })));
	output.clear();

	tokenize_string("", ' ', output);
	REQUIRE(output.empty());
}
