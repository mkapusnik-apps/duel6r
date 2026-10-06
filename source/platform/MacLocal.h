#ifndef DUEL6_MAC_LOCAL_H
#define DUEL6_MAC_LOCAL_H

#include <filesystem>
#include <string>

namespace Duel6::MacLocal {
    // SDL supplies the bundle Resources directory and the per-user application
    // support directory. Kept separate so a read-only bundle never receives saves.
    void preparePaths(const std::filesystem::path &resources,
                      const std::filesystem::path &applicationSupport);
    const std::string &personDataPath();
    const std::string &resourceDirectory();

}

#endif
