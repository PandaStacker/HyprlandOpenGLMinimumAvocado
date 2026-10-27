#include "app.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// app.cpp primarily handles the initialization you would expect at the most initial
// point of the app. client holds the actual running state.

WindowConfig App::parse_config(int argc, char** argv) {
    bool enable_exit = false;
    bool fullscreen = true;
    for (int i = 1; i < argc; ++i) {
        if (!argv || !argv[i]) continue;
        std::string arg = argv[i];
        if (arg == "--enable-exit" || arg == "-e" || arg == "--exit-key" || arg == "--test") {
            enable_exit = true;
        } else if (arg == "--windowed" || arg == "-w") {
            fullscreen = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --enable-exit, -e   Enable '_' exit button (disabled by default)\n"
                      << "  --windowed, -w      Launch in windowed mode instead of fullscreen\n"
                      << "  --help, -h          Show this help message\n";
        }
    }

    if (enable_exit) {
        std::cout << "[Config] Exit button ('_') is ENABLED." << std::endl;
    } else {
        std::cout << "[Config] Exit button ('_') is DISABLED by default (run with --enable-exit to test)." << std::endl;
    }

    return WindowConfig{
        .width              = 1920,
        .height             = 1200,
        .title              = "Wayland Minimums - Zero Framework Vulkan",
        .fullscreen         = fullscreen,
        .enable_exit_button = enable_exit,
        .vsync              = true
    };
}

App::App(int argc, char** argv)
    : App(std::make_unique<Client>(), argc, argv) {}

App::App(std::unique_ptr<Client> custom_state, int argc, char** argv)
    : window_(parse_config(argc, argv)),
      graphics_(window_),
      client_(custom_state ? std::move(custom_state) : std::make_unique<Client>()) {

    client_->bind_inputs(window_.input());
    init_event_hooks();
}

void App::init_event_hooks() {
    window_.on_resize([this](int width, int height) {
        graphics_.resize(width, height);
    });

    // Optional hooks: F5 to reload shaders, F11 to toggle fullscreen
    window_.on_key([this](uint32_t, uint32_t keysym, bool pressed) {
        if (pressed && keysym == XKB_KEY_F5) {
            graphics_.reload_shaders();
        } else if (pressed && keysym == XKB_KEY_F11) {
            window_.set_fullscreen(!window_.is_fullscreen());
        }
    });
}

int App::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (window_.is_running()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime - lastTime;
        float deltaTime = elapsedTime.count();
        lastTime = currentTime;

        window_.poll_events();

        // Check for quit action via InputMapper (unified Escape check)
        if (window_.input().is_action_down(Action::Quit)) {
            window_.close();
            break;
        }

        // 1. Update application state (all gameplay input queried through InputMapper)
        client_->update(window_.input(), deltaTime);

        // 2. Render graphics (read-only observer)
        graphics_.render(*client_);

        window_.swap_buffers();
    }
    return 0;
}
