#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

// -----------------------------------------------------------------------------
// Hardware Input Devices
// -----------------------------------------------------------------------------
enum class InputDevice : uint8_t {
    Keyboard,
    Mouse,
    Gamepad,
    Touch,
    Stylus
};

// -----------------------------------------------------------------------------
// Abstract Digital Actions (Buttons & Keys only - Strictly no analog axes)
// -----------------------------------------------------------------------------
enum class Action : uint32_t {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Jump,
    ColorRed,
    ColorGreen,
    ColorBlue,
    ColorReset,
    PrimaryAction,   // Mouse Left Click / Stylus Tip / Gamepad A
    SecondaryAction, // Mouse Right Click / Stylus Barrel / Gamepad B
    Quit,
    Count
};

// -----------------------------------------------------------------------------
// Abstract Analog Axes (Continuous inputs only - Strictly no buttons/keys)
// -----------------------------------------------------------------------------
enum class VirtualAxis : uint32_t {
    MoveX,    // Left/Right motion (-1.0 to +1.0)
    MoveY,    // Up/Down motion    (-1.0 to +1.0)
    LookX,    // Camera aim X      (-1.0 to +1.0)
    LookY,    // Camera aim Y      (-1.0 to +1.0)
    TriggerL, // Left trigger      (0.0 to 1.0)
    TriggerR, // Right trigger     (0.0 to 1.0)
    Count
};

// -----------------------------------------------------------------------------
// Binding Descriptors (C++20 Designated Initializers Supported)
// -----------------------------------------------------------------------------
struct DigitalBinding {
    InputDevice device;
    uint32_t    code; // Keysym (XKB), Mouse button code, or Gamepad button index
};

struct AxisBinding {
    VirtualAxis axis      = VirtualAxis::MoveX;
    InputDevice device    = InputDevice::Gamepad;
    uint32_t    raw_axis  = 0;     // Physical axis number
    bool        inverted  = false; // Invert axis sign
    float       deadzone  = 0.15f; // Deadzone threshold [0.0 to 1.0]
};

// -----------------------------------------------------------------------------
// InputMapper Class (C++20)
// -----------------------------------------------------------------------------
class InputMapper {
public:
    InputMapper() {
        action_held.fill(false);
        action_just_pressed.fill(false);
        axis_values.fill(0.0f);
        load_defaults();
    }

    // --- Action (Digital) Remapping API ---
    void bind_action(Action action, InputDevice device, uint32_t code) {
        action_bindings[action].push_back(DigitalBinding{ .device = device, .code = code });
    }

    void clear_action_bindings(Action action) {
        action_bindings.erase(action);
    }

    void unbind_key(Action action, uint32_t keysym) {
        if (auto it = action_bindings.find(action); it != action_bindings.end()) {
            std::erase_if(it->second, [&](const DigitalBinding& b) {
                return b.device == InputDevice::Keyboard && b.code == keysym;
            });
        }
    }

    // --- Axis (Analog) Remapping API ---
    void bind_axis(const AxisBinding& binding) {
        axis_bindings[binding.axis] = binding;
    }

    void swap_axes(VirtualAxis a, VirtualAxis b) {
        std::swap(axis_bindings[a], axis_bindings[b]);
        axis_bindings[a].axis = a;
        axis_bindings[b].axis = b;
    }

    void set_axis_inverted(VirtualAxis axis, bool inverted) {
        axis_bindings[axis].inverted = inverted;
    }

    // --- Feed Raw Hardware Events ---
    void process_digital(InputDevice device, uint32_t code, bool pressed) {
        for (const auto& [action, bindings] : action_bindings) {
            for (const auto& b : bindings) {
                if (b.device == device && b.code == code) {
                    size_t idx = static_cast<size_t>(action);
                    if (pressed && !action_held[idx]) {
                        action_just_pressed[idx] = true;
                    }
                    action_held[idx] = pressed;
                }
            }
        }
    }

    void process_axis(InputDevice device, uint32_t raw_axis, float raw_value) {
        for (const auto& [axis, b] : axis_bindings) {
            if (b.device == device && b.raw_axis == raw_axis) {
                float val = raw_value;
                if (std::abs(val) < b.deadzone) {
                    val = 0.0f;
                }
                if (b.inverted) {
                    val = -val;
                }
                axis_values[static_cast<size_t>(axis)] = val;
            }
        }
    }

