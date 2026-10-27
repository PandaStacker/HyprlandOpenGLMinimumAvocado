#include "user_controller.hpp"

void UserController::bind_inputs(InputMapper& input) {
    input.bind_action(DemoAction::MoveUp,     InputDevice::Keyboard, XKB_KEY_w);
    input.bind_action(DemoAction::MoveDown,   InputDevice::Keyboard, XKB_KEY_s);
    input.bind_action(DemoAction::MoveRight,  InputDevice::Keyboard, XKB_KEY_d);
    input.bind_action(DemoAction::MoveLeft,   InputDevice::Keyboard, XKB_KEY_a);

    input.bind_action(DemoAction::ColorReset, InputDevice::Keyboard, XKB_KEY_space);
    input.bind_action(DemoAction::ColorRed,   InputDevice::Keyboard, XKB_KEY_r);
    input.bind_action(DemoAction::ColorGreen, InputDevice::Keyboard, XKB_KEY_g);
    input.bind_action(DemoAction::ColorBlue,  InputDevice::Keyboard, XKB_KEY_b);

    input.bind_action(DemoAction::ShaderModeNext,     InputDevice::Keyboard, XKB_KEY_Tab);
    input.bind_action(DemoAction::ShaderModeCombined, InputDevice::Keyboard, XKB_KEY_0);
    input.bind_action(DemoAction::ShaderModeSkybox,   InputDevice::Keyboard, XKB_KEY_1);
    input.bind_action(DemoAction::ShaderModePBR,      InputDevice::Keyboard, XKB_KEY_2);
    input.bind_action(DemoAction::ShaderModeTriangle, InputDevice::Keyboard, XKB_KEY_3);
    input.bind_action(DemoAction::ShaderModeUI,       InputDevice::Keyboard, XKB_KEY_4);
}

void UserController::update(const InputMapper& input) {
    (void)input;
    // process inputs
    // send over network
    // if no network, just send to client reference
}
