#include "MacLocal.h"
#include <stdexcept>

namespace Duel6::MacLocal {
    namespace {
        std::string savedPersonPath;
        std::string bundledResources;
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
        bundledResources = std::filesystem::canonical(resources).string();
    }

    const std::string &personDataPath() {
        if (savedPersonPath.empty())
            throw std::runtime_error("macOS application paths have not been initialized");
        return savedPersonPath;
    }

    const std::string &resourceDirectory() {
        if (bundledResources.empty()) throw std::runtime_error("macOS application paths have not been initialized");
        return bundledResources;
    }

}
