#include <imgui.h>
#include <imgui_stdlib.h>
#include "tavernmx/client-ui.h"

using namespace std::string_literals;

namespace tavernmx::client
{
    void ChatWindowScreen::render(ClientUi* ui, bool viewport_resized) {
        this->window_size(0.8f, 0.8f, viewport_resized);
        this->window_center();
        ImGui::Begin(this->host_name.c_str(), nullptr);
        const auto window_size = ImGui::GetWindowSize();
        ImGui::BeginChild("Rooms", ImVec2{ window_size.x * 0.2f, 0.0f }, ImGuiChildFlags_None);
        if (ImGui::ListBox("##Rooms1", &this->current_room_index, this->room_names,
            static_cast<int32_t>(this->room_names_size))) {
            if (this->current_room_index < this->room_names_size) {
                this->current_room_name = std::string{ this->room_names[this->current_room_index] + 1 };
            } else {
                this->current_room_name.clear();
            }
            this->call_handler(MSG_ROOM_CHANGED, ui);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Chat", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Border);
        ImGui::InputTextMultiline("##ChatLog", &this->chat_display, ImVec2{ 0.0f, 0.0f }, ImGuiInputTextFlags_ReadOnly);
        if (this->waiting_on_server) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::Text("Server not responding, please wait...");
            ImGui::PopStyleColor();
        }
        ImGui::BeginDisabled(this->waiting_on_server);
        if (ImGui::InputText("##ChatEntry", &this->chat_input, ImGuiInputTextFlags_EnterReturnsTrue) &&
            !this->chat_input.empty()) {
            this->call_handler(MSG_CHAT_SUBMIT, ui);
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::End();
    }

    size_t ChatWindowScreen::select_room_by_name(const std::string& room_name) {
        for (size_t i = 0; i < this->room_names_size; ++i) {
            if (std::strcmp(room_name.c_str(), this->room_names[i] + 1) == 0) {
                this->current_room_index = static_cast<int32_t>(i);
                this->current_room_name = room_name;
                break;
            }
        }
        return this->current_room_index;
    }

    void ChatWindowScreen::update_rooms(const std::vector<std::string>& room_name_list) {
        delete[] this->room_names;
        this->room_names_size = room_name_list.size();
        this->current_room_index = 0;
        this->room_names = new char*[this->room_names_size];
        for (size_t i = 0; i < this->room_names_size; ++i) {
            this->room_names[i] = new char[room_name_list[i].length() + 1];
            this->room_names[i][0] = '#';
            std::strcpy(this->room_names[i] + 1, room_name_list[i].c_str());
        }
        if (this->room_names_size > 0) {
            this->current_room_name = std::string{this->room_names[0] + 1};
        } else {
            this->current_room_name.clear();
        }
    }

}
