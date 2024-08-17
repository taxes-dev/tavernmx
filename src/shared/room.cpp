#include "tavernmx/room.h"

namespace tavernmx::rooms
{
	bool is_valid_room_name(std::string_view room_name) {
		if (room_name.empty() || room_name.length() > MAX_ROOM_NAME_SIZE) {
			return false;
		}
		if (room_name.front() == '-' || room_name.back() == '-') {
			return false;
		}
		return std::all_of(
			std::cbegin(room_name), std::cend(room_name),
			[](const unsigned char c) { return isalnum(c) || c == '-'; });
	}
}