#include "tavernmx/room.h"

namespace tavernmx::rooms
{
    bool is_valid_room_name(const std::string& room_name) {
        if (room_name.empty() || room_name.length() > MAX_ROOM_NAME_SIZE) {
            return false;
        }
        if (room_name[0] == '-' || room_name[room_name.length() - 1] == '-') {
            return false;
        }
        return std::all_of(std::cbegin(room_name), std::cend(room_name), [](unsigned char c) {
            return isalnum(c) || c == '-';
        });
    }
}