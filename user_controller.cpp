#include "user_controller.h"

void UserController::bind_inputs(InputMapper& input) {
  input.bind_action(DemoAction::MoveUp,     InputDevice::Keyboard, XKB_KEY_w);
  input.bind_action(DemoAction::MoveDown,   InputDevice::Keyboard, XKB_KEY_s);
  input.bind_action(DemoAction::MoveRight,  InputDevice::Keyboard, XKB_KEY_d);
  input.bind_action(DemoAction::MoveLeft,   InputDevice::Keyboard, XKB_KEY_a);

  input.bind_action(DemoAction::ColorReset, InputDevice::Keyboard, XKB_KEY_space);
  input.bind_action(DemoAction::ColorRed,   InputDevice::Keyboard, XKB_KEY_r);
  input.bind_action(DemoAction::ColorGreen, InputDevice::Keyboard, XKB_KEY_g);
  input.bind_action(DemoAction::ColorBlue,  InputDevice::Keyboard, XKB_KEY_b);
}

void UserController::update(const InputMapper& input) {
  // In a networked game, we would be creating a network packet to send, right?
  // Every controller just makes network packets.
  // So does an offline controller not make them or does it send them with 0
  // latency, or does it...?
  // We should certainly have a way to send a change in state without hassle.
  // We would have a ServerPredictedMoveCommand or something, which we'll alias
  // to something shorter.
  // Also, I think we do all the checks here as well? We get updates from the server
  // on some things like position, although we're constantly predicting it as
  // well. It's a local variable.
  //
  //if (input.is_action_down(DemoAction::MoveUp)) y+= MOVE_SPEED;
} 
