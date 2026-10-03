#ifndef DUEL6_MAC_LOCAL_H
#define DUEL6_MAC_LOCAL_H

#include <filesystem>
#include <string>
#include <SDL2/SDL_events.h>

namespace Duel6::MacLocal {
    // SDL supplies the bundle Resources directory and the per-user application
    // support directory. Kept separate so a read-only bundle never receives saves.
    void preparePaths(const std::filesystem::path &resources,
                      const std::filesystem::path &applicationSupport);
    const std::string &personDataPath();

    constexpr const char *networkMessage =
            "Network play is unavailable in this macOS build. Use Play (F1) for local play. Press any key.";

    class NetworkMessage {
    public:
        void open() { visible = true; }
        bool isVisible() const { return visible; }
        bool consume(const SDL_Event &event);

    private:
        bool visible = false;
        SDL_Scancode dismissalKey = SDL_SCANCODE_UNKNOWN;
    };
}

#endif
