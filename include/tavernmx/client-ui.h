#pragma once
#ifndef TMX_CLIENT
#define TMX_CLIENT
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "client-rooms.h"
#include "queue.h"
#include "ringbuffer.h"
#include "stack.h"

namespace tavernmx::client
{
    class ClientUi;

    /// Callback signature for UI event handlers.
    using ClientUiHandler = std::function<void(ClientUi*, class ClientUiScreen*)>;

    /// Type for UI messages, used by individual screens.
    using ClientUiMessage = int32_t;

    /**
     * @brief Base class for UI screen behaviours.
     */
    class ClientUiScreen
    {
    public:
        /**
         * @brief The UPDATE handler is called on every frame on every screen.
         * @note This is the only common message. Derived classes should not reuse value 0, but
         * may use any other positive integer.
         */
        static constexpr ClientUiMessage MSG_UPDATE = 0;

        /**
         * @brief Creates a ClientUiScreen.
         */
        ClientUiScreen() noexcept = default;

        /**
         * @brief Destructor.
         */
        virtual ~ClientUiScreen() = default;

        /**
         * @brief Add a new handler for the UI event \p message. If one existed already, it will
         * be replaced.
         * @param message A ClientUiMessage value specific to this screen.
         * @param handler A ClientUiHandler function to call when \p message is raised.
         */
        void add_handler(ClientUiMessage message, ClientUiHandler&& handler);

        /**
         * @brief Removes the handler function for the UI event \p message, if any exists.
         * @param message A ClientUiMessage value specific to this screen.
         */
        void remove_handler(ClientUiMessage message);

        /**
         * @brief Called when the main UI thread wants to render this screen.
         * @param ui Pointer to ClientUi that owns this screen.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous render call.
         * @note Derived classes must implement this method.
         */
        virtual void render(ClientUi* ui, bool viewport_resized) = 0;

        /**
         * @brief Called when the main UI thread wants this screen to update its state.
         * @param ui Pointer to ClientUi that owns this screen.
         * @note By default, this calls the MSG_UPDATE handler if one exists. Derived classes
         * may override this behaviour.
         */
        virtual void update(ClientUi* ui) {
            this->call_handler(MSG_UPDATE, ui);
        };

    protected:
        /**
         * @brief Calls the ClientUiHandler function associated with \p message.
         * @param message A ClientUiMessage value specific to this screen.
         * @param ui Pointer to ClientUi that owns this screen.
         */
        void call_handler(ClientUiMessage message, ClientUi* ui);

        /**
         * @brief Can be called prior to starting a new ImGui window to request that it be
         * centred in the viewport.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous render call.
         */
        void window_center(bool viewport_resized = false);

        /**
         * @brief Can be called prior to starting a new ImGui window to request the desired
         * width and height.
         * @param rel_w A value from 0.0f to 1.0f indicating the ratio of the window's width
         * to the viewport's width. 0.0f indicates to size this dimension automatically.
         * @param rel_h A value from 0.0f to 1.0f indicating the ratio of the window's height
         * to the viewport's height. 0.0f indicates to size this dimension automatically.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous redner call.
         */
        void window_size(float rel_w = 0.0f, float rel_h = 0.0f, bool viewport_resized = false);

    private:
        std::unordered_map<ClientUiMessage, ClientUiHandler> handlers{};
    };

    /**
     * @brief Manages the updating and rendering of the client user interface. Screens to render
     * should be pushed onto the screen stack by calling push_screen(). They can subsequently be
     * removed by calling pop_screen(). Only the top screen on the stack will be updated and rendered
     * when render() is called. (i.e., lower screens will be suspended.)
     */
    class ClientUi
    {
    public:
        /**
         * @brief Creates a new ClientUi.
         */
        ClientUi() noexcept = default;

        /**
         * @brief Call this when you want to update the current screen and issue
         * ImGui commands to build the UI.
         */
        void render();

