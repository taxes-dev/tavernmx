#pragma once
#include <functional>
#include <unordered_map>

namespace tavernmx::client
{
    enum class ClientUiState
    {
        Connect,
        Connecting,
        ChatWindow
    };

    enum class ClientUiMessage : int32_t
    {
        Connect_ConnectButton,
        Connecting_CancelButton
    };

    using ClientUiHandler = std::function<void(class ClientUi*)>;
    using ClientStateBag = std::unordered_map<const char*, std::string>;

    class ClientUi
    {
    public:
        ClientUi() noexcept = default;

        void add_handler(ClientUiMessage message, ClientUiHandler&& handler);

        ClientUiState get_state() const noexcept { return this->state; }

        void remove_handler(ClientUiMessage message);

        void render();

        void set_error(std::string message) { this->current_error = std::move(message); };

        ClientUiState set_state(const ClientUiState& new_state) noexcept;

        void set_viewport_resized() noexcept { this->viewport_resized = true; };

        ClientStateBag& operator[](const ClientUiState& state) {
            if (!this->state_bag.contains(state)) {
                state_bag.insert({ state, ClientStateBag{} });
            }
            return this->state_bag[state];
        }

        const ClientStateBag& operator[](const ClientUiState& state) const {
            if (!this->state_bag.contains(state)) {
                state_bag.insert({ state, ClientStateBag{} });
            }
            return this->state_bag[state];
        }

    private:
        ClientUiState state{ ClientUiState::Connect };
        bool viewport_resized{ false };
        std::unordered_map<ClientUiMessage, ClientUiHandler> handlers{};
        mutable std::unordered_map<ClientUiState, ClientStateBag> state_bag{};
        std::string current_error{};

        void call_handler(ClientUiMessage message);

        void window_center();

        void window_size(float rel_w = 0.0f, float rel_h = 0.0f);

        void render_connect();

        void render_connecting();

        void render_chat_window();

        void render_error();
    };
}
