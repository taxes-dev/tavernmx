#include <imgui.h>
#include "tavernmx/client-ui.h"

namespace tavernmx::client
{
    void ConnectingUiScreen::render(ClientUi* ui, bool viewport_resized) {
        this->window_size(0.2f, 0.0f, viewport_resized);
        this->window_center(viewport_resized);
        ImGui::Begin("Connecting ...", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Connecting ...");
        ImGui::BeginDisabled(this->is_cancelled());
        if (ImGui::Button("Cancel")) {
            this->cancelled = true;
            this->call_handler(MSG_CANCEL_BUTTON, ui);
        }
        ImGui::EndDisabled();
        ImGui::End();
    }
}
