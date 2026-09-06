#define _GNU_SOURCE

#include <dlfcn.h>
#include <SDL2/SDL.h>

typedef int (*SDLInitSubSystem)(Uint32 flags);

int SDL_InitSubSystem(Uint32 flags) {
    static SDLInitSubSystem initialize;
    static int virtualControllerAttached;

    if (initialize == NULL) {
        initialize = (SDLInitSubSystem) dlsym(RTLD_NEXT, "SDL_InitSubSystem");
    }

    int result = initialize(flags);
    if (result == 0 && !virtualControllerAttached && (flags & SDL_INIT_JOYSTICK) != 0) {
        SDL_VirtualJoystickDesc controller = {0};
        controller.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        controller.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        controller.naxes = SDL_CONTROLLER_AXIS_MAX;
        controller.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
        controller.name = "Controller1234";

        virtualControllerAttached = SDL_JoystickAttachVirtualEx(&controller) >= 0;
    }

    return result;
}
