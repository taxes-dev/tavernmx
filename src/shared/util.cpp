#include <algorithm>
#include <string>
#include <vector>
#include "tavernmx/shared.h"

namespace tavernmx
{
    void tokenize_string(const std::string& input, char delimiter, std::vector<std::string>& output) {
        if (input.empty()) {
            return;
        }

        std::string token{};
        token.reserve(input.size());
        for (auto c : input) {
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

    std::string str_tolower(std::string s) {
        std::transform(std::begin(s), std::end(s), std::begin(s),
            [](unsigned char c) { return std::tolower(c); }
            );
        return s;
    }
}
