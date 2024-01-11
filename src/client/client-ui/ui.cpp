#include <imgui.h>
#include "tavernmx/client-ui.h"

using namespace std::string_literals;

#define RESIZE_COND_FLAGS (this->viewport_resized ? ImGuiCond_Always : ImGuiCond_Appearing)

namespace tavernmx::client
{

    void ClientUi::render() {
        // process push/pop requests to screen stack
        while (std::optional<PushPopRequest> op = this->op_queue.pop()) {
            switch (op->op) {
            case PushPopRequest::Pop:
                this->screen_stack.pop();
                break;
            case PushPopRequest::Push:
                this->screen_stack.push(std::move(op->to_push));
                break;
            }
        }

        // call current screen to render
        if (!this->screen_stack.empty()) {
            const std::unique_ptr<ClientUiScreen>& screen = this->screen_stack.top();
            screen->update(this);
            screen->render(this, this->viewport_resized);
        }

        // check for any errors to display
        if (!this->current_error.empty()) {
            this->render_error();
        }

        // reset flags
        this->viewport_resized = false;
        this->screen_popped = false;
    }

    void ClientUi::render_error() {
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;
        ImGui::SetNextWindowPos(ImVec2{ 20.0f, viewport.y - 20.0f }, RESIZE_COND_FLAGS, ImVec2{ 0.0f, 1.0f });
        ImGui::Begin("Error!", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("%s", this->current_error.c_str());
        if (ImGui::Button("OK")) {
            this->current_error.clear();
        }
        ImGui::End();
    }
}
