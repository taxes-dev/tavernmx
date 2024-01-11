#include <cstring>
#include <imgui.h>
#include <imgui_stdlib.h>
#include "tavernmx/client-ui.h"

using namespace tavernmx::rooms;

namespace
{
    bool color_too_dark(uint32_t r, uint32_t g, uint32_t b) {
        // convert RGB to YIQ to get an idea of how dark or light the color is
        const uint32_t yiq = ((r * 299) + (g * 587) + (b * 114)) / 1000;
        return (yiq < 128);
    }

    ImU32 chat_name_to_color(const std::string& chat_name) {
        uint32_t hash = 0;
        for (unsigned char c : chat_name) {
            hash = static_cast<uint32_t>(c) + ((hash << 5) - hash);
        }

        uint32_t r = hash >> 16 & 0xff;
        uint32_t g = hash >> 8 & 0xff;
        uint32_t b = hash & 0xff;
        while (color_too_dark(r, g, b)) {
            r += 20;
            g += 20;
            b += 20;
        }

        return IM_COL32(r, g, b, 255);
    }
}

namespace tavernmx::client
{
    void ChatWindowScreen::render(ClientUi* ui, bool viewport_resized) {
        if (!this->window_open) {
            return;
        }

        this->window_size(0.8f, 0.8f, viewport_resized);
        this->window_center();
        ImGui::Begin(this->window_label.c_str(), &this->window_open);

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::BeginChild("Rooms", ImVec2{ window_size.x * 0.2f, 0.0f }, ImGuiChildFlags_None);
        if (ImGui::ListBox("##Rooms1", &this->current_room_index, this->room_names,
            static_cast<int32_t>(this->room_names_size))) {
            if (this->current_room_index < this->room_names_size) {
                this->current_room_name = std::string{ this->room_names[this->current_room_index] + 1 };
            } else {
                this->current_room_name.clear();
            }
            this->reset_scroll_pos = true;
            this->reset_text_focus = true;
            this->call_handler(MSG_ROOM_CHANGED, ui);
        }
        // Rooms
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Chat", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Border);

        ImGui::BeginChild("ChatHistory",
            ImVec2{ 0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() * 2 - 4.0f },
            ImGuiChildFlags_None);
        this->render_chat_history();
        if (this->reset_scroll_pos) {
            ImGui::SetScrollHereY();
            this->reset_scroll_pos = false;
        }
        // ChatHistory
        ImGui::EndChild();

        ImGui::BeginChild("ChatEntryAndErrors", ImVec2{ 0.0f, -4.0f }, ImGuiChildFlags_None);
        if (this->waiting_on_server) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::Text("Server not responding, please wait...");
            ImGui::PopStyleColor();
        }
        ImGui::BeginDisabled(this->waiting_on_server);
        if (this->reset_text_focus) {
            ImGui::SetKeyboardFocusHere();
            this->reset_text_focus = false;
        }
        if (ImGui::InputText("##ChatEntry", &this->chat_input, ImGuiInputTextFlags_EnterReturnsTrue) &&
            !this->chat_input.empty()) {
            this->call_handler(MSG_CHAT_SUBMIT, ui);
            this->reset_text_focus = true;
        }
        ImGui::EndDisabled();
        // ChatEntryAndErrors
        ImGui::EndChild();

        // Chat
        ImGui::EndChild();

        // Window
        ImGui::End();

        if (!this->window_open) {
            this->call_handler(MSG_CHAT_CLOSED, ui);
        }
    }

    void ChatWindowScreen::render_chat_history() {
        std::lock_guard lock_guard{ this->chat_history_mutex };
        auto history = this->chat_room_history.find(this->current_room_name);
        if (history != this->chat_room_history.end()) {
            for (RoomEvent& event : history->second) {
                switch (event.event_type) {
                case RoomEvent::ChatMessage:
                    ImGui::PushStyleColor(ImGuiCol_Text, chat_name_to_color(event.origin_user_name));
                    ImGui::Text("%s", event.origin_user_name.c_str());
                    ImGui::SameLine();
                    ImGui::Text("at %s", event.timestamp_text.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Text("%s", event.event_text.c_str());
                    ImGui::Spacing();
                    break;
                default:
                    break;
                }
            }
        }
    }

    void ChatWindowScreen::insert_chat_history_event(const std::string& room_name, rooms::RoomEvent event) {
        std::lock_guard lock_guard{ this->chat_history_mutex };
        if (this->chat_room_history.find(room_name) == this->chat_room_history.end()) {
            this->chat_room_history[room_name] = {};
        }
        this->chat_room_history[room_name].insert(std::move(event));
        this->reset_scroll_pos = room_name == this->current_room_name;
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
            this->current_room_name = std::string{ this->room_names[0] + 1 };
        } else {
            this->current_room_name.clear();
        }
    }

}
