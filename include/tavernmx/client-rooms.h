#pragma once
#include <string>
#include <string_view>
#include "room.h"

namespace tavernmx::rooms
{
    /**
     * @brief Represents a chat room event on the client.
     */
    struct ClientRoomEvent : RoomEvent
    {
        /// Translation of timestamp into displayable format.
        std::string timestamp_text{};
    };

    /**
     * @brief Represents a chat room managed by the client.
     */
    class ClientRoom : public Room
    {
    public:
        /// Have we already requested to join this room?
        bool is_joined{};

        /**
         * @brief Create a ClientRoom.
         * @param room_name The room's unique name.
         */
        explicit ClientRoom(std::string_view room_name)
            : Room{ room_name } {
        };

        ClientRoom(const ClientRoom&) = delete;

        ClientRoom(ClientRoom&&) = default;

        ClientRoom& operator=(const ClientRoom&) = delete;

        ClientRoom& operator=(ClientRoom&&) = default;
    };

}
