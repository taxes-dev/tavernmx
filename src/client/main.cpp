#include <csignal>
#include <filesystem>
#include <future>
#include <iostream>
#include <semaphore>
#include <thread>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL.h>
#include "tavernmx/client.h"
#include "tavernmx/client-ui.h"
#include "tavernmx/client-workers.h"

using namespace tavernmx::client;
using namespace tavernmx::messaging;
using namespace tavernmx::rooms;

/// Signals that the connection to the server should be cleanly shutdown.
extern std::binary_semaphore shutdown_connection_signal;

namespace
{
    /// Initial pixel width of the main window.
    constexpr int32_t WIN_WIDTH = 1280;
    /// Initial pixel height of the main window.
    constexpr int32_t WIN_HEIGHT = 720;
}

int main(int argv, char** argc) {
#ifndef TMX_WINDOWS
    std::signal(SIGPIPE, SIG_IGN);
#endif

    try {
        tavernmx::configure_logging(spdlog::level::warn, {});
        TMX_INFO("Loading configuration ...");
        const ClientConfiguration config{ "client-config.json" };
        const spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        tavernmx::configure_logging(log_level, config.log_file);
        TMX_INFO("Client starting.");

        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            TMX_ERR("SDL Error: {}", SDL_GetError());
            return 1;
        }
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

        // Create window with SDL_Renderer graphics context
        SDL_Window* window = SDL_CreateWindow("tavernmx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (window == nullptr) {
            TMX_ERR("SDL Error creating SDL_Window: {}", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            TMX_ERR("SDL Error creating SDL_Renderer: {}", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        SDL_RendererInfo info{};
        SDL_GetRendererInfo(renderer, &info);
        TMX_INFO("Current SDL_Renderer: {}", info.name);

        // Create ImGUI context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // load custom fonts if specified
        ImFont* custom_font{ nullptr };
        if (!config.custom_font.en.empty()) {
            if (std::filesystem::exists(config.custom_font.en)) {
                ImVector<ImWchar> ranges;
                ImFontGlyphRangesBuilder builder;
                builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
                builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
                builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
                builder.BuildRanges(&ranges);

                ImFontConfig font_config{};
                font_config.MergeMode = true;

                const auto font_size = static_cast<float>(config.custom_font.font_size);
                custom_font = io.Fonts->AddFontFromFileTTF(config.custom_font.en.c_str(),
                    font_size, nullptr, ranges.Data);
                if (!config.custom_font.cn.empty() && std::filesystem::exists(config.custom_font.cn)) {
                    io.Fonts->AddFontFromFileTTF(config.custom_font.cn.c_str(),
                        font_size, &font_config, io.Fonts->GetGlyphRangesChineseFull());
                }
                if (!config.custom_font.jp.empty() && std::filesystem::exists(config.custom_font.jp)) {
                    io.Fonts->AddFontFromFileTTF(config.custom_font.jp.c_str(),
                        font_size, &font_config, io.Fonts->GetGlyphRangesJapanese());
                }
                if (!config.custom_font.kr.empty() && std::filesystem::exists(config.custom_font.kr)) {
                    io.Fonts->AddFontFromFileTTF(config.custom_font.kr.c_str(),
                        font_size, &font_config, io.Fonts->GetGlyphRangesKorean());
                }

                io.Fonts->Build();
            } else {
                TMX_WARN("Font file '{}' does not exist.", config.custom_font.en);
            }
        }

        // setup UI handlers
        bool done = false;
        const auto client_ui = std::make_shared<ClientUi>();

        // create initial UI screen
        auto connect_screen = std::make_unique<ConnectUiScreen>();
        connect_screen_worker(connect_screen.get(), config);
        client_ui->push_screen(std::move(connect_screen));

        // Main loop
        while (!done) {
            // Poll events
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                switch (event.type) {
                case SDL_QUIT:
                    done = true;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        done = true;
                    }
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        client_ui->set_viewport_resized();
                    }
                    break;
                }
            }

            // Start ImGui frame
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Issue ImGui commands
            if (custom_font) {
                ImGui::PushFont(custom_font);
            }
            client_ui->render();
            if (custom_font) {
                ImGui::PopFont();
            }

            // Render and present
            ImGui::Render();
            SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer);
        }

        // Signal server connection worker in case it's still running
        shutdown_connection_signal.release();
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Shutdown
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Client shutdown unexpectedly.");
        SDL_Quit();
        return 1;
    }
}
