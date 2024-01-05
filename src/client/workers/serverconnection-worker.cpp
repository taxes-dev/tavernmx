#include "tavernmx/client-workers.h"
#include <semaphore>

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;

/// Signals that the connection to the server has been ended.
std::binary_semaphore connection_ended_signal{0};
/// This flag will be set to let the UI know we're waiting on a response from the server.
bool waiting_on_server{false};

namespace tavernmx::client
{
    void server_message_worker(std::unique_ptr<ServerConnection> server) {
        try {
            auto last_message_received = std::chrono::system_clock::now();
            std::optional<std::chrono::time_point<std::chrono::system_clock>> heartbeat_sent{};

            // send initial request for rooms list
            server->send_message(create_room_list());

            while (server->is_connected()) {
                std::vector<Message> send_messages{};

                // 1. Read waiting messages on socket
                if (auto block = server->receive_message()) {
                    TMX_INFO("Receive message block: {} bytes", block->payload_size);
                    for (auto& msg : unpack_messages(block.value())) {
                        TMX_INFO("Receive message: {}", static_cast<int32_t>(msg.message_type));
                        switch (msg.message_type) {
                        case MessageType::HEARTBEAT:
                            // if server requests a HEARTBEAT, we can respond immediately
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
                            server->messages_in->push(std::move(msg));
                            break;
                        }
                    };
                    last_message_received = std::chrono::system_clock::now();
                    heartbeat_sent.reset();
                    waiting_on_server = false;
                }

                // 2. Have we heard from the server lately? If not, send heartbeat.
                if ((std::chrono::system_clock::now() - last_message_received > QUIET_TIMEOUT)) {
                    if (!heartbeat_sent.has_value()) {
                        send_messages.push_back(create_heartbeat());
                        heartbeat_sent = std::chrono::system_clock::now();
                        waiting_on_server = true;
                    } else if (std::chrono::system_clock::now() - heartbeat_sent.value() > QUIET_TIMEOUT) {
                        TMX_INFO("Server did not respond to heartbeat.");
                        break;
                    }
                }

                // 3. Send queued messages to socket
                while (auto msg = server->messages_out->pop()) {
                    TMX_INFO("Send message: {}", static_cast<int32_t>(msg->message_type));
                    send_messages.push_back(std::move(msg.value()));
                }
                server->send_messages(std::cbegin(send_messages), std::cend(send_messages));

                // 3. Sleep
                std::this_thread::sleep_for(std::chrono::milliseconds{ 50ll });
            }
            TMX_INFO("Connection worker exiting.");
        } catch (std::exception& ex) {
            TMX_ERR("Connection worker exited with exception: {}", ex.what());
        }
        connection_ended_signal.release();
    }

}