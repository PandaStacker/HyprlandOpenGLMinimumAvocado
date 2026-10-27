#include "client.hpp"

void Client::bind_inputs(InputMapper& input) {
    user.bind_inputs(input);
}

void Client::update(const InputMapper& input, float deltaTime) {
    // client should actually just pass updates to entities
    time += deltaTime;
    constexpr float MOVE_SPEED = 1.05f;

    // Shader Display Mode Switching
    if (input.is_action_just_pressed(DemoAction::ShaderModeNext)) {
        render_mode = (render_mode + 1) % 5;
    } else if (input.is_action_just_pressed(DemoAction::ShaderModeCombined)) {
        render_mode = 0;
    } else if (input.is_action_just_pressed(DemoAction::ShaderModeSkybox)) {
        render_mode = 1;
    } else if (input.is_action_just_pressed(DemoAction::ShaderModePBR)) {
        render_mode = 2;
    } else if (input.is_action_just_pressed(DemoAction::ShaderModeTriangle)) {
        render_mode = 3;
    } else if (input.is_action_just_pressed(DemoAction::ShaderModeUI)) {
        render_mode = 4;
    }

    // Movement (polled through InputMapper)
    if (input.is_action_down(DemoAction::MoveUp))    y += MOVE_SPEED * deltaTime;
    if (input.is_action_down(DemoAction::MoveDown))  y -= MOVE_SPEED * deltaTime;
    if (input.is_action_down(DemoAction::MoveLeft))  x -= MOVE_SPEED * deltaTime;
    if (input.is_action_down(DemoAction::MoveRight)) x += MOVE_SPEED * deltaTime;

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