        /**
         * @brief Sets an error message to be displayed in a dialog box.
         * @param message (copied) std::string
         */
        void set_error(std::string_view message) { this->current_error = std::string{message}; };

        /**
         * @brief Call this when the viewport dimensions have changed.
         */
        void set_viewport_resized() { this->viewport_resized = true; };

        /**
         * @brief Pops the top screen off the stack.
         * @note This operation is deferred until the next call to render().
         */
        void pop_screen() {
            this->op_queue.push(PushPopRequest{ .op = PushPopRequest::Pop });
        }

        /**
         * @brief Pushes a new screen on to the stack.
         * @param screen A unique pointer to a ClientUiScreen. This class
         * takes ownership of it.
         * @note This operation is deferred until the next call to render().
         */
        void push_screen(std::unique_ptr<ClientUiScreen> screen) {
            this->op_queue.push(PushPopRequest{ .op = PushPopRequest::Push,
                                                .to_push = std::move(screen) });
        };

    private:
        /**
         * @brief Tracks requests to push or pop a screen on the stack.
         */
        struct PushPopRequest
        {
            /// Push or pop operation.
            enum { Push, Pop } op{};

            /// If this is a push, there should be a screen to push on the stack.
            std::unique_ptr<ClientUiScreen> to_push{};
        };

        bool viewport_resized{ false };
        bool screen_popped{ false };
        std::string current_error{};
        ThreadSafeStack<std::unique_ptr<ClientUiScreen>> screen_stack{};
        ThreadSafeQueue<PushPopRequest> op_queue{};

        void render_error();
    };

    /**
     * @brief UI screen which displays a few input boxes for information
     * needed to connect to the server.
     */
    class ConnectUiScreen : public ClientUiScreen
    {
    public:
        /// Message issued when the "Connect" button is pressed.
        static constexpr ClientUiMessage MSG_CONNECT_BUTTON = 1;

        /**
         * @brief The user name entered into the box.
         * @note Do not update this value while the screen is active.
         */
        std::string user_name{};

        /**
         * @brief The host name entered into the box.
         * @note Do not update this value while the screen is active.
         */
        std::string host_name{};

        /**
         * @brief The host port entered into the box.
         * @note Do not update this value while the screen is active.
         */
        std::string host_port{};

        /**
         * @brief Creates a ConnectUiScreen.
         */
        ConnectUiScreen() noexcept = default;

        /**
         * @brief Called when the main UI thread wants to render this screen.
         * @param ui Pointer to ClientUi that owns this screen.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous render call.
         */
        void render(ClientUi* ui, bool viewport_resized) override;

    private:
        bool user_name_invalid{ false };
        bool host_name_invalid{ false };
        bool host_port_invalid{ false };

        void try_submit(ClientUi* ui);
    };

    /**
     * @brief Dialog box shown while the client is attempting to connect to the server.
     * The user can push a button to cancel the attempt.
     */
    class ConnectingUiScreen : public ClientUiScreen
    {
    public:
        /// Message issued when the "Cancel" button is pressed.
        static constexpr ClientUiMessage MSG_CANCEL_BUTTON = 1;

        /**
         * @brief Creates a ConnectingUiScreen.
         */
        ConnectingUiScreen() noexcept = default;

        /**
         * @brief If the user pressed the "Cancel" button, this will return true.
         * @return true if the dialog was cancelled, otherwise false.
         */
        bool is_cancelled() const { return this->cancelled; }

        /**
         * @brief Called when the main UI thread wants to render this screen.
         * @param ui Pointer to ClientUi that owns this screen.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous render call.
         */
        void render(ClientUi* ui, bool viewport_resized) override;

    private:
        bool cancelled{ false };
    };

