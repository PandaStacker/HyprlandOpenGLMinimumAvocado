#ifndef USER_CONTROLLER_H
#define USER_CONTROLLER_H

#include <array>
#include "input_mapper.hpp"

namespace Constants {
  inline constexpr float MOVE_SPEED = 0.015f;
}

// Demo-Specific Actions (Custom Enums)
enum DemoAction : uint32_t {
    MoveUp = Action::User,
    MoveDown,
    MoveLeft,
    MoveRight,
    ColorReset,
    ColorRed,
    ColorGreen,
    ColorBlue
};

class UserController {
private:
    // add context, are we controlling a player, a menu, what?
public:
    virtual ~UserController() = default;

    virtual void bind_inputs(InputMapper& input);
    virtual void update(const InputMapper& input);
};

#endif // USER_CONTROLLER_H
