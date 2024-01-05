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

        /*size_t pos = 0;
        while (pos < input.length()) {
            const auto delim_pos = input.find(delimiter, pos);
            if (delim_pos == std::string::npos) {
                output.push_back(input.substr(pos));
                break;
            }
            output.push_back(input.substr(pos, delim_pos - pos));
            pos = delim_pos + 1;
        }
        std::erase_if(output, [delimiter](const std::string& s) {
            return std::all_of(std::cbegin(s), std::cend(s),
                [delimiter](char c) { return c == delimiter; });
        });*/
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
