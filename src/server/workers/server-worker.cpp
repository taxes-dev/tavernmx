#include "tavernmx/server-workers.h"
#include <semaphore>

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;

/// Signals that the server work thread is ready to receive data.
std::binary_semaphore server_ready_signal{ 0 };
/// Signals that the main server thread has started accepting TLS connections.
std::binary_semaphore server_accept_signal{ 0 };
/// Signals that the server work thread would like the main server thread to shutdown.
std::binary_semaphore server_shutdown_signal{ 0 };

namespace
{
    /// Maximum amount of chat room history to track.
    constexpr size_t CHAT_ROOM_HISTORY_SIZE = 1000;
    using RoomHistory = std::unordered_map<std::string, tavernmx::RingBuffer<RoomEvent, CHAT_ROOM_HISTORY_SIZE>>;

    /// Target maximum ms for loop processing.
    constexpr std::chrono::milliseconds TARGET_SERVER_LOOP_MS{ 20ll };

    /// Convert RoomEvents in \p room into Message objects.
    std::vector<Message> room_events_to_messages(ServerRoom* room) {
        std::vector<Message> messages{};
        while (const std::optional<RoomEvent> event = room->events.pop()) {
            messages.push_back(create_chat_echo(room->room_name(), event->event_text, event->origin_user_name,
                static_cast<int32_t>(event->timestamp.time_since_epoch().count())));
        }
        return messages;
    }

    /// Record \p room_event as part of the history of \p room_name.
    void insert_event_into_room_history(RoomHistory& room_history, const std::string& room_name, RoomEvent room_event) {
        if (!room_history.contains(room_name)) {
            room_history[room_name] = {};
        }
        room_history[room_name].insert(std::move(room_event));
    }

    /// Pack the history for \p room_name into a Message.
    Message get_room_history(RoomHistory& room_history, const std::string& room_name, size_t max_event_count) {
        Message history_msg = create_room_history(room_name, 0);
        if (room_history.contains(room_name)) {
            for (RoomEvent& event : room_history[room_name]) {
                if (add_room_history_event(history_msg, static_cast<int32_t>(event.timestamp.
                        time_since_epoch().count()), event.origin_user_name, event.event_text) == max_event_count) {
                    break;
                }
            }
        }
        return history_msg;
    }
}

