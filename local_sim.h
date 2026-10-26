#ifndef APP_STATE_H
#define APP_STATE_H

#include <array>
#include "input_mapper.h"

// =============================================================================
// ARCHITECTURE & MULTIPLAYER DESIGN: AppState
// =============================================================================
//
// Why does AppState look like this?
//
// 1. ZERO GRAPHICS / PLATFORM DEPENDENCIES (Headless Server Ready):
//    - AppState contains NO OpenGL identifiers, NO Wayland window pointers, and
//      NO display-server headers.
//    - A dedicated server can run this exact state in a headless Linux CLI.
//      This is no longer true in our current config, but it's close, it would
//      simply be running the server-side user controller rather than the client's
//      server controller.
//      Is it even possible to make this universal? Why would we do that? The Server's
//      state and the client's state are of course going to have differences in
//      anything that isn't synced.
//
// 2. UNIFIED INPUT CONSUMPTION:
//    - AppState only queries InputMapper (via is_action_down / is_action_just_pressed).
//    - No raw callbacks, no duplicate mapping.
//
// 3. HOW TO REPLACE THIS:
//    - This class is just a minimal demonstration. For your actual game or app,
//      simply replace this file with your own state representation.
//    - Edit the user controller to change and link up the controls.
// =============================================================================

class AppState {
  private:
    UserController user; // should only be one, generally speaking

  public:
    virtual ~AppState() = default;

    // Registers demo-specific keybindings into the generic input mapper
    // This is now a controller instantiater.
    virtual void InitializeControllers();

    // Simulation / State update logic (Runs on client and headless server alike)
    // Runs controller updates and world sim updates, graphics processed elsewhere
    // Maybe should have and pass deltatime?
    virtual void update();
};

#endif // APP_STATE_H
