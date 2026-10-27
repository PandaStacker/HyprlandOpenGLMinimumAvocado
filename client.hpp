#ifndef CLIENT_H
#define CLIENT_H

#include <array>
#include "input_mapper.hpp"
#include "user_controller.hpp"

class Client {
private:
    UserController user; // should only be one, generally speaking, unless splitscreen
    // server corrections
    // active menu, if any... this feels wrong here.

public:
    virtual ~Client() = default;

    virtual void bind_inputs(InputMapper& input);

    // Simulation / State update logic (Runs on client and headless server alike)
    // Runs controller updates and world sim updates, graphics processed elsewhere
    // can the input mapper be passed directly to the user controller outside of this?
    // We maybe only need a reference to it.
    virtual void update(const InputMapper& input, float deltaTime);

    // Baseline state properties (simple, demonstrative, unopinionated)
    float x = 0.0f;
    float y = 0.0f;
    std::array<float, 3> color{ 1.0f, 0.5f, 0.2f }; // Default orange
};

#endif // CLIENT_H
