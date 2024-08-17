#pragma once
#include <algorithm>
#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tavernmx::rooms
{
    /// Maximum number of characters allowed in room names.
    constexpr size_t MAX_ROOM_NAME_SIZE = 25;

    /// Time stamp type for room events.
    using EventTimeStamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

    /**
     * @brief Checks if \p room_name is in a valid format.
     * @param room_name The room name.
     * @return true if \p room_name can be used for a chat room name, otherwise false.
     * @note Rules:
     *  - \p room_name cannot be empty or larger than MAX_ROOM_NAME_SIZE.
     *  - \p room_name can only contain alphanumeric characters or hyphen (-).
     *  - \p room_name can only begin or end with alphanumeric characters.
     */
    bool is_valid_room_name(std::string_view room_name);

    /**
     * @brief Describes an individual event that occurred in a chat room.
     */
    struct RoomEvent
    {
        /// Timestamp of the event.
        EventTimeStamp timestamp{ time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()) };

        /// The user that originated the event, if any.
        std::string origin_user_name{};

        /// Event text to be displayed, if any.
        std::string event_text{};
    };

    /**
     * @brief Base class for chat rooms.
     */
    class Room
    {
    public:
        /**
         * @brief Create a Room.
         * @param room_name The room's unique name.
         */
        explicit Room(std::string_view room_name)
            : _room_name{ std::string{room_name} } {
        };

        virtual ~Room() = default;

        Room(const Room&) = delete;

        Room(Room&&) = default;

        Room& operator=(const Room&) = delete;

        Room& operator=(Room&&) = default;

        /**
         * @brief Get the room's name.
         * @return std::string
         */
        const std::string& room_name() const { return this->_room_name; };

        /**
         * @brief Should this room be destroyed?
         * @return true if the room should be destroyed, otherwise false.
         */
        bool is_destroy_requested() const {
            return this->destroy_requested;
        }

        /**
         * @brief Mark this chat room to be destroyed.
         * @note If overridden, derived classes should call the base class.
         */
        virtual void request_destroy() {
            this->destroy_requested = true;
        }

    private:
        std::string _room_name{};
        bool destroy_requested{ false };
    };

    /**
     * @brief Concept describing classes which derive from Room so they can be used with
     * the RoomManager.
     * @tparam TRoom derived type
     */
    template <typename TRoom>
    concept IsRoom = requires(std::string room_name)
    {
        std::derived_from<TRoom, Room>;
        std::move_constructible<TRoom>;
        TRoom(room_name);
    };

    /**
     * @brief Manages a set of chat rooms.
     * @tparam T Type derived from Room and constrained by IsRoom<T>.
     */
    template <typename T>
        requires IsRoom<T>
    class RoomManager
    {
    public:
        /**
         * @brief Creates a RoomManager.
         */
        RoomManager() noexcept = default;

        RoomManager(const RoomManager&) = delete;

        RoomManager(RoomManager&&) = default;

        RoomManager& operator=(const RoomManager&) = delete;

        RoomManager& operator=(RoomManager&&) = default;

        /**
         * @brief Creates a new room with name \p room_name and adds it to this manager.
         * @param room_name The unique room name to use.
         * @return A std::shared_ptr<T> if the room was created, otherwise nullptr.
         * @note \p room_name must be unique to other rooms currently owned by the
         * RoomManager and be a valid room name (see is_valid_room_name(std::string)).
         */
        std::shared_ptr<T> create_room(std::string_view room_name) {
            if (!is_valid_room_name(room_name)) {
                return nullptr;
            }
            if (std::find(std::cbegin(this->_room_names), std::cend(this->_room_names), room_name) !=
                std::cend(this->_room_names)) {
                return nullptr;
            }
            this->_room_names.emplace_back(room_name);
            return this->active_rooms.emplace_back(std::move(std::make_shared<T>(room_name)));
        }

        /**
         * @brief Get the current set of active rooms.
         * @return std::vector<std::shared_ptr<T>>
         */
        const std::vector<std::shared_ptr<T>>& rooms() const {
            return this->active_rooms;
        }

        /**
         * @brief Get the current set of active room names.
         * @return std::vector<std::string>
         */
        const std::vector<std::string>& room_names() const {
            return this->_room_names;
        }

        /**
         * @brief Attempt to retrieve a specific chat room by name.
         * @param room_name The unique name of the chat room.
         * @return A std::shared_ptr<T> if the room is found, otherwise nullptr.
         */
        std::shared_ptr<T> operator[](std::string_view room_name) const {
            auto it = std::find_if(std::cbegin(this->active_rooms), std::cend(this->active_rooms),
                [&room_name](const std::shared_ptr<T>& room) {
                    return room->room_name() == room_name;
                });
            if (it != std::cend(this->active_rooms)) {
                return *it;
            }
            return nullptr;
        }

        /**
         * @brief Remove all active chat rooms.
         */
        void clear() {
            this->active_rooms.clear();
            this->_room_names.clear();
        }

        /**
         * @brief Remove all rooms marked for destruction.
         */
        void remove_destroyed_rooms() {
            if (std::erase_if(this->active_rooms, [](const std::shared_ptr<Room>& room) {
                return room->is_destroy_requested();
            }) > 0) {
                this->_room_names.clear();
                for (std::shared_ptr<T>& room : this->active_rooms) {
                    this->_room_names.push_back(room->room_name());
                }
            }
        }

        /**
         * @brief Get the number of active chat rooms.
         * @return size_t
         */
        size_t size() const {
            return this->active_rooms.size();
        }

    private:
        std::vector<std::shared_ptr<T>> active_rooms{};
        std::vector<std::string> _room_names{};
    };
}
