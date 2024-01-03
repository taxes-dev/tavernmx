#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <imgui_stdlib.h>
#include "tavernmx/client-ui.h"

namespace tavernmx::client
{
    void ConnectUiScreen::render(ClientUi* ui, bool viewport_resized) {
        this->window_size(0.33f, 0.0f, viewport_resized);
        this->window_center(viewport_resized);
        ImGui::Begin("Connect to server ...", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // user name
        ImGui::Text("User name:");
        ImGui::SameLine();
        if (this->user_name_invalid) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 0, 0, 50));
        }
        if (ImGui::InputText("##user", &this->user_name, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->try_submit(ui);
        }
        if (this->user_name_invalid) {
            ImGui::PopStyleColor();
        }

        // host name
        ImGui::Text("Host:");
        ImGui::SameLine();
        if (this->host_name_invalid) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 0, 0, 50));
        }
        if (ImGui::InputText("##host", &this->host_name, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->try_submit(ui);
        }
        if (this->host_name_invalid) {
            ImGui::PopStyleColor();
        }

        // host port
        ImGui::Text("Port:");
        ImGui::SameLine();
        if (this->host_port_invalid) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 0, 0, 50));
        }
        if (ImGui::InputText("##port", &this->host_port,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
            this->try_submit(ui);
        }
        if (this->host_port_invalid) {
            ImGui::PopStyleColor();
        }

        // button
        if (ImGui::Button("Connect")) {
            this->try_submit(ui);
        }
        ImGui::End();

    }

    void ConnectUiScreen::try_submit(ClientUi* ui) {
        this->user_name_invalid = user_name.empty() || !std::all_of(std::cbegin(this->user_name),
                                      std::cend(this->user_name), [](char c) {
                                          return std::isalnum(c);
                                      });
        this->host_name_invalid = host_name.empty() || !std::all_of(std::cbegin(this->host_name),
                                      std::cend(this->host_name), [](char c) {
                                          return std::isalnum(c) || c == '.';
                                      });;
        this->host_port_invalid = this->host_port.empty() || !std::all_of(std::cbegin(this->host_port),
                                      std::cend(this->host_port), [](char c) {
                                          return std::isdigit(c);
                                      });
        if (this->user_name_invalid || this->host_name_invalid || this->host_port_invalid) {
            return;
        }
        this->call_handler(MSG_CONNECT_BUTTON, ui);
    }

}