    void post_frame_update() {
        action_just_pressed.fill(false);
    }

    // --- State Query API ---
    [[nodiscard]] bool is_action_down(Action action) const noexcept {
        return action_held[static_cast<size_t>(action)];
    }

    [[nodiscard]] bool is_action_just_pressed(Action action) const noexcept {
        return action_just_pressed[static_cast<size_t>(action)];
    }

    [[nodiscard]] float get_axis(VirtualAxis axis) const noexcept {
        return axis_values[static_cast<size_t>(axis)];
    }

    // Helper string conversion for UI menus / settings
    static constexpr std::string_view to_string(Action act) noexcept {
        switch (act) {
            case Action::MoveUp:          return "Move Up";
            case Action::MoveDown:        return "Move Down";
            case Action::MoveLeft:        return "Move Left";
            case Action::MoveRight:       return "Move Right";
            case Action::Jump:            return "Jump";
            case Action::ColorRed:        return "Color Red";
            case Action::ColorGreen:      return "Color Green";
            case Action::ColorBlue:       return "Color Blue";
            case Action::ColorReset:      return "Reset Color";
            case Action::PrimaryAction:   return "Primary Action";
            case Action::SecondaryAction: return "Secondary Action";
            case Action::Quit:            return "Quit";
            default:                      return "Unknown Action";
        }
    }

    void load_defaults();

private:
    std::unordered_map<Action, std::vector<DigitalBinding>> action_bindings;
    std::unordered_map<VirtualAxis, AxisBinding>           axis_bindings;

    std::array<bool, static_cast<size_t>(Action::Count)>      action_held;
    std::array<bool, static_cast<size_t>(Action::Count)>      action_just_pressed;
    std::array<float, static_cast<size_t>(VirtualAxis::Count)> axis_values;
};

inline void InputMapper::load_defaults() {
    action_bindings.clear();
    axis_bindings.clear();

    // Default Keyboard Bindings
    bind_action(Action::Quit,            InputDevice::Keyboard, XKB_KEY_Escape);
    bind_action(Action::ColorReset,      InputDevice::Keyboard, XKB_KEY_space);
    bind_action(Action::ColorRed,        InputDevice::Keyboard, XKB_KEY_r);
    bind_action(Action::ColorRed,        InputDevice::Keyboard, XKB_KEY_R);
    bind_action(Action::ColorGreen,      InputDevice::Keyboard, XKB_KEY_g);
    bind_action(Action::ColorGreen,      InputDevice::Keyboard, XKB_KEY_G);
    bind_action(Action::ColorBlue,       InputDevice::Keyboard, XKB_KEY_b);
    bind_action(Action::ColorBlue,       InputDevice::Keyboard, XKB_KEY_B);

    // Default Mouse Bindings
    bind_action(Action::PrimaryAction,   InputDevice::Mouse, BTN_LEFT);
    bind_action(Action::SecondaryAction, InputDevice::Mouse, BTN_RIGHT);

    // Default Gamepad Buttons
    bind_action(Action::PrimaryAction,   InputDevice::Gamepad, 0); // A / Cross
    bind_action(Action::SecondaryAction, InputDevice::Gamepad, 1); // B / Circle

    // Default Gamepad Axes (C++20 Designated Initializers)
    bind_axis({ .axis = VirtualAxis::MoveX,    .device = InputDevice::Gamepad, .raw_axis = 0, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::MoveY,    .device = InputDevice::Gamepad, .raw_axis = 1, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::LookX,    .device = InputDevice::Gamepad, .raw_axis = 3, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::LookY,    .device = InputDevice::Gamepad, .raw_axis = 4, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::TriggerL, .device = InputDevice::Gamepad, .raw_axis = 2, .deadzone = 0.05f });
    bind_axis({ .axis = VirtualAxis::TriggerR, .device = InputDevice::Gamepad, .raw_axis = 5, .deadzone = 0.05f });
}

#endif // INPUT_MAPPER_H
