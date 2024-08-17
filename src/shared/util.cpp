#include <algorithm>
#include <string>
#include <vector>
#include "tavernmx/shared.h"

namespace tavernmx
{
	void tokenize_string(std::string_view input, char delimiter, std::vector<std::string>& output) {
		if (input.empty()) {
			return;
		}

		std::string token{};
		token.reserve(input.size());
		for (const char c : input) {
			if (c == delimiter && !token.empty()) {
				output.push_back(token);
				token.clear();
			} else if (c != delimiter) {
				token.push_back(c);
			}
		}
		if (!token.empty()) {
			output.push_back(token);
		}
	}

	std::string str_tolower(std::string_view s) {
		std::string result{};
		result.reserve(s.size());
		std::transform(
			std::cbegin(s), std::cend(s), std::back_inserter(result),
			[](unsigned char c) { return std::tolower(c); });
		return result;
	}
}
