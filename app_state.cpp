#include "app_state.h"

void AppState::bind_inputs(InputMapper& input) {
    // Movement Bindings (WASD and Arrows)
    input.bind_action(DemoAction::MoveUp,    InputDevice::Keyboard, XKB_KEY_w);
    input.bind_action(DemoAction::MoveDown,  InputDevice::Keyboard, XKB_KEY_s);
    input.bind_action(DemoAction::MoveLeft,  InputDevice::Keyboard, XKB_KEY_a);
    input.bind_action(DemoAction::MoveRight, InputDevice::Keyboard, XKB_KEY_d);

    // Color triggers (Keyboard keys)
    input.bind_action(DemoAction::ColorReset, InputDevice::Keyboard, XKB_KEY_space);
    input.bind_action(DemoAction::ColorRed,   InputDevice::Keyboard, XKB_KEY_r);
    input.bind_action(DemoAction::ColorGreen, InputDevice::Keyboard, XKB_KEY_g);
    input.bind_action(DemoAction::ColorBlue,  InputDevice::Keyboard, XKB_KEY_b);
}

void AppState::update(const InputMapper& input) {
  // appstate should actually just pass updates to entities
    constexpr float MOVE_SPEED = 0.015f;

    // Movement (polled through InputMapper)
    if (input.is_action_down(DemoAction::MoveUp))    y += MOVE_SPEED;
    if (input.is_action_down(DemoAction::MoveDown))  y -= MOVE_SPEED;
    if (input.is_action_down(DemoAction::MoveLeft))  x -= MOVE_SPEED;
    if (input.is_action_down(DemoAction::MoveRight)) x += MOVE_SPEED;

    // Keyboard colors (polled through InputMapper)
    if (input.is_action_down(DemoAction::ColorReset)) {
        color = { 1.0f, 0.5f, 0.2f }; // Orange
    } else if (input.is_action_down(DemoAction::ColorRed)) {
        color = { 1.0f, 0.1f, 0.1f }; // Red
    } else if (input.is_action_down(DemoAction::ColorGreen)) {
        color = { 0.1f, 1.0f, 0.1f }; // Green
    } else if (input.is_action_down(DemoAction::ColorBlue)) {
        color = { 0.1f, 0.2f, 1.0f }; // Blue
    }

    // Mouse clicks (polled through InputMapper! No raw callbacks!)
    if (input.is_action_just_pressed(Action::PrimaryAction)) {
        color = { 0.2f, 0.8f, 0.2f }; // Green on Left Click
    } else if (input.is_action_just_pressed(Action::SecondaryAction)) {
        color = { 0.2f, 0.4f, 1.0f }; // Blue on Right Click
    } else if (input.is_action_just_pressed(Action::MiddleAction)) {
        color = { 1.0f, 1.0f, 0.2f }; // Yellow on Middle Click
    }
}
