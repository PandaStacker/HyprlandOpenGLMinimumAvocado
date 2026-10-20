#pragma once USER_CONTROLLER_H
#include <array>
#include "input_mapper.h"

enum DefaultAction : uiint32_t {
  MoveUp = Action::User,
  MoveDown,
  MoveLeft,
  moveRight,
  ColorReset,
  ColorRed,
  ColorGreen,
  ColorBlue
};

namespace Constants {
  inline constexpr float MOVE_SPEED = 0.015f;
}

class UserController {
public:
  virtual ~AppState() = default;

  virtual void bind_inputs(InputMapper& input);
  virtual void update(const InputMapper& input);
  
  // triangle position ref or something
};
