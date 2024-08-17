#pragma once
#include <string>
#include <vector>

namespace tavernmx
{
	/**
     * @brief Converts a string to lowercase.
     * @param s std::string_view
     * @return std::string
     */
	std::string str_tolower(std::string_view s);

	/**
     * @brief Split \p input into tokens based on the given \p delimiter.
     * @param input std::string_view
     * @param delimiter char which determines the boundard between tokens.
     * @param output std::vector<std::string> which will receive the tokens.
     * @note Any existing data in \p output will not be altered; tokens are appended
     * to the end of the container.
     */
	void tokenize_string(std::string_view input, char delimiter, std::vector<std::string>& output);
}
