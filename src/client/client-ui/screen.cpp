#include <imgui.h>
#include "tavernmx/client-ui.h"

#define RESIZE_COND_FLAGS (viewport_resized ? ImGuiCond_Always : ImGuiCond_Appearing)

namespace tavernmx::client
{
    void ClientUiScreen::add_handler(ClientUiMessage message, ClientUiHandler&& handler) {
        this->handlers.insert_or_assign(message, std::move(handler));
    }

    void ClientUiScreen::remove_handler(ClientUiMessage message) {
        this->handlers.erase(message);
    }

    void ClientUiScreen::call_handler(ClientUiMessage message, ClientUi* ui) {
        const auto iter = this->handlers.find(message);
        if (iter != this->handlers.end()) {
            iter->second(ui, this);
        }
    }

    void ClientUiScreen::window_center(bool viewport_resized) {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, RESIZE_COND_FLAGS, ImVec2{ 0.5f, 0.5f });
    }

    void ClientUiScreen::window_size(float rel_w, float rel_h, bool viewport_resized) {
        assert(rel_w >= 0.0f && rel_w <= 1.0f);
        assert(rel_h >= 0.0f && rel_w <= 1.0f);
        ImVec2 size = ImGui::GetMainViewport()->Size;
        size.x *= rel_w;
        size.y *= rel_h;
        ImGui::SetNextWindowSize(size, RESIZE_COND_FLAGS);
    }

}
