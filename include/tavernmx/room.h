#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "queue.h"

namespace tavernmx::rooms
{
    using EventTimeStamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

    struct RoomEvent
    {
        EventTimeStamp timestamp{ time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()) };
        std::string origin_user_name{};
        std::string event_text{};
    };

    class Room
    {
    public:
        ThreadSafeQueue<RoomEvent> events{};

        explicit Room(std::string room_name)
            : _room_name{ std::move(room_name) } {
        };

        Room(const Room&) = delete;

        Room(Room&&) = default;

        Room& operator=(const Room&) = delete;

        Room& operator=(Room&&) = default;

        const std::string& room_name() const { return this->_room_name; };

    private:
        std::string _room_name{};
    };

    class RoomManager
    {
    public:
        RoomManager() = default;

        RoomManager(const RoomManager&) = delete;

        RoomManager(RoomManager&&) = default;

        RoomManager& operator=(const RoomManager&) = delete;

        RoomManager& operator=(RoomManager&&) = default;

        std::shared_ptr<Room> create_room(const std::string& room_name) {
            this->_room_names.push_back(room_name);
            return this->active_rooms.emplace_back(std::move(std::make_shared<Room>(room_name)));
        }

        const std::vector<std::shared_ptr<Room>>& rooms() const {
            return this->active_rooms;
        }

        const std::vector<std::string>& room_names() const {
            return this->_room_names;
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

        void clear() {
            this->active_rooms.clear();
            this->_room_names.clear();
        }

        size_t size() const {
            return this->active_rooms.size();
        }

    private:
        std::vector<std::shared_ptr<Room>> active_rooms{};
        std::vector<std::string> _room_names{};
    };
}
