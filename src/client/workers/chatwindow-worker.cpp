#include <semaphore>
#include <fmt/chrono.h>
#include "tavernmx/client-workers.h"

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;

/// Signals that the connection to the server has been ended.
extern std::binary_semaphore connection_ended_signal;
/// This flag will be set to let the UI know we're waiting on a response from the server.
extern bool waiting_on_server;

namespace
{
    /// Manages chat rooms for the client while it's connected.
    RoomManager<ClientRoom> client_rooms{};

    /**
     * @brief Any time the room list is altered, we may need to rejoin a requested room.
     * @param room_name The unique room name to (potentially) join.
     * @param messages_out Outbound message queue.
     * @note Will not do anything if \p room_name is empty or if we're already joined to \p room_name.
     */
    void issue_room_join_if_needed(const std::string& room_name, tavernmx::ThreadSafeQueue<Message>* messages_out) {
        if (room_name.empty()) {
            return;
        }
        const auto selected_room = client_rooms[room_name];
        if (selected_room && !selected_room->is_joined) {
            TMX_INFO("Join issued for room: {}", selected_room->room_name());
            messages_out->push(create_room_join(selected_room->room_name()));
            selected_room->is_joined = true;
        }
    }
}

namespace tavernmx::client
{

    std::string room_event_to_string(const RoomEvent& event) {
        return fmt::format("[{:%H:%M}] {}: {}",
            fmt::localtime(event.timestamp.time_since_epoch().count()),
            event.origin_user_name,
            event.event_text);
    }

