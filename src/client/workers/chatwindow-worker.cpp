#include <semaphore>
#include <fmt/chrono.h>
#include "tavernmx/client-workers.h"

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;

/// Signals that the connection to the server has been ended.
extern std::binary_semaphore connection_ended_signal;
/// Signals that the connection to the server should be cleanly shutdown.
extern std::binary_semaphore shutdown_connection_signal;
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
        const std::shared_ptr<ClientRoom> selected_room = client_rooms[room_name];
        if (selected_room && !selected_room->is_joined) {
            TMX_INFO("Join issued for room: {}", selected_room->room_name());
            messages_out->push(create_room_join(selected_room->room_name()));
            selected_room->is_joined = true;
            TMX_INFO("Requesting room history for room: {}", selected_room->room_name());
            messages_out->push(create_room_history(selected_room->room_name()));
        }
    }

    /**
     * @brief Convert the JSON for a single room event into a ClientRoomEvent struct.
     * @param event_json nlohmann::json object containing room event data.
     * @return ClientRoomEvent
     */
    ClientRoomEvent event_json_to_room_event(const json& event_json) {
        assert(event_json.contains("timestamp"s) &&
            event_json.contains("user_name"s) &&
            event_json.contains("timestamp"s));
        return {
            {
                .timestamp = EventTimeStamp{
                    std::chrono::seconds{ event_json.value("timestamp"s, 0) } },
                .origin_user_name = event_json.value("user_name"s, ""s),
                .event_text = event_json.value("text", ""s),
            },
            .timestamp_text = fmt::format("{:%0I:%M %p}",
                fmt::localtime(event_json.value("timestamp"s, 0)))
        };
    }

    /**
     * @brief Take a \p message of type ROOM_HISTORY and convert it back to a set of events.
     * @param message Message
     * @return std::vector<ClientRoomEvent>
     */
    std::vector<ClientRoomEvent> room_history_message_to_events(const Message& message) {
        const auto event_count = static_cast<size_t>(message_value_or<int32_t>(message, "event_count"s));
        std::vector<ClientRoomEvent> events{};
        if (message.values.contains("events"s)) {
            for (const auto& item : message.values["events"s].items()) {
                events.push_back(event_json_to_room_event(item.value()));
            }
        }
        if (event_count != events.size()) {
            TMX_WARN("Event count mismatch: {} vs {}", event_count, events.size());
        }
        return events;
    }
}

namespace tavernmx::client
{
    void chat_window_worker(std::unique_ptr<ServerConnection> connection, ChatWindowScreen* screen) {
        std::shared_ptr<ThreadSafeQueue<Message>> messages_in = connection->messages_in,
            messages_out = connection->messages_out;

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
                while (const std::optional<Message> msg = messages_in->pop()) {
                    TMX_INFO("UI message: {}", static_cast<int32_t>(msg->message_type));
                    switch (msg->message_type) {
                    case MessageType::ROOM_LIST: {
                        // current_room_name stored here since update_rooms() will modify it
                        const std::string current_room_name = chat_screen->current_room_name;
                        client_rooms.clear();
                        for (auto& [key, value] : msg->values.items()) {
                            if (const std::shared_ptr<ClientRoom> room =
                                client_rooms.create_room(value.get<std::string>())) {
                                TMX_INFO("Created room: #{}", room->room_name());
                            } else {
                                TMX_WARN("Room already exists: #{}", value.get<std::string>());
                            }
                        }
                        chat_screen->update_rooms(client_rooms.room_names());

                        // rejoin previously selected room if it still exists, otherwise will default to first room
                        chat_screen->select_room_by_name(current_room_name);
                        issue_room_join_if_needed(chat_screen->current_room_name, messages_out.get());
                    }
                    break;
                    case MessageType::ROOM_CREATE: {
                        const auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                        const std::string current_room_name = chat_screen->current_room_name;
                        if (const std::shared_ptr<ClientRoom> room = client_rooms.create_room(room_name)) {
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
                        const auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                        const std::string current_room_name = chat_screen->current_room_name;
                        if (std::shared_ptr<ClientRoom> room = client_rooms[room_name]) {
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
                    case MessageType::ROOM_HISTORY: {
                        const auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                        if (std::shared_ptr<ClientRoom> room = client_rooms[room_name]) {
                            const std::vector<ClientRoomEvent> events = room_history_message_to_events(*msg);
                            chat_screen->rewrite_chat_history(room->room_name(), std::cbegin(events),
                                std::cend(events));
                        }
                    }
                    break;
                    case MessageType::CHAT_ECHO: {
                        const auto room_name = message_value_or<std::string>(*msg, "room_name");
                        chat_screen->insert_chat_history_event(room_name,
                            event_json_to_room_event(msg->values));
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
                    } else if (const std::shared_ptr<ClientRoom> room_to_destroy = client_rooms[tokens[1]]) {
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

        screen->add_handler(ChatWindowScreen::MSG_CHAT_CLOSED, [](ClientUi* ui, ClientUiScreen*) {
            ui->pop_screen();
            shutdown_connection_signal.release();
        });

        // push connection to background thread for message handling
        std::thread connection_thread{ server_message_worker, std::move(connection) };
        connection_thread.detach();
    }

}
