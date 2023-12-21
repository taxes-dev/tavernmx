#include <imgui.h>
#include <imgui_stdlib.h>
#include "tavernmx/client-ui.h"
#include "tavernmx/logging.h"

using namespace std::string_literals;

#define RESIZE_COND_FLAGS (this->viewport_resized ? ImGuiCond_Always : ImGuiCond_Appearing)

namespace tavernmx::client {
    void ClientUi::add_handler(ClientUiMessage message, ClientUiHandler&& handler) {
        this->handlers.insert_or_assign(message, std::move(handler));
    }

    void ClientUi::remove_handler(ClientUiMessage message) {
        this->handlers.erase(message);
    }

    void ClientUi::call_handler(ClientUiMessage message) {
        auto iter = this->handlers.find(message);
        if (iter != this->handlers.end()) {
            iter->second(this);
        }
    }


    void ClientUi::render() {
        // call state-specific rendering
        switch (this->state) {
            case ClientUiState::Connect:
                this->render_connect();
                break;

            case ClientUiState::Connecting:
                this->render_connecting();
                break;

            case ClientUiState::ChatWindow:
                this->render_chat_window();
                break;

            default:
                assert(false && "Unhandled UI state");
        }

        // check for any errors to display
        if (!this->current_error.empty()) {
            this->render_error();
        }

        // reset viewport flag
        this->viewport_resized = false;
    }

    ClientUiState ClientUi::set_state(const ClientUiState& new_state) noexcept {
        switch (this->state) {
            case ClientUiState::Connect:
                if (new_state == ClientUiState::Connecting) {
                    this->state = ClientUiState::Connecting;
                }
                break;
            case ClientUiState::Connecting:
                if (new_state == ClientUiState::Connect || new_state == ClientUiState::ChatWindow) {
                    this->state = new_state;
                }
                break;
            default:
                assert(false && "Unhandled UI state");
        }
        return this->state;
    }

    void ClientUi::window_center() {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, RESIZE_COND_FLAGS, ImVec2{0.5f, 0.5f});
    }

    void ClientUi::window_size(float rel_w, float rel_h) {
        assert(rel_w >= 0.0f && rel_w <= 1.0f);
        assert(rel_h >= 0.0f && rel_w <= 1.0f);
        ImVec2 size = ImGui::GetMainViewport()->Size;
        size.x *= rel_w;
        size.y *= rel_h;
        ImGui::SetNextWindowSize(size, RESIZE_COND_FLAGS);
    }

    void ClientUi::render_connect() {
        auto& state_bag = (*this)[ClientUiState::Connect];
        if (!state_bag.contains("host")) {
            state_bag["host"] = ""s;
        }
        if (!state_bag.contains("port")) {
            state_bag["port"] = "8080"s;
        }

        this->window_size(0.33f);
        this->window_center();
        ImGui::Begin("Connect to server ...", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("Host:");
        ImGui::SameLine();
        if (ImGui::InputText("##host", &state_bag["host"], ImGuiInputTextFlags_EnterReturnsTrue, nullptr,
                             nullptr)) {
            this->call_handler(ClientUiMessage::Connect_ConnectButton);
        }
        ImGui::Text("Port:");
        ImGui::SameLine();
        if (ImGui::InputText("##port", &state_bag["port"],
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal, nullptr,
                             nullptr)) {
            this->call_handler(ClientUiMessage::Connect_ConnectButton);
        }

        if (ImGui::Button("Connect")) {
            this->call_handler(ClientUiMessage::Connect_ConnectButton);
        }
        ImGui::End();
    }

    void ClientUi::render_connecting() {
        this->window_size(0.2f);
        this->window_center();
        ImGui::Begin("Connecting ...", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Connecting ...");
        ImGui::BeginDisabled(!((*this)[ClientUiState::Connecting]["cancelled"].empty()));
        if (ImGui::Button("Cancel")) {
            this->call_handler(ClientUiMessage::Connecting_CancelButton);
        }
        ImGui::EndDisabled();
        ImGui::End();
    }

    void ClientUi::render_chat_window() {
        auto& state_bag = (*this)[ClientUiState::ChatWindow];

        this->window_size(0.8f, 0.8f);
        this->window_center();
        ImGui::Begin(state_bag["host"].c_str(), nullptr);
        ImGui::End();
    }


    void ClientUi::render_error() {
        this->window_size(0.25f);
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;
        ImGui::SetNextWindowPos(ImVec2{20.0f, viewport.y - 20.0f}, RESIZE_COND_FLAGS, ImVec2{0.0f, 1.0f});
        ImGui::Begin("Error!", nullptr,
                     ImGuiWindowFlags_NoCollapse);
        ImGui::Text("%s", this->current_error.c_str());
        if (ImGui::Button("OK")) {
            this->current_error.clear();
        }
        ImGui::End();
    }
}
