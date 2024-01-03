#include <imgui.h>
#include "tavernmx/client-ui.h"

namespace tavernmx::client
{
    void ChatWindowScreen::render(ClientUi* ui, bool viewport_resized) {
        this->window_size(0.8f, 0.8f, viewport_resized);
        this->window_center();
        ImGui::Begin(this->host_name.c_str(), nullptr);
        ImGui::End();
    }
}
