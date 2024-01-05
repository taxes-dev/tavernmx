#include <chrono>
#include <csignal>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "thread-pool/BS_thread_pool.hpp"
#include "tavernmx/server.h"

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;
using namespace tavernmx::server;

namespace
{
    /// Signals that the server work thread is ready to receive data.
    std::binary_semaphore server_ready_signal{ 0 };
    /// Signals that the main server thread has started accepting TLS connections.
    std::binary_semaphore server_accept_signal{ 0 };
    /// Signals that the server work thread would like the main server thread to shutdown.
    std::binary_semaphore server_shutdown_signal{ 0 };

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

    void client_worker(std::shared_ptr<ClientConnection> client) {
        try {
            // Expect client to send HELLO as the first message
            if (auto hello = client->wait_for(MessageType::HELLO)) {
                client->connected_user_name = std::get<std::string>(hello->values["user_name"s]);
                TMX_INFO("Client connected: {}", client->connected_user_name);
                // TODO: validate user name
                client->send_message(create_ack());
            } else {
                TMX_INFO("No HELLO sent by client, disconnecting.");
                TMX_INFO("Client worker exiting.");
                return;
            }

            // Serialize messages back and forth from client
            while (client->is_connected()) {
                std::vector<Message> send_messages{};

                // 1. Read waiting messages on socket
                if (auto block = client->receive_message()) {
                    TMX_INFO("Receive message block: {} bytes", block->payload_size);
                    for (auto& msg : unpack_messages(block.value())) {
                        TMX_INFO("Receive message: {}", static_cast<int32_t>(msg.message_type));
                        switch (msg.message_type) {
                        case MessageType::HEARTBEAT:
                            // if client requests a HEARTBEAT, we can respond immediately
                            send_messages.push_back(create_ack());
                            break;
                        case MessageType::ACK:
                        case MessageType::NAK:
                            // outside of connection handshake, ACK/NAK can be ignored
                            break;
                        case MessageType::Invalid:
                            // programming error?
                            assert(false && "Received Invalid message type");
                            break;
                        default:
                            // anything else, queue it for processing
                            client->messages_in.push(std::move(msg));
                            break;
                        }
                    };
                }

                // 2. Send queued messages to socket
                while (auto msg = client->messages_out.pop()) {
                    TMX_INFO("Send message: {}", static_cast<int32_t>(msg->message_type));
                    send_messages.push_back(std::move(msg.value()));
                }
                client->send_messages(std::cbegin(send_messages), std::cend(send_messages));

                // 3. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{ 100ll });
            }
            TMX_INFO("Client worker exiting.");
        } catch (const std::exception& ex) {
            TMX_ERR("Client worker exited with exception: {}", ex.what());
        }
    }
}

int main() {
    try {
#ifndef TMX_WINDOWS
        std::signal(SIGPIPE, SIG_IGN);
#endif

        tavernmx::configure_logging(spdlog::level::warn, {});
        TMX_INFO("Loading configuration ...");
        const ServerConfiguration config{ "server-config.json" };
        const spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);

        TMX_INFO("Configuration loaded. Server starting ...");

        auto connections = std::make_shared<ClientConnectionManager>(config.host_port);
        connections->load_certificate(config.host_certificate_path, config.host_private_key_path);
        std::weak_ptr wk_connections = connections;
        static auto sigint_handler = [&wk_connections]() {
            TMX_WARN("Interrupt received.");
            if (auto connections = wk_connections.lock()) {
                connections->shutdown();
            }
        };
        std::signal(SIGINT, [](int32_t) { sigint_handler(); });

        // start server worker & wait for it to be ready
        std::thread server_thread{ server_worker, config, connections };
        server_ready_signal.acquire();

        connections->begin_accept();
        server_accept_signal.release();

        TMX_INFO("Accepting connections ...");
        BS::thread_pool client_thread_pool{ static_cast<BS::concurrency_t>(config.max_clients) };
        while (!server_shutdown_signal.try_acquire() && connections->is_accepting_connections()) {
            if (auto client = connections->await_next_connection()) {
                TMX_INFO("Running: {} / {}", client_thread_pool.get_tasks_running(),
                    client_thread_pool.get_thread_count());
                if (client_thread_pool.get_tasks_running() >= client_thread_pool.get_thread_count()) {
                    TMX_WARN("Too many connections.");
                    client->get()->shutdown();
                    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
                } else {
                    client_thread_pool.detach_task(std::bind(client_worker, client.value()));
                }
            }
        }

        TMX_INFO("Waiting for server worker thread ...");
        server_thread.join();

        TMX_INFO("Server shutdown.");
        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Server shutdown unexpectedly.");
        return 1;
    }
}
