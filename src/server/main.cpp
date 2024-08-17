#include <chrono>
#include <csignal>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "thread-pool/BS_thread_pool.hpp"
#include "tavernmx/server.h"
#include "tavernmx/server-workers.h"

using namespace tavernmx::messaging;
using namespace tavernmx::rooms;
using namespace tavernmx::server;

extern std::binary_semaphore server_ready_signal;
extern std::binary_semaphore server_accept_signal;
extern std::binary_semaphore server_shutdown_signal;

int main() {
	try {
#ifndef TMX_WINDOWS
		std::signal(SIGPIPE, SIG_IGN);
#endif

		tavernmx::configure_logging(spdlog::level::warn, {});
		TMX_INFO("Loading configuration ...");
		const ServerConfiguration config{ "server-config.json" };
		const spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
		tavernmx::configure_logging(log_level, config.log_file);

		TMX_INFO("Configuration loaded. Server starting ...");

		const auto connections = std::make_shared<ClientConnectionManager>(config.host_port);
		connections->load_certificate(config.host_certificate_path, config.host_private_key_path);
		std::weak_ptr wk_connections = connections;
		static auto sigint_handler = [&wk_connections]() {
			TMX_WARN("Interrupt received.");
			if (const std::shared_ptr<ClientConnectionManager> connections = wk_connections.lock()) {
				connections->shutdown();
			}
		};
		std::signal(SIGINT, [](int32_t) { sigint_handler(); });

		// start server worker & wait for it to be ready
		std::thread server_thread{ server_worker, config, connections };
		server_ready_signal.acquire();

		// begin accepting connections and inform the server worker
		connections->begin_accept();
		server_accept_signal.release();

		TMX_INFO("Accepting connections ...");
		BS::thread_pool client_thread_pool{ static_cast<BS::concurrency_t>(config.max_clients) };
		while (!server_shutdown_signal.try_acquire() && connections->is_accepting_connections()) {
			if (std::optional<std::shared_ptr<ClientConnection>> client = connections->await_next_connection()) {
				TMX_INFO("Running threads: {} / {}", client_thread_pool.get_tasks_running(),
					client_thread_pool.get_thread_count());
				if (client_thread_pool.get_tasks_running() >= client_thread_pool.get_thread_count()) {
					TMX_WARN("Too many connections.");
					(*client)->send_message(create_nak("Too many connections."));
					(*client)->shutdown();

					// slight delay before trying to accept another client
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
