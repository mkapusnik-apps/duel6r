#ifndef DUEL6_NETWORK_COMPATIBILITYMANIFEST_H
#define DUEL6_NETWORK_COMPATIBILITYMANIFEST_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Duel6::Network {
    constexpr std::size_t ContentIdentityBytes = 32;
    constexpr std::uintmax_t MaxGameplayContentFileBytes = 64u * 1024u * 1024u;
    constexpr std::uintmax_t MaxGameplayContentTotalBytes = 256u * 1024u * 1024u;

    using ContentIdentity = std::array<std::uint8_t, ContentIdentityBytes>;

    struct GameplayManifestEntry {
        std::string logicalPath;
        ContentIdentity contentIdentity{};
    };

    using GameplayManifest = std::vector<GameplayManifestEntry>;

    enum class ManifestStatus {
        Valid,
        InvalidRoot,
        MissingRequiredContent,
        UnsafeFilesystemEntry,
        InvalidLogicalPath,
        DuplicateLogicalPath,
        TooManyEntries,
        ContentTooLarge,
        ReadFailed
    };

    struct ManifestBuildResult {
        ManifestStatus status = ManifestStatus::InvalidRoot;
        GameplayManifest manifest;

        bool valid() const { return status == ManifestStatus::Valid; }
    };

    class CompatibilityManifestBuilder {
    public:
        explicit CompatibilityManifestBuilder(std::string resourceRoot,
                                              std::vector<std::string> enabledGameplayScripts = {});

        ManifestBuildResult build() const;

    private:
        std::string resourceRoot;
        std::vector<std::string> enabledGameplayScripts;
    };

    bool validCanonicalManifest(const GameplayManifest &manifest);
    bool gameplayManifestsEqual(const GameplayManifest &left, const GameplayManifest &right);
    std::string contentIdentityHex(const ContentIdentity &identity);
}

#endif
