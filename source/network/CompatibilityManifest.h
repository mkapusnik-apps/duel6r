#ifndef DUEL6_NETWORK_COMPATIBILITYMANIFEST_H
#define DUEL6_NETWORK_COMPATIBILITYMANIFEST_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace Duel6::Network {
    constexpr std::size_t ContentIdentityBytes = 32;
    constexpr std::uintmax_t MaxGameplayContentFileBytes = 64u * 1024u * 1024u;
    constexpr std::uintmax_t MaxGameplayContentTotalBytes = 256u * 1024u * 1024u;
    constexpr std::size_t MaxManifestDirectories = 256;
    constexpr std::size_t MaxManifestTraversalEntries = 512;

    using ContentIdentity = std::array<std::uint8_t, ContentIdentityBytes>;

    struct GameplayManifestEntry {
        std::string logicalPath;
        ContentIdentity contentIdentity{};
    };

    using GameplayManifest = std::vector<GameplayManifestEntry>;

    using FrozenGameplayContent = std::map<std::string, std::vector<std::uint8_t>>;

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
        std::shared_ptr<const FrozenGameplayContent> content;

        bool valid() const { return status == ManifestStatus::Valid; }
    };

    enum class ManifestFilesystemStage {
        RootPinned,
        DirectoryPinned,
        EntryExamined,
        FileOpened,
        HashStarted,
        BeforeRead,
        HashCompleted
    };
    using ManifestFilesystemObserver =
            std::function<bool(ManifestFilesystemStage, const std::string &logicalPath)>;

    class ManifestSource {
    public:
        virtual ~ManifestSource() = default;
        virtual ManifestBuildResult build(const std::string &resourceRoot,
                                          const std::vector<std::string> &enabledGameplayScripts) const = 0;
    };

    class CompatibilityManifestBuilder {
    public:
        explicit CompatibilityManifestBuilder(std::string resourceRoot,
                                              std::vector<std::string> enabledGameplayScripts = {},
                                              std::shared_ptr<const ManifestSource> source = {},
                                              ManifestFilesystemObserver filesystemObserver = {});

        ManifestBuildResult build() const;

    private:
        std::string resourceRoot;
        std::vector<std::string> enabledGameplayScripts;
        std::shared_ptr<const ManifestSource> source;
    };

    bool validCanonicalManifest(const GameplayManifest &manifest);
    bool gameplayManifestsEqual(const GameplayManifest &left, const GameplayManifest &right);
    std::string contentIdentityHex(const ContentIdentity &identity);
}

#endif
