#pragma once
#include "server.h"

namespace tavernmx::server
{
    /**
     * @brief Handles sending and receiving messages to a specific client.
     * @param client (copied) An active client connection.
     */
    void client_worker(std::shared_ptr<ClientConnection> client);

    /**
     * @brief Main server work process that handles distributing messages to all clients.
     * @param config Current server configuration.
     * @param connections (copied) Manager of active client connections.
     */
    void server_worker(const ServerConfiguration& config, std::shared_ptr<ClientConnectionManager> connections);
}