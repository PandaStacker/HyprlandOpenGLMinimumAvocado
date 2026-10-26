#ifndef LOCAL_SIM_SYNCHRONIZER_H
#define LOCAL_SIM_SYNCHRONIZER_H

#include <array>
#include "input_mapper.h"


class LocalSimSynchronizer {
  private:
    UserController user; // should only be one, generally speaking, unless splitscreen
    LocalSim sim;
    // server corrections
    // active menu, if any... this feels wrong here.

  public:
    virtual ~LocalSimSynchronizer() = default;

    // Simulation / State update logic (Runs on client and headless server alike)
    // Runs controller updates and world sim updates, graphics processed elsewhere
    // can the input mapper be passed directly to the user controller outside of this?
    // We maybe only need a reference to it.
    virtual void update(const InputMapper&, float deltaTime);
};

#endif // LOCAL_SIM_SYNCHRONIZER_H
