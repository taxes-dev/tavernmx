#pragma once
#include <functional>
#include <unordered_map>

namespace tavernmx::client {
    enum class ClientUiState {
        Connect,
        Connecting
    };

    enum class ClientUiMessage : int32_t {
        Connect_ConnectButton,
        Connecting_CancelButton
    };

    using ClientUiHandler = std::function<void(class ClientUi *)>;

    class ClientUi {
    public:
        ClientUi() noexcept = default;

        void add_handler(ClientUiMessage message, ClientUiHandler && handler);
        ClientUiState get_state() const noexcept { return this->state; }
        void remove_handler(ClientUiMessage message);
        void render();
        ClientUiState set_state(const ClientUiState & new_state) noexcept;
        void set_viewport_resized() noexcept { this->viewport_resized = true; };

    private:
        ClientUiState state{ClientUiState::Connect};
        bool viewport_resized{false};
        std::unordered_map<ClientUiMessage, ClientUiHandler> handlers{};

        void call_handler(ClientUiMessage message);
        void window_center();
        void window_size(float rel_w = 0.0f, float rel_h = 0.0f);
        void render_connect();
        void render_connecting();
    };
}