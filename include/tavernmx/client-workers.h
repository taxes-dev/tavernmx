#pragma once
#include "client.h"
#include "client-ui.h"

namespace tavernmx::client
{
    /// If the server hasn't sent anything in this period, send a HEARTBEAT.
    static constexpr std::chrono::seconds QUIET_TIMEOUT{ 30ll };

    /**
     * @brief Configures the initial connect screen. When submitted, this will
     * attempt to establish the connection and then start the chat window if it
     * is successful.
     * @param connect_screen The UI connect screen to attach to.
     * @param config Current client configuration.
     */
    void connect_screen_worker(ConnectUiScreen* connect_screen, const ClientConfiguration& config);

    /**
     * @brief Starts the background thread that will process incoming and outgoing
     * messages from/to the server.
     * @param server Active connection to the server. The worker takes ownership of it.
     */
    void server_message_worker(std::unique_ptr<ServerConnection> server);

    /**
     * @brief Configures the chat window and starts the main chat loop.
     * @param connection Active connection to the server. The worker takes ownership of it.
     * @param screen The UI chat window to attach to.
     */
    void chat_window_worker(std::unique_ptr<ServerConnection> connection, ChatWindowScreen* screen);
}