#include <csignal>
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

namespace
{
    /// Initial pixel width of the main window.
    constexpr int32_t WIN_WIDTH = 1280;
    /// Initial pixel height of the main window.
    constexpr int32_t WIN_HEIGHT = 720;
}

int main(int argv, char** argc) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
#ifndef TMX_WINDOWS
    std::signal(SIGPIPE, SIG_IGN);
#endif

    try {
        ClientConfiguration config{ "client-config.json" };

        spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
        std::optional<std::string> log_file{};
        if (!config.log_file.empty()) {
            log_file = config.log_file;
        }
        tavernmx::configure_logging(log_level, log_file);
        TMX_INFO("Client starting.");

        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            TMX_ERR("SDL Error: {}", SDL_GetError());
            return 1;
        }
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

        // Create window with SDL_Renderer graphics context
        window = SDL_CreateWindow("tavernmx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (window == nullptr) {
            TMX_ERR("SDL Error creating SDL_Window: {}", SDL_GetError());
            return 1;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            TMX_ERR("SDL Error creating SDL_Renderer: {}", SDL_GetError());
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

        // setup UI handlers
        bool done = false;
        auto client_ui = std::make_shared<ClientUi>();

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

            client_ui->render();

            // Render and present
            ImGui::Render();
            SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer);
        }
    } catch (std::exception& ex) {
        TMX_ERR("Unhandled exception: {}", ex.what());
        TMX_WARN("Client shutdown unexpectedly.");
        return 1;
    }

    // Shutdown
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
