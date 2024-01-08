#include "tavernmx/server-workers.h"

using namespace tavernmx::messaging;

namespace
{
    /// Target maximum ms for loop processing. This is based off of SSL_RETRY_MILLISECONDS since there's
    /// a built-in wait in tavernmx::ssl::receive_message(BIO*).
    constexpr std::chrono::milliseconds TARGET_CLIENT_LOOP_MS{ tavernmx::ssl::SSL_RETRY_MILLISECONDS * 2 };
}

namespace tavernmx::server
{
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
                auto loop_start = std::chrono::high_resolution_clock::now();

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
                auto loop_elapsed = std::chrono::high_resolution_clock::now() - loop_start;
                if (loop_elapsed < TARGET_CLIENT_LOOP_MS) {
                    std::this_thread::sleep_for(TARGET_CLIENT_LOOP_MS - loop_elapsed);
                } else {
                    TMX_WARN("Client worker loop took too long to process: {}ms",
                        duration_cast<std::chrono::milliseconds>(loop_elapsed).count());
                }
            }
            TMX_INFO("Client worker exiting.");
        } catch (const std::exception& ex) {
            TMX_ERR("Client worker exited with exception: {}", ex.what());
        }
    }

}
