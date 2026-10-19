#ifndef APP_STATE_H
#define APP_STATE_H

#include <cstdint>
#include <array>
#include "input_mapper.h"

// =============================================================================
// ARCHITECTURE & MULTIPLAYER DESIGN: AppState
// =============================================================================
//
// Why does AppState look like this?
//
// 1. ZERO GRAPHICS / PLATFORM DEPENDENCIES (Headless Server Ready):
//    - AppState contains NO OpenGL identifiers (VAOs, VBOs, textures), NO Wayland
//      window pointers, and NO display-server headers.
//    - Why: A dedicated server or multiplayer simulation worker must run without
//      a GPU or display server (Wayland/X11). Keeping AppState as pure C++ data
//      allows the exact same state and logic to run headless on a server.
//
// 2. PURE VALUE SEMANTICS (Trivially Copyable & Serializable):
//    - AppState is composed of plain standard data types with no pointers or deep graphs.
//    - Why: Multiplayer netcode relies on taking state snapshots and sending them
//      over the wire (UDP/TCP/WebSockets). Keeping state flat and self-contained
//      makes serialization, diffing, and syncing straightforward.
//
// 3. GRAPHICS AS A READ-ONLY OBSERVER:
//    - AppState represents the authoritative ground truth of the application.
//    - The graphics pipeline is strictly an observer: it reads AppState to draw
//      the screen, but AppState has zero awareness that rendering even exists.
//
// 4. HOW TO CUSTOMIZE / EXTEND:
//    - OPTION A (Inheritance): You can inherit from AppState and override update()
//      or add new gameplay fields to customize or remove the default behavior.
//    - OPTION B (RECOMMENDED - Replace Entirely): For real projects, you can
//      completely bypass or replace AppState with your own dedicated class.
//      The App and Graphics classes can be customized or templated to accept your
//      own domain-specific state representation.
// =============================================================================

class AppState {
public:
    virtual ~AppState() = default;

    // Simulation / State update logic (Can run on client and headless server alike)
    virtual void update(const InputMapper& input) {
        if (input.is_action_down(Action::ColorReset)) {
            color_ = { 1.0f, 0.5f, 0.2f }; // Orange
        } else if (input.is_action_down(Action::ColorRed)) {
            color_ = { 1.0f, 0.1f, 0.1f }; // Red
        } else if (input.is_action_down(Action::ColorGreen)) {
            color_ = { 0.1f, 1.0f, 0.1f }; // Green
        } else if (input.is_action_down(Action::ColorBlue)) {
            color_ = { 0.1f, 0.2f, 1.0f }; // Blue
        }
    }

    // Direct color controls (e.g. for mouse hooks or external events)
    virtual void set_color(float r, float g, float b) noexcept {
        color_ = { r, g, b };
    }

    [[nodiscard]] const std::array<float, 3>& get_color() const noexcept {
        return color_;
    }

protected:
    std::array<float, 3> color_{ 1.0f, 0.5f, 0.2f }; // Default orange
};

#endif // APP_STATE_H