    void chat_window_worker(std::unique_ptr<ServerConnection> connection, ChatWindowScreen* screen) {
        auto messages_in = connection->messages_in;
        auto messages_out = connection->messages_out;

        // update loop to handle incoming messages
        screen->add_handler(ChatWindowScreen::MSG_UPDATE,
            [messages_in, messages_out](ClientUi* ui, ClientUiScreen* screen) {
                // are we still connected?
                if (connection_ended_signal.try_acquire()) {
                    ui->pop_screen();
                    ui->set_error("Connection to server lost.");
                    return;
                }
                const auto chat_screen = dynamic_cast<ChatWindowScreen*>(screen);

                // check server status
                chat_screen->waiting_on_server = waiting_on_server;

                // process incoming messages
                while (auto msg = messages_in->pop()) {
                    TMX_INFO("UI message: {}", static_cast<int32_t>(msg->message_type));
                    switch (msg->message_type) {
                    case MessageType::ROOM_LIST: {
                        std::string current_room_name = chat_screen->current_room_name;
                        client_rooms.clear();
                        for (auto& [key, value] : msg->values) {
                            if (const auto room = client_rooms.create_room(std::get<std::string>(value))) {
                                TMX_INFO("Created room: #{}", room->room_name());
                            } else {
                                TMX_WARN("Room already exists: #{}", std::get<std::string>(value));
                            }
                        }
                        chat_screen->update_rooms(client_rooms.room_names());

                        // rejoin previously selected room if it still exists, otherwise will default to first room
                        chat_screen->select_room_by_name(current_room_name);
                        issue_room_join_if_needed(chat_screen->current_room_name, messages_out.get());
                    }
                    break;
                    case MessageType::ROOM_CREATE: {
                        auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                        std::string current_room_name = chat_screen->current_room_name;
                        if (const auto room = client_rooms.create_room(room_name)) {
                            TMX_INFO("Created room: #{}", room->room_name());
                            chat_screen->update_rooms(client_rooms.room_names());

                            // re-select previously selected room if it still exists, otherwise will default to first room
                            chat_screen->select_room_by_name(current_room_name);
                            issue_room_join_if_needed(chat_screen->current_room_name, messages_out.get());
                        } else {
                            TMX_WARN("Room already exists: #{}", room_name);
                        }
                    }
                    break;
                    case MessageType::ROOM_DESTROY: {
                        auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                        std::string current_room_name = chat_screen->current_room_name;
                        if (auto room = client_rooms[room_name]) {
                            TMX_INFO("Destroyed room: #{}", room->room_name());
                            room->request_destroy();
                            client_rooms.remove_destroyed_rooms();
                            chat_screen->update_rooms(client_rooms.room_names());
                            if (current_room_name == room_name) {
                                // Room we were in was destroyed, select a new one
                                issue_room_join_if_needed(chat_screen->current_room_name, messages_out.get());
                            } else {
                                // Re-select previous room
                                chat_screen->select_room_by_name(current_room_name);
                            }
                        }
                    }
                    break;
                    case MessageType::CHAT_ECHO: {
                        auto room_name = message_value_or<std::string>(*msg, "room_name");
                        RoomEvent event{
                            .timestamp = EventTimeStamp{
                                std::chrono::seconds{ message_value_or(*msg, "timestamp"s, 0) } },
                            .event_type = RoomEvent::ChatMessage,
                            .origin_user_name = message_value_or(*msg, "user_name"s, "(unknown)"s),
                            .event_text = message_value_or(*msg, "text"s, ""s),
                        };
                        TMX_INFO("CHAT_ECHO: {}", room_name);
                        TMX_INFO("timestamp: {}", event.timestamp.time_since_epoch().count());
                        TMX_INFO("origin_user_name: {}", event.origin_user_name);
                        TMX_INFO("event_text: {}", event.event_text);
                        if (auto room = client_rooms[room_name]) {
                            // TODO: placeholder
                            chat_screen->chat_display += "\n"s + room_event_to_string(event);

                            room->events.push_back(std::move(event));
                        }
                    }
                    break;
                    default:
                        TMX_WARN("Unhandled UI message type: {}", static_cast<int32_t>(msg->message_type));
                        break;
                    }
                }
            });
        screen->add_handler(ChatWindowScreen::MSG_ROOM_CHANGED, [messages_out](ClientUi*, ClientUiScreen* screen) {
            const auto chat_screen = dynamic_cast<ChatWindowScreen*>(screen);
            TMX_INFO("Chat room changed: {}", chat_screen->current_room_name);
            issue_room_join_if_needed(chat_screen->current_room_name, messages_out.get());
        });
        screen->add_handler(ChatWindowScreen::MSG_CHAT_SUBMIT, [messages_out](ClientUi*, ClientUiScreen* screen) {
            const auto chat_screen = dynamic_cast<ChatWindowScreen*>(screen);
            if (chat_screen->chat_input.empty()) {
                return;
            }
            TMX_INFO("Chat entry: {}", chat_screen->chat_input);
            if (chat_screen->chat_input[0] == '/') {
                // chat command
                std::vector<std::string> tokens{};
                tokenize_string(chat_screen->chat_input, ' ', tokens);
                const std::string command = str_tolower(tokens[0]);
                if (command == "/create_room"s) {
                    if (tokens.size() != 2) {
                        TMX_WARN("Usage: /create_room <room_name>");
                    } else if (is_valid_room_name(tokens[1])) {
                        messages_out->push(create_room_create(tokens[1]));
                    } else {
                        TMX_WARN("create_room: '{}' is not a valid room name", tokens[1]);
                    }
                } else if (command == "/destroy_room"s) {
                    if (tokens.size() != 2) {
                        TMX_WARN("Usage: /destroy_room <room_name>");
                    } else if (const auto room_to_destroy = client_rooms[tokens[1]]) {
                        messages_out->push(create_room_destroy(room_to_destroy->room_name()));
                    } else {
                        TMX_WARN("destroy_room: '{}' is not a valid room name", tokens[1]);
                    }
                } else {
                    TMX_WARN("Unkown chat command: {}", command);
                }
            } else if (!chat_screen->current_room_name.empty()) {
                // chat text
                messages_out->push(create_chat_send(chat_screen->current_room_name, chat_screen->chat_input));
            } else {
                TMX_WARN("No room selected.");
            }
            chat_screen->chat_input.clear();
        });

        // push connection to background thread for message handling
        std::thread connection_thread{ server_message_worker, std::move(connection) };
        connection_thread.detach();
    }

}
