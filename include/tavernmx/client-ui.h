#pragma once

namespace tavernmx::client {
    enum class ClientUiState {
        Connect,
        Connecting
    };

    class ClientUi {
    public:
        ClientUi() noexcept = default;

        ClientUiState get_state() const noexcept { return this->state; }
        void render();
        ClientUiState set_state(const ClientUiState & new_state) noexcept;
        void set_viewport_resized() noexcept { this->viewport_resized = true; };

    private:
        ClientUiState state{ClientUiState::Connect};
        bool viewport_resized{false};

        void window_center();
        void window_size(float rel_w = 0.0f, float rel_h = 0.0f);
        void render_connect();
        void render_connecting();
    };
}