namespace tavernmx::server
{
    void server_worker(const ServerConfiguration& config, std::shared_ptr<ClientConnectionManager> connections) {
        try {
            RoomManager<ServerRoom> rooms{};
            RoomHistory room_history{};

            TMX_INFO("Server worker starting.");

            TMX_INFO("Creating initial rooms ...");
            for (const std::string& room_name : config.initial_rooms) {
                if (const std::shared_ptr<ServerRoom> room = rooms.create_room(room_name)) {
                    TMX_INFO("Room created: #{}", room->room_name());
                } else {
                    TMX_WARN("Room already exists or invalid name: #{}", room_name);
                }
            }
            TMX_INFO("All rooms created.");

            // Server work thread is ready, wait for main thread to start accepting connections.
            server_ready_signal.release();
            server_accept_signal.acquire();

            TMX_INFO("Server work loop starting ...");
            while (connections->is_accepting_connections()) {
                std::chrono::time_point<std::chrono::high_resolution_clock> loop_start =
                    std::chrono::high_resolution_clock::now();

                // Step 1. Gather all messages from clients and distribute room events
                std::vector<std::string> new_rooms{};
                std::vector<std::string> destroyed_rooms{};
                const std::vector<std::shared_ptr<ClientConnection>> clients = connections->get_active_connections();

                for (const std::shared_ptr<ClientConnection>& client : clients) {
                    while (const std::optional<Message> msg = client->messages_in.pop()) {
                        switch (msg->message_type) {
                        case MessageType::ROOM_LIST:
                            // Client requested the room list, send it back
                            client->messages_out.push(
                                create_room_list(std::cbegin(rooms.room_names()), std::cend(rooms.room_names()))
                                );
                            break;
                        case MessageType::ROOM_CREATE: {
                            // Client wants to create a new room.
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (!room_name.empty()) {
                                if (const std::shared_ptr<ServerRoom> room = rooms.create_room(room_name)) {
                                    TMX_INFO("Room created (client request): #{}", room->room_name());
                                    room->joined_clients.emplace_back(client);
                                    new_rooms.push_back(std::move(room_name));
                                } else {
                                    TMX_WARN("Room already exists or invalid name (client create request): #{}",
                                        room_name);
                                }
                            }
                        }
                        break;
                        case MessageType::ROOM_JOIN: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (const std::shared_ptr<ServerRoom> room = rooms[room_name]) {
                                room->join(client);
                            } else {
                                TMX_WARN("Room does not exist (client join request): #{}", room_name);
                            }
                        }
                        break;
                        case MessageType::ROOM_DESTROY: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (const std::shared_ptr<ServerRoom> room = rooms[room_name]) {
                                room->request_destroy();
                                destroyed_rooms.push_back(std::move(room_name));
                            } else {
                                TMX_WARN("Room does not exist (client destroy request): #{}", room_name);
                            }
                        }
                        break;
                        case MessageType::ROOM_HISTORY: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            auto event_count = message_value_or<std::int32_t>(*msg, "event_count"s);
                            const std::shared_ptr<ServerRoom> room = rooms[room_name];
                            if (event_count >= 0 && event_count <= ROOM_HISTORY_MAX_ENTRIES && room) {
                                client->messages_out.push(
                                    get_room_history(room_history, room->room_name(), event_count));
                            } else {
                                TMX_WARN("Invalid room history request: name '{}', count {}", room_name, event_count);
                            }
                        }
                        break;
                        case MessageType::CHAT_SEND: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (const std::shared_ptr<ServerRoom> room = rooms[room_name]) {
                                RoomEvent room_event{
                                    .origin_user_name = client->connected_user_name,
                                    .event_text = message_value_or<std::string>(*msg, "text"s)
                                };
                                insert_event_into_room_history(room_history, room->room_name(), room_event);
                                room->events.push(std::move(room_event));
                            } else {
                                TMX_WARN("Client sent message to unknown room: {}", room_name);
                            }
                        }
                        break;
                        default:
                            TMX_WARN("Client sent unhandled message type: {}", static_cast<int32_t>(msg->message_type));
                            break;
                        }
                    }
                }

                // Step 2. Gather events from rooms and distribute to clients
                // Step 2a. For new & destroyed rooms, notify everyone of its creation/destruction
                for (const std::string& room_name : new_rooms) {
                    Message msg = create_room_create(room_name);
                    for (const std::shared_ptr<ClientConnection>& client : clients) {
                        client->messages_out.push(msg);
                    }
                }
                for (const std::string& room_name : destroyed_rooms) {
                    Message msg = create_room_destroy(room_name);
                    for (const std::shared_ptr<ClientConnection>& client : clients) {
                        client->messages_out.push(msg);
                    }
                }

                // Step 2b. For existing rooms, only distribute events to joined clients
                for (const std::shared_ptr<ServerRoom>& room : rooms.rooms()) {
                    room->clean_expired_clients();
                    std::vector<Message> messages = room_events_to_messages(room.get());
                    for (const std::weak_ptr<ClientConnection>& client_ptr : room->joined_clients) {
                        if (const std::shared_ptr<ClientConnection> client = client_ptr.lock()) {
                            for (const Message& message : messages) {
                                client->messages_out.push(message);
                            }
                        }
                    }
                }

                // Step 3. Clean up
                for (const std::string& room_name : destroyed_rooms) {
                    room_history.erase(room_history.find(room_name));
                }
                rooms.remove_destroyed_rooms();

                // Step 4. Sleep
                const std::chrono::high_resolution_clock::duration loop_elapsed =
                    std::chrono::high_resolution_clock::now() - loop_start;
                if (loop_elapsed < TARGET_SERVER_LOOP_MS) {
                    std::this_thread::sleep_for(TARGET_SERVER_LOOP_MS - loop_elapsed);
                } else {
                    TMX_WARN("Server worker loop took too long to process: {}ms",
                        duration_cast<std::chrono::milliseconds>(loop_elapsed).count());
                }
            }

            TMX_INFO("Server worker exiting.");
            server_shutdown_signal.release();
        } catch (std::exception& ex) {
            TMX_ERR("Server worker exited with exception: {}", ex.what());
            server_shutdown_signal.release();
        }
    }
}
