#pragma once
#include <memory>
#include <string>
#include <vector>

namespace tavernmx::rooms {
    class Room {
    public:
        explicit Room(std::string room_name) : _room_name{std::move(room_name)} {
        };

        Room(const Room&) = delete;

        Room(Room&&) = default;

        Room& operator=(const Room&) = delete;

        Room& operator=(Room&&) = default;

        const std::string& room_name() const { return this->_room_name; };

    private:
        std::string _room_name{};
    };

    class RoomManager {
    public:
        RoomManager() = default;

        RoomManager(const RoomManager&) = delete;

        RoomManager(RoomManager&&) = default;

        RoomManager& operator=(const RoomManager&) = delete;

        RoomManager& operator=(RoomManager&&) = default;

        std::shared_ptr<Room> create_room(const std::string& room_name) {
            return this->active_rooms.emplace_back(std::move(std::make_shared<Room>(room_name)));
        }

        const std::vector<std::shared_ptr<Room>>& rooms() const {
            return this->active_rooms;
        }

        std::shared_ptr<Room> operator[](const std::string& room_name) const {
            auto it = std::find_if(std::cbegin(this->active_rooms), std::cend(this->active_rooms),
                                   [&room_name](const std::shared_ptr<Room>& room) {
                                       return room->room_name() == room_name;
                                   });
            if (it != std::cend(this->active_rooms)) {
                return *it;
            }
            return nullptr;
        }

    private:
        std::vector<std::shared_ptr<Room>> active_rooms{};
        std::vector<std::string> room_names{};
    };
}
