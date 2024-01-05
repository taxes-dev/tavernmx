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
    std::vector<Message> room_events_to_messages(ServerRoom* room) {
        std::vector<Message> messages{};
        while (auto event = room->events.pop()) {
            switch (event->event_type) {
            case RoomEvent::Created:
                // TODO
                    break;
            case RoomEvent::Destroyed:
                // TODO
                    break;
            case RoomEvent::ChatMessage:
                // TODO: timestamp storage
                    messages.push_back(create_chat_echo(room->room_name(), event->event_text, event->origin_user_name,
                        static_cast<int32_t>(event->timestamp.time_since_epoch().count())));
                break;
            }
        }
        return messages;
    }
}

namespace tavernmx::server
{
    void server_worker(const ServerConfiguration& config, std::shared_ptr<ClientConnectionManager> connections) {
        try {
            RoomManager<ServerRoom> rooms{};
            TMX_INFO("Server worker starting.");

            TMX_INFO("Creating initial rooms ...");
            for (auto& room_name : config.initial_rooms) {
                if (const auto room = rooms.create_room(room_name)) {
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
                // Step 1. Gather all messages from clients and distribute room events
                std::vector<std::string> new_rooms{};
                auto clients = connections->get_active_connections();
                for (auto& client : clients) {
                    while (auto msg = client->messages_in.pop()) {
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
                                if (const auto room = rooms.create_room(room_name)) {
                                    TMX_INFO("Room created (client request): #{}", room->room_name());
                                    room->events.push({
                                        .event_type = RoomEvent::Created,
                                        .origin_user_name = client->connected_user_name
                                    });
                                    room->joined_clients.emplace_back(client);
                                    new_rooms.push_back(std::move(room_name));
                                } else {
                                    TMX_WARN("Room already exists or invalid name (client request): #{}", room_name);
                                }
                            }
                        }
                        break;
                        case MessageType::ROOM_JOIN: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (auto room = rooms[room_name]) {
                                room->join(client);
                            }
                        }
                            break;
                        case MessageType::CHAT_SEND: {
                            auto room_name = message_value_or<std::string>(*msg, "room_name"s);
                            if (auto room = rooms[room_name]) {
                                room->events.push({
                                    .event_type = RoomEvent::ChatMessage,
                                    .origin_user_name = client->connected_user_name,
                                    .event_text = message_value_or<std::string>(*msg, "text"s)
                                });
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
                // Step 2a. For new rooms, notify everyone of its creation
                for (const auto& room_name : new_rooms) {
                    auto msg = create_room_create(room_name);
                    for (const auto& client : clients) {
                        client->messages_out.push(msg);
                    }
                }
                // Step 2b. For existing rooms, only distribute events to joined clients
                for (auto& room : rooms.rooms()) {
                    room->clean_expired_clients();
                    auto messages = room_events_to_messages(room.get());
                    for (auto& client_ptr : room->joined_clients) {
                        if (auto client = client_ptr.lock()) {
                            for (auto& message : messages) {
                                client->messages_out.push(message);
                            }
                        }
                    }
                }
                // Step 3. Clean up
                // Step 4. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{ 50ll });
            }

            TMX_INFO("Server worker exiting.");
            server_shutdown_signal.release();
        } catch (std::exception& ex) {
            TMX_ERR("Server worker exited with exception: {}", ex.what());
            server_shutdown_signal.release();
        }
    }
}