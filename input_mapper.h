#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <type_traits>
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
// Core Engine Actions
// -----------------------------------------------------------------------------
// ActionId is a generic uint32_t. Consumers can define their own enums or IDs
// starting from Action::User without touching this library header.
using ActionId = uint32_t;

namespace Action {
    inline constexpr ActionId None            = 0;
    inline constexpr ActionId Quit            = 1;
    inline constexpr ActionId PrimaryAction   = 2; // Mouse Left / Stylus Tip / Gamepad A
    inline constexpr ActionId SecondaryAction = 3; // Mouse Right / Stylus Barrel / Gamepad B
    inline constexpr ActionId MiddleAction    = 4; // Mouse Middle Click
    inline constexpr ActionId User            = 100; // Base index for consumer actions
}

// -----------------------------------------------------------------------------
// Abstract Analog Axes
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
    struct ActionState {
        bool held = false;
        bool just_pressed = false;
        bool enabled = true;
    };

    InputMapper() {
        axis_values.fill(0.0f);
        load_defaults();
    }

    // --- Action (Digital) Remapping API ---
    void bind_action(ActionId action, InputDevice device, uint32_t code) {
        action_bindings[action].push_back(DigitalBinding{ .device = device, .code = code });
        action_states[action].enabled = true;
    }

    // Type-safe enum overload: accepts any user-defined enum without casting
    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    void bind_action(EnumT action, InputDevice device, uint32_t code) {
        bind_action(static_cast<ActionId>(action), device, code);
    }

    void clear_action_bindings(ActionId action) {
        action_bindings.erase(action);
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    void clear_action_bindings(EnumT action) {
        clear_action_bindings(static_cast<ActionId>(action));
    }

    void unbind_key(ActionId action, uint32_t keysym) {
        if (auto it = action_bindings.find(action); it != action_bindings.end()) {
            std::erase_if(it->second, [&](const DigitalBinding& b) {
                return b.device == InputDevice::Keyboard && b.code == keysym;
            });
        }
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    void unbind_key(EnumT action, uint32_t keysym) {
        unbind_key(static_cast<ActionId>(action), keysym);
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

    // --- Feed Raw Hardware Events (Called internally by Wayland window) ---
    void process_digital(InputDevice device, uint32_t code, bool pressed) {
        for (const auto& [action, bindings] : action_bindings) {
            for (const auto& b : bindings) {
                if (b.device == device && b.code == code) {
                    auto& state = action_states[action];
                    if (pressed && !state.held) {
                        state.just_pressed = true;
                    }
                    state.held = pressed;
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
        for (auto& [id, state] : action_states) {
            state.just_pressed = false;
        }
    }

    // --- Action Enable / Disable API ---
    void set_action_enabled(ActionId action, bool enabled) noexcept {
        action_states[action].enabled = enabled;
    }

    void enable_exit_action(bool enabled = true) noexcept {
        set_action_enabled(Action::Quit, enabled);
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    void set_action_enabled(EnumT action, bool enabled) noexcept {
        set_action_enabled(static_cast<ActionId>(action), enabled);
    }

    [[nodiscard]] bool is_action_enabled(ActionId action) const noexcept {
        auto it = action_states.find(action);
        return (it != action_states.end()) ? it->second.enabled : true;
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    [[nodiscard]] bool is_action_enabled(EnumT action) const noexcept {
        return is_action_enabled(static_cast<ActionId>(action));
    }

    // --- State Query API (The single unified way gameplay queries input) ---
    [[nodiscard]] bool is_action_down(ActionId action) const noexcept {
        auto it = action_states.find(action);
        return (it != action_states.end()) && it->second.enabled && it->second.held;
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    [[nodiscard]] bool is_action_down(EnumT action) const noexcept {
        return is_action_down(static_cast<ActionId>(action));
    }

    [[nodiscard]] bool is_action_just_pressed(ActionId action) const noexcept {
        auto it = action_states.find(action);
        return (it != action_states.end()) && it->second.enabled && it->second.just_pressed;
    }

    template <typename EnumT>
        requires std::is_enum_v<EnumT>
    [[nodiscard]] bool is_action_just_pressed(EnumT action) const noexcept {
        return is_action_just_pressed(static_cast<ActionId>(action));
    }

    [[nodiscard]] float get_axis(VirtualAxis axis) const noexcept {
        return axis_values[static_cast<size_t>(axis)];
    }

    void load_defaults();

private:
    std::unordered_map<ActionId, std::vector<DigitalBinding>> action_bindings;
    std::unordered_map<ActionId, ActionState>                action_states;
    std::unordered_map<VirtualAxis, AxisBinding>            axis_bindings;

    std::array<float, static_cast<size_t>(VirtualAxis::Count)> axis_values;
};

inline void InputMapper::load_defaults() {
    action_bindings.clear();
    action_states.clear();
    axis_bindings.clear();

    // 1. Core System Actions
    bind_action(Action::Quit,            InputDevice::Keyboard, XKB_KEY_Escape);

    // 2. Default Mouse Actions
    bind_action(Action::PrimaryAction,   InputDevice::Mouse,    BTN_LEFT);
    bind_action(Action::SecondaryAction, InputDevice::Mouse,    BTN_RIGHT);
    bind_action(Action::MiddleAction,    InputDevice::Mouse,    BTN_MIDDLE);

    // 3. Default Gamepad Actions
    bind_action(Action::PrimaryAction,   InputDevice::Gamepad,  0); // A / Cross
    bind_action(Action::SecondaryAction, InputDevice::Gamepad,  1); // B / Circle

    // 4. Default Gamepad Axes
    bind_axis({ .axis = VirtualAxis::MoveX,    .device = InputDevice::Gamepad, .raw_axis = 0, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::MoveY,    .device = InputDevice::Gamepad, .raw_axis = 1, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::LookX,    .device = InputDevice::Gamepad, .raw_axis = 3, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::LookY,    .device = InputDevice::Gamepad, .raw_axis = 4, .deadzone = 0.15f });
    bind_axis({ .axis = VirtualAxis::TriggerL, .device = InputDevice::Gamepad, .raw_axis = 2, .deadzone = 0.05f });
    bind_axis({ .axis = VirtualAxis::TriggerR, .device = InputDevice::Gamepad, .raw_axis = 5, .deadzone = 0.05f });
}

#endif // INPUT_MAPPER_H
