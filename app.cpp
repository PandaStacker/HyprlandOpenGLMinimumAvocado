#include "app.h"
#include <iostream>
#include <string>

WindowConfig App::parse_config(int argc, char** argv) {
    bool enable_exit = false;
    for (int i = 1; i < argc; ++i) {
        if (!argv || !argv[i]) continue;
        std::string arg = argv[i];
        if (arg == "--enable-exit" || arg == "-e" || arg == "--exit-key" || arg == "--test") {
            enable_exit = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --enable-exit, -e   Enable '_' exit button (disabled by default)\n"
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
        .title              = "Wayland Minimums - Zero Framework OpenGL",
        .fullscreen         = true,
        .enable_exit_button = enable_exit
    };
}

App::App(int argc, char** argv)
    : App(std::make_unique<AppState>(), argc, argv) {}

App::App(std::unique_ptr<AppState> custom_state, int argc, char** argv)
    : window_(parse_config(argc, argv)),
      graphics_(),
      app_state_(custom_state ? std::move(custom_state) : std::make_unique<AppState>()) {
    init_event_hooks();
}

void App::init_event_hooks() {
    // Optional hook: Press F5 to reload shaders during runtime
    window_.on_key([this](uint32_t, uint32_t keysym, bool pressed) {
        if (pressed && keysym == XKB_KEY_F5) {
            graphics_.reload_shaders();
        }
    });

    // Optional hook: Mouse click triggers color changes
    window_.on_mouse_button([this](uint32_t button, bool pressed, double, double) {
        if (!pressed) return;
        if (button == BTN_LEFT)   app_state_->set_color(0.2f, 0.8f, 0.2f); // Green
        if (button == BTN_RIGHT)  app_state_->set_color(0.2f, 0.4f, 1.0f); // Blue
        if (button == BTN_MIDDLE) app_state_->set_color(1.0f, 1.0f, 0.2f); // Yellow
    });
}

int App::run() {
    while (window_.is_running()) {
        window_.poll_events();

        // 1. Update application state
        app_state_->update(window_.input());

        // 2. Render graphics (read-only observer)
        graphics_.render(*app_state_);

        window_.swap_buffers();
    }
    return 0;
}
