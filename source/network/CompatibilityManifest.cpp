#include "CompatibilityManifest.h"

#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace Duel6::Network {
    namespace {
        namespace fs = std::filesystem;

        constexpr std::array<std::uint32_t, 64> Sha256Constants{{
                0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
                0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
                0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
                0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
                0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
                0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
                0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
                0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
                0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        }};

        std::uint32_t rotateRight(std::uint32_t value, unsigned amount) {
            return (value >> amount) | (value << (32u - amount));
        }

        class Sha256 {
        public:
            void update(const std::uint8_t *data, std::size_t size) {
                if (size > (std::numeric_limits<std::uint64_t>::max() - byteCount) / 8u)
                    throw std::overflow_error("Gameplay content is too large to hash");
                byteCount += static_cast<std::uint64_t>(size) * 8u;
                while (size > 0) {
                    const std::size_t copied = (std::min)(size, block.size() - buffered);
                    std::copy_n(data, copied, block.data() + buffered);
                    data += copied;
                    size -= copied;
                    buffered += copied;
                    if (buffered == block.size()) {
                        transform(block.data());
                        buffered = 0;
                    }
                }
            }

            ContentIdentity finish() {
                block[buffered++] = 0x80;
                if (buffered > 56) {
                    std::fill(block.begin() + static_cast<std::ptrdiff_t>(buffered), block.end(), 0);
                    transform(block.data());
                    buffered = 0;
                }
                std::fill(block.begin() + static_cast<std::ptrdiff_t>(buffered), block.begin() + 56, 0);
                for (std::size_t index = 0; index < 8; ++index)
                    block[63 - index] = static_cast<std::uint8_t>(byteCount >> (index * 8u));
                transform(block.data());

                ContentIdentity result{};
                for (std::size_t index = 0; index < state.size(); ++index) {
                    result[index * 4] = static_cast<std::uint8_t>(state[index] >> 24u);
                    result[index * 4 + 1] = static_cast<std::uint8_t>(state[index] >> 16u);
                    result[index * 4 + 2] = static_cast<std::uint8_t>(state[index] >> 8u);
                    result[index * 4 + 3] = static_cast<std::uint8_t>(state[index]);
                }
                return result;
            }

        private:
            void transform(const std::uint8_t *input) {
                std::array<std::uint32_t, 64> words{};
                for (std::size_t index = 0; index < 16; ++index) {
                    const std::size_t offset = index * 4;
                    words[index] = static_cast<std::uint32_t>(input[offset]) << 24u
                                   | static_cast<std::uint32_t>(input[offset + 1]) << 16u
                                   | static_cast<std::uint32_t>(input[offset + 2]) << 8u
                                   | static_cast<std::uint32_t>(input[offset + 3]);
                }
                for (std::size_t index = 16; index < words.size(); ++index) {
                    const std::uint32_t s0 = rotateRight(words[index - 15], 7)
                                             ^ rotateRight(words[index - 15], 18)
                                             ^ (words[index - 15] >> 3u);
                    const std::uint32_t s1 = rotateRight(words[index - 2], 17)
                                             ^ rotateRight(words[index - 2], 19)
                                             ^ (words[index - 2] >> 10u);
                    words[index] = words[index - 16] + s0 + words[index - 7] + s1;
                }

                std::uint32_t a = state[0];
                std::uint32_t b = state[1];
                std::uint32_t c = state[2];
                std::uint32_t d = state[3];
                std::uint32_t e = state[4];
                std::uint32_t f = state[5];
                std::uint32_t g = state[6];
                std::uint32_t h = state[7];
                for (std::size_t index = 0; index < words.size(); ++index) {
                    const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                    const std::uint32_t choose = (e & f) ^ (~e & g);
                    const std::uint32_t temporary1 = h + sum1 + choose + Sha256Constants[index] + words[index];
                    const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temporary2 = sum0 + majority;
                    h = g;
                    g = f;
                    f = e;
                    e = d + temporary1;
                    d = c;
                    c = b;
                    b = a;
                    a = temporary1 + temporary2;
                }
                state[0] += a;
                state[1] += b;
                state[2] += c;
                state[3] += d;
                state[4] += e;
                state[5] += f;
                state[6] += g;
                state[7] += h;
            }

            std::array<std::uint32_t, 8> state{{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                                 0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                                 0x1f83d9abu, 0x5be0cd19u}};
            std::array<std::uint8_t, 64> block{};
            std::size_t buffered = 0;
            std::uint64_t byteCount = 0;
        };

        bool pathInside(const fs::path &root, const fs::path &candidate) {
            auto rootIterator = root.begin();
            auto candidateIterator = candidate.begin();
            while (rootIterator != root.end() && candidateIterator != candidate.end()) {
                if (*rootIterator != *candidateIterator) return false;
                ++rootIterator;
                ++candidateIterator;
            }
            return rootIterator == root.end();
        }

        ManifestStatus hashFile(const fs::path &root, const fs::path &path, ContentIdentity &identity,
                                std::uintmax_t &totalBytes) {
            std::error_code error;
            if (fs::symlink_status(path, error).type() != fs::file_type::regular || error)
                return ManifestStatus::UnsafeFilesystemEntry;
            const fs::path canonical = fs::canonical(path, error);
            if (error || !pathInside(root, canonical)) return ManifestStatus::UnsafeFilesystemEntry;
            const std::uintmax_t size = fs::file_size(canonical, error);
            if (error) return ManifestStatus::ReadFailed;
            if (size > MaxGameplayContentFileBytes || totalBytes > MaxGameplayContentTotalBytes - size)
                return ManifestStatus::ContentTooLarge;

#if defined(_WIN32) && !defined(_MSC_VER) && !defined(__MINGW32__)
            // A non-Windows compiler may emulate _WIN32 while retaining native narrow streams.
            // Real Windows toolchains accept fs::path and preserve native Unicode paths.
            std::ifstream stream(canonical.string(), std::ios::binary);
#else
            std::ifstream stream(canonical, std::ios::binary);
#endif
            if (!stream) return ManifestStatus::ReadFailed;
            Sha256 sha;
            std::array<std::uint8_t, 64 * 1024> buffer{};
            std::uintmax_t readBytes = 0;
            while (stream) {
                stream.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
                const auto count = stream.gcount();
                if (count > 0) {
                    readBytes += static_cast<std::uintmax_t>(count);
                    if (readBytes > size) return ManifestStatus::ReadFailed;
                    sha.update(buffer.data(), static_cast<std::size_t>(count));
                }
            }
            if (!stream.eof() || readBytes != size) return ManifestStatus::ReadFailed;
            identity = sha.finish();
            totalBytes += size;
            return ManifestStatus::Valid;
        }

        bool isGameplayDataFile(const std::string &logicalPath) {
            return logicalPath == "data/blocks.json" || logicalPath == "data/config.script";
        }
    }

    CompatibilityManifestBuilder::CompatibilityManifestBuilder(std::string resourceRoot,
                                                                 std::vector<std::string> enabledGameplayScripts)
            : resourceRoot(std::move(resourceRoot)), enabledGameplayScripts(std::move(enabledGameplayScripts)) {}

    ManifestBuildResult CompatibilityManifestBuilder::build() const {
        ManifestBuildResult result;
        std::error_code error;
        const fs::path root = fs::canonical(fs::path(resourceRoot), error);
        if (error || !fs::is_directory(root, error) || error) return result;

        std::vector<std::pair<std::string, fs::path>> candidates;
        const auto addTree = [&](const fs::path &relativeRoot) -> ManifestStatus {
            const fs::path directory = root / relativeRoot;
            if (!fs::exists(directory, error) || error) return ManifestStatus::MissingRequiredContent;
            if (fs::symlink_status(directory, error).type() != fs::file_type::directory || error)
                return ManifestStatus::UnsafeFilesystemEntry;
            fs::recursive_directory_iterator iterator(directory, fs::directory_options::none, error);
            const fs::recursive_directory_iterator end;
            for (; !error && iterator != end; iterator.increment(error)) {
                const fs::file_status status = iterator->symlink_status(error);
                if (error) break;
                if (status.type() == fs::file_type::symlink || status.type() == fs::file_type::unknown)
                    return ManifestStatus::UnsafeFilesystemEntry;
                if (status.type() != fs::file_type::regular) continue;
                const std::string logical = iterator->path().lexically_relative(root).generic_string();
                candidates.emplace_back(logical, iterator->path());
            }
            return error ? ManifestStatus::ReadFailed : ManifestStatus::Valid;
        };

        result.status = addTree("levels");
        if (result.status != ManifestStatus::Valid) return result;
        for (const char *dataPath: {"data/blocks.json", "data/config.script"}) {
            const fs::path path = root / fs::path(dataPath);
            if (!fs::exists(path, error) || error) {
                result.status = ManifestStatus::MissingRequiredContent;
                return result;
            }
            candidates.emplace_back(dataPath, path);
        }
        for (const std::string &logical: enabledGameplayScripts) {
            if (!Trust::validLogicalPath(logical) || logical.compare(0, 8, "scripts/") != 0) {
                result.status = ManifestStatus::InvalidLogicalPath;
                return result;
            }
            candidates.emplace_back(logical, root / fs::path(logical));
        }

        if (candidates.empty() || candidates.size() > Trust::MaxManifestEntries) {
            result.status = candidates.empty() ? ManifestStatus::MissingRequiredContent : ManifestStatus::TooManyEntries;
            return result;
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
            return left.first < right.first;
        });
        std::set<std::string> paths;
        std::uintmax_t totalBytes = 0;
        result.manifest.reserve(candidates.size());
        for (const auto &candidate: candidates) {
            if (!Trust::validLogicalPath(candidate.first) ||
                (candidate.first.compare(0, 7, "levels/") != 0
                 && !isGameplayDataFile(candidate.first)
                 && candidate.first.compare(0, 8, "scripts/") != 0)) {
                result.status = ManifestStatus::InvalidLogicalPath;
                result.manifest.clear();
                return result;
            }
            if (!paths.insert(candidate.first).second) {
                result.status = ManifestStatus::DuplicateLogicalPath;
                result.manifest.clear();
                return result;
            }
            GameplayManifestEntry entry;
            entry.logicalPath = candidate.first;
            result.status = hashFile(root, candidate.second, entry.contentIdentity, totalBytes);
            if (result.status != ManifestStatus::Valid) {
                result.manifest.clear();
                return result;
            }
            result.manifest.push_back(std::move(entry));
        }
        result.status = validCanonicalManifest(result.manifest) ? ManifestStatus::Valid
                                                                : ManifestStatus::InvalidLogicalPath;
        if (!result.valid()) result.manifest.clear();
        return result;
    }

    bool validCanonicalManifest(const GameplayManifest &manifest) {
        if (manifest.empty() || !Trust::validManifestEntryCount(manifest.size())) return false;
        for (std::size_t index = 0; index < manifest.size(); ++index) {
            if (!Trust::validLogicalPath(manifest[index].logicalPath)) return false;
            if (index > 0 && !(manifest[index - 1].logicalPath < manifest[index].logicalPath)) return false;
        }
        return true;
    }

    bool gameplayManifestsEqual(const GameplayManifest &left, const GameplayManifest &right) {
        if (!validCanonicalManifest(left) || !validCanonicalManifest(right) || left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (left[index].logicalPath != right[index].logicalPath
                || left[index].contentIdentity != right[index].contentIdentity) return false;
        }
        return true;
    }

    std::string contentIdentityHex(const ContentIdentity &identity) {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (std::uint8_t byte: identity) stream << std::setw(2) << static_cast<unsigned>(byte);
        return stream.str();
    }
}
