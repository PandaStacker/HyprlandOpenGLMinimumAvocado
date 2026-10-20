#ifndef APP_STATE_H
#define APP_STATE_H

#include <array>
#include "input_mapper.h"

// =============================================================================
// Demo-Specific Actions (Custom Enums)
// =============================================================================
// Notice: Consumer projects define their own actions here starting from Action::User.
// The core library (input_mapper.h) never has to be modified!
enum DemoAction : uint32_t {
    MoveUp = Action::User,
    MoveDown,
    MoveLeft,
    MoveRight,
    ColorReset,
    ColorRed,
    ColorGreen,
    ColorBlue
};

// =============================================================================
// ARCHITECTURE & MULTIPLAYER DESIGN: AppState
// =============================================================================
//
// Why does AppState look like this?
//
// 1. ZERO GRAPHICS / PLATFORM DEPENDENCIES (Headless Server Ready):
//    - AppState contains NO OpenGL identifiers, NO Wayland window pointers, and
//      NO display-server headers.
//    - A dedicated server can run this exact state in a headless Linux CLI.
//
// 2. UNIFIED INPUT CONSUMPTION:
//    - AppState only queries InputMapper (via is_action_down / is_action_just_pressed).
//    - No raw callbacks, no duplicate mapping.
//
// 3. HOW TO REPLACE THIS:
//    - This class is just a minimal demonstration. For your actual game or app,
//      simply replace this file with your own state representation.
// =============================================================================

class AppState {
public:
    virtual ~AppState() = default;

    // Registers demo-specific keybindings into the generic input mapper
    virtual void bind_inputs(InputMapper& input);

    // Simulation / State update logic (Runs on client and headless server alike)
    virtual void update(const InputMapper& input);

    // Baseline state properties (simple, demonstrative, unopinionated)
    float x = 0.0f;
    float y = 0.0f;
    std::array<float, 3> color{ 1.0f, 0.5f, 0.2f }; // Default orange
};

#endif // APP_STATE_H