    /**
     * @brief Main chat window shown while connected to the server.
     */
    class ChatWindowScreen : public ClientUiScreen
    {
    public:
        /// Maximum amount of chat room history to track.
        static constexpr size_t CHAT_ROOM_HISTORY_SIZE = 1000;
        /// Message issued whenever the user changes the current room.
        static constexpr ClientUiMessage MSG_ROOM_CHANGED = 1;
        /// Message issued whenever the user inputs a new chat line.
        static constexpr ClientUiMessage MSG_CHAT_SUBMIT = 2;
        /// Message issued whenever the chat window is closed.
        static constexpr ClientUiMessage MSG_CHAT_CLOSED = 3;

        /// Currently selected room (index).
        int32_t current_room_index{ 0 };

        /// Currently selected room (name).
        std::string current_room_name{};

        /// Text currently in the chat input box.
        std::string chat_input{};

        /// If true, show a message that we're waiting on the server and disable chat controls.
        bool waiting_on_server{ false };

        /**
         * @brief Creates a ChatWindowScreen.
         * @param host_name The currently connected host name, for display purposes only.
         * @param user_name The currently connected user name, for display purposes only.
         */
        ChatWindowScreen(std::string_view host_name, std::string_view user_name) {
            this->window_label = std::string{user_name} + "@" + std::string{host_name};
            this->room_names = new char*[0];
        };

        ~ChatWindowScreen() override {
            delete[] room_names;
        }

        /**
         * @brief Called when the main UI thread wants to render this screen.
         * @param ui Pointer to ClientUi that owns this screen.
         * @param viewport_resized A value indicating whether the viewport was resized since the
         * previous render call.
         */
        void render(ClientUi* ui, bool viewport_resized) override;

        /**
         * @brief Select chat room with name \p room_name, if it exists.
         * @param room_name Unique name of the room to select.
         * @return Index of the room name actually selected, which may be different from the
         * requested one if it was unable to be selected.
         * @note This can also result in an update to current_room_name / current_room_index. Also
         * it will not trigger MSG_ROOM_CHANGED.
         */
        size_t select_room_by_name(std::string_view room_name);

        /**
         * @brief Update the room list in the display.
         */
        void update_rooms(const std::vector<std::string>& room_name_list);

        /**
         * @brief Insert an \p event into the history for \p room_name.
         * @param room_name Unique name of the room that generated \p event.
         * @param event (copied) tavernmx::rooms::ClientRoomEvent
         * @note Thread-safe.
         */
        void insert_chat_history_event(std::string_view room_name, rooms::ClientRoomEvent event);

        /**
         * @brief Replace the chat history for \p room_name with a new set of events.
         * @tparam Iterator Forward iterator of tavernmx::rooms::ClientRoomEvent values.
         * @param room_name Unique name of the room whose history will be replaced.
         * @param begin Start of range pointing to ClientRoomEvent values.
         * @param end End of range pointing to ClientRoomEvent values.
         */
        template <typename Iterator>
            requires std::forward_iterator<Iterator> && std::same_as<std::iter_value_t<Iterator>, rooms::ClientRoomEvent>
        void rewrite_chat_history(const std::string& room_name, Iterator begin, Iterator end) {
            std::lock_guard lock_guard{ this->chat_history_mutex };
            if (this->chat_room_history.contains(room_name)) {
                this->chat_room_history[room_name] = {};
            } else {
                this->chat_room_history[room_name].reset();
            }
            for (auto it = begin; it != end; ++it) {
                this->chat_room_history[room_name].insert(std::move(*it));
            }
            this->reset_scroll_pos = room_name == this->current_room_name;
        }

    private:
        struct StringHash : std::hash<std::string_view>
        {
         using is_transparent = void;
        };
        std::string window_label{};
        char** room_names{ nullptr };
        size_t room_names_size{ 0 };
        std::unordered_map<std::string, RingBuffer<rooms::ClientRoomEvent, CHAT_ROOM_HISTORY_SIZE>, StringHash, std::equal_to<>> chat_room_history{};
        std::mutex chat_history_mutex{};
        bool reset_scroll_pos{ false };
        bool reset_text_focus{ true };
        bool window_open{ true };

        void render_chat_history();
    };
}
