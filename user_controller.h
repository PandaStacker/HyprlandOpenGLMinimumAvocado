#pragma once USER_CONTROLLER_H
#include <array>
#include "input_mapper.h"
#include "local_sim.h"

namespace Constants {
  inline constexpr float MOVE_SPEED = 0.015f;
}

class UserController {
private:
    InputMapper inputer;
    LocalSim localsim;
    // add context, are we controlling a player, a menu, what?
public:
  virtual ~AppState() = default;

  virtual void bind_inputs(InputMapper& input);
  virtual void update(const InputMapper& input);
};
