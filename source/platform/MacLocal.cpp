#include "MacLocal.h"
#include <stdexcept>

namespace Duel6::MacLocal {
    namespace {
        std::string savedPersonPath;
    }

    void preparePaths(const std::filesystem::path &resources,
                      const std::filesystem::path &applicationSupport) {
        if (!resources.is_absolute() || !applicationSupport.is_absolute())
            throw std::runtime_error("macOS application paths must be absolute");
        for (const char *directory : {"data", "levels", "profiles", "shaders", "sound", "textures"}) {
            if (!std::filesystem::is_directory(resources / directory))
                throw std::runtime_error("Missing bundled resource directory: " + std::string(directory));
        }
        std::filesystem::create_directories(applicationSupport / "data");
        const auto path = (applicationSupport / "data" / "persons.json").string();
        std::filesystem::current_path(resources);
        savedPersonPath = path;
    }

    const std::string &personDataPath() {
        if (savedPersonPath.empty())
            throw std::runtime_error("macOS application paths have not been initialized");
        return savedPersonPath;
    }

    bool NetworkMessage::consume(const SDL_Event &event) {
        // Window close is always handled by the normal application event loop.
        if (event.type == SDL_QUIT) return false;
        if (dismissalKey != SDL_SCANCODE_UNKNOWN) {
            if (event.type == SDL_KEYUP && event.key.keysym.scancode == dismissalKey) {
                dismissalKey = SDL_SCANCODE_UNKNOWN;
                return true;
            }
            if (event.type == SDL_TEXTINPUT || event.type == SDL_TEXTEDITING
                || ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                    && event.key.keysym.scancode == dismissalKey)) return true;
        }
        if (!visible) return false;
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            visible = false;
            dismissalKey = event.key.keysym.scancode;
        }
        // Do not let pointer input, text input or repeated keys alter the retained
        // menu. Device/window events still reach the regular application handling.
        return event.type == SDL_KEYDOWN || event.type == SDL_KEYUP
               || event.type == SDL_TEXTINPUT || event.type == SDL_TEXTEDITING
               || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP
               || event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL;
    }
}
