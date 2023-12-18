#include <imgui.h>
#include "tavernmx/client-ui.h"
#include "tavernmx/logging.h"

namespace tavernmx::client {
    void ClientUi::render() {
        // call state-specific rendering
        switch (this->state) {
            case ClientUiState::Connect:
                this->render_connect();
                break;
            case ClientUiState::Connecting:
                this->render_connecting();
                break;

            default:
                assert((false, "Unhandled UI state"));
                break;
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
                if (new_state == ClientUiState::Connect) {
                    this->state = ClientUiState::Connect;
                }
                break;
            default:
                assert((false, "Unhandled UI state"));
                break;
        }
        return this->state;
    }


    void ClientUi::window_center() {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, (this->viewport_resized ? ImGuiCond_Always : ImGuiCond_Appearing),
                                ImVec2{0.5f, 0.5f});
    }

    void ClientUi::window_size(float rel_w, float rel_h) {
        assert(rel_w >= 0.0f && rel_w <= 1.0f);
        assert(rel_h >= 0.0f && rel_w <= 1.0f);
        ImVec2 size = ImGui::GetMainViewport()->Size;
        size.x *= rel_w;
        size.y *= rel_h;
        ImGui::SetNextWindowSize(size, (this->viewport_resized ? ImGuiCond_Always : ImGuiCond_Appearing));
    }

    void ClientUi::render_connect() {
        this->window_size(0.33f);
        this->window_center();
        ImGui::Begin("Connect to server ...", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("Connect to:");
        ImGui::SameLine();
        ImGui::Text("server.tld:8080");
        if (ImGui::Button("Connect")) {
            // TODO: move to external handler
            TMX_INFO("Connect button pressed.");
            this->set_state(ClientUiState::Connecting);
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
        if (ImGui::Button("Cancel")) {
            // TODO: move to external handler
            TMX_INFO("Cancel connecting button pressed.");
            this->set_state(ClientUiState::Connect);
        }
        ImGui::End();
    }
}
