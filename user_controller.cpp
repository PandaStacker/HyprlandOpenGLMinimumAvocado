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
  //  After working on it for a while, it's clear that networked inputs should be
  //  sent through here, by having a global resource which creates them linked
  //  by the app_state when it initializes all the controllers.
  // So we are free to just make what we want here for now.
  // If we ever make other objects, NPCs, moving platforms, actually anything
  // interactive, they're made in the sim which becomes the server-side interpolated
  // objects when networked.

  // It turns out though, that this IS the right place to process the update for
  // user controllers. We just send our info to the network from here as well.
} 
