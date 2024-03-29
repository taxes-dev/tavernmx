#include <catch.hpp>
#include "tavernmx/util.h"

using namespace tavernmx;
using namespace std::string_literals;
using Catch::Matchers::RangeEquals;

TEST_CASE("str_tolower") {
    REQUIRE(str_tolower("HELLO"s) == "hello"s);
    REQUIRE(str_tolower("Hello World"s) == "hello world"s);
    REQUIRE(str_tolower("hello"s) == "hello"s);
    REQUIRE(str_tolower("12341234"s) == "12341234"s);
}

TEST_CASE("tokenize_string") {
    std::vector<std::string> output{};
    tokenize_string("hello"s, ' ', output);
    REQUIRE(output.size() == 1);
    REQUIRE(output[0] == "hello"s);
    output.clear();

    tokenize_string("hello world this is a string"s, ' ', output);
    REQUIRE(output.size() == 6);
    REQUIRE_THAT(output, RangeEquals<decltype(output)>({"hello"s, "world"s, "this"s, "is"s, "a"s, "string"s}));
    output.clear();

    tokenize_string(""s, ' ', output);
    REQUIRE(output.empty());
}
