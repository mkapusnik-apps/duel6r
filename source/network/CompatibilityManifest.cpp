#include "CompatibilityManifest.h"

#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef D6R_TRANSPORT_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <climits>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

                std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
                std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
                for (std::size_t index = 0; index < words.size(); ++index) {
                    const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                    const std::uint32_t choose = (e & f) ^ (~e & g);
                    const std::uint32_t temporary1 = h + sum1 + choose + Sha256Constants[index] + words[index];
                    const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temporary2 = sum0 + majority;
                    h = g; g = f; f = e; e = d + temporary1;
                    d = c; c = b; b = a; a = temporary1 + temporary2;
                }
                state[0] += a; state[1] += b; state[2] += c; state[3] += d;
                state[4] += e; state[5] += f; state[6] += g; state[7] += h;
            }

            std::array<std::uint32_t, 8> state{{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                                 0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                                 0x1f83d9abu, 0x5be0cd19u}};
            std::array<std::uint8_t, 64> block{};
            std::size_t buffered = 0;
            std::uint64_t byteCount = 0;
        };

        bool approvedLogicalPath(const std::string &logical) {
            return Trust::validLogicalPath(logical)
                   && (logical.compare(0, 7, "levels/") == 0
                       || logical == "data/blocks.json"
                       || logical == "data/config.script"
                       || logical.compare(0, 8, "scripts/") == 0);
        }

#ifdef D6R_TRANSPORT_WINDOWS
        class Handle {
        public:
            explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value(value) {}
            ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
            Handle(const Handle &) = delete;
            Handle &operator=(const Handle &) = delete;
            Handle(Handle &&other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
            Handle &operator=(Handle &&other) noexcept {
                if (this != &other) {
                    if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
                    value = other.value;
                    other.value = INVALID_HANDLE_VALUE;
                }
                return *this;
            }
            HANDLE get() const { return value; }
            explicit operator bool() const { return value != INVALID_HANDLE_VALUE; }
        private:
            HANDLE value;
        };

        std::wstring finalPath(HANDLE handle) {
            const DWORD required = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED);
            if (required == 0 || required > 32768) return {};
            std::wstring result(required, L'\0');
            const DWORD written = GetFinalPathNameByHandleW(handle, result.data(), required, FILE_NAME_NORMALIZED);
            if (written == 0 || written >= required) return {};
            result.resize(written);
            return result;
        }

        bool ordinalEqual(std::wstring_view left, std::wstring_view right) {
            if (left.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())
                || right.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
            return CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                                        right.data(), static_cast<int>(right.size()), FALSE) == CSTR_EQUAL;
        }

        bool insideFinalRoot(const std::wstring &root, const std::wstring &candidate) {
            return candidate.size() > root.size()
                   && ordinalEqual(root, std::wstring_view(candidate.data(), root.size()))
                   && (candidate[root.size()] == L'\\' || candidate[root.size()] == L'/');
        }

        bool utf8Name(const wchar_t *value, std::string &result) {
            const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1 || size > 256) return false;
            std::string converted(static_cast<std::size_t>(size), '\0');
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, converted.data(), size,
                                    nullptr, nullptr) == 0) return false;
            converted.pop_back();
            result = std::move(converted);
            return std::all_of(result.begin(), result.end(), [](unsigned char byte) { return byte < 0x80; });
        }

        class SecureFilesystem {
        public:
            SecureFilesystem(const std::string &rootText, ManifestFilesystemObserver observer)
                    : observer(std::move(observer)) {
                try { rootPath = fs::absolute(fs::u8path(rootText)).lexically_normal(); } catch (...) { return; }
                root = Handle(CreateFileW(rootPath.c_str(), FILE_READ_ATTRIBUTES,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                if (!root) return;
                BY_HANDLE_FILE_INFORMATION information{};
                if (!GetFileInformationByHandle(root.get(), &information)
                    || !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return;
                rootVolume = information.dwVolumeSerialNumber;
                rootFinal = finalPath(root.get());
                validRoot = !rootFinal.empty() && notify(ManifestFilesystemStage::RootPinned, {});
            }

            bool valid() const { return validRoot; }

            ManifestStatus enumerate(std::vector<std::string> &paths) const {
                std::size_t directories = 0, examined = 0;
                return enumerateDirectory(rootPath / L"levels", "levels", 1, directories, examined, paths);
            }

            ManifestStatus size(const std::string &logical, std::uintmax_t &value) const {
                Handle file;
                BY_HANDLE_FILE_INFORMATION information{};
                const ManifestStatus status = openFile(logical, file, information);
                if (status != ManifestStatus::Valid) return status;
                value = (static_cast<std::uintmax_t>(information.nFileSizeHigh) << 32u) | information.nFileSizeLow;
                return value > MaxGameplayContentFileBytes ? ManifestStatus::ContentTooLarge : ManifestStatus::Valid;
            }

            ManifestStatus hash(const std::string &logical, std::uintmax_t expected, ContentIdentity &identity) const {
                Handle file;
                BY_HANDLE_FILE_INFORMATION before{};
                ManifestStatus status = openFile(logical, file, before);
                if (status != ManifestStatus::Valid) return status;
                if (!notify(ManifestFilesystemStage::HashStarted, logical)) return ManifestStatus::ReadFailed;
                const std::uintmax_t sizeValue = (static_cast<std::uintmax_t>(before.nFileSizeHigh) << 32u)
                                                 | before.nFileSizeLow;
                if (sizeValue != expected) return ManifestStatus::ReadFailed;
                Sha256 sha;
                std::array<std::uint8_t, 64 * 1024> buffer{};
                std::uintmax_t readBytes = 0;
                for (;;) {
                    if (!notify(ManifestFilesystemStage::BeforeRead, logical)) return ManifestStatus::ReadFailed;
                    DWORD read = 0;
                    if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
                        return ManifestStatus::ReadFailed;
                    if (read == 0) break;
                    readBytes += read;
                    if (readBytes > expected) return ManifestStatus::ReadFailed;
                    sha.update(buffer.data(), read);
                }
                BY_HANDLE_FILE_INFORMATION after{};
                if (readBytes != expected || !GetFileInformationByHandle(file.get(), &after)
                    || before.dwVolumeSerialNumber != after.dwVolumeSerialNumber
                    || before.nFileIndexHigh != after.nFileIndexHigh || before.nFileIndexLow != after.nFileIndexLow
                    || before.nFileSizeHigh != after.nFileSizeHigh || before.nFileSizeLow != after.nFileSizeLow
                    || CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) != 0)
                    return ManifestStatus::ReadFailed;
                if (!notify(ManifestFilesystemStage::HashCompleted, logical)) return ManifestStatus::ReadFailed;
                identity = sha.finish();
                return ManifestStatus::Valid;
            }

        private:
            ManifestStatus openFile(const std::string &logical, Handle &file,
                                    BY_HANDLE_FILE_INFORMATION &information) const {
                if (!approvedLogicalPath(logical)) return ManifestStatus::InvalidLogicalPath;
                fs::path relative;
                try { relative = fs::u8path(logical); } catch (...) { return ManifestStatus::InvalidLogicalPath; }
                std::vector<Handle> pinnedParents;
                const ManifestStatus pinned = pinParents(relative.parent_path(), pinnedParents);
                if (pinned != ManifestStatus::Valid) return pinned;
                file = Handle(CreateFileW((rootPath / relative).c_str(), GENERIC_READ,
                                          FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                          FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                if (!file || !GetFileInformationByHandle(file.get(), &information)) return ManifestStatus::ReadFailed;
                if ((information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT
                                                      | FILE_ATTRIBUTE_DEVICE))
                    || information.nNumberOfLinks != 1 || information.dwVolumeSerialNumber != rootVolume)
                    return ManifestStatus::UnsafeFilesystemEntry;
                const std::wstring resolved = finalPath(file.get());
                std::wstring expected = rootFinal;
                expected.push_back(L'\\');
                std::wstring relativeText = relative.native();
                std::replace(relativeText.begin(), relativeText.end(), L'/', L'\\');
                expected += relativeText;
                if (!insideFinalRoot(rootFinal, resolved) || !ordinalEqual(expected, resolved))
                    return ManifestStatus::UnsafeFilesystemEntry;
                return notify(ManifestFilesystemStage::FileOpened, logical) ? ManifestStatus::Valid
                                                                            : ManifestStatus::ReadFailed;
            }

            ManifestStatus pinParents(const fs::path &relativeParent, std::vector<Handle> &pinned) const {
                fs::path current = rootPath;
                std::string logical;
                for (const fs::path &segment: relativeParent) {
                    if (segment.empty()) continue;
                    current /= segment;
                    Handle directory(CreateFileW(current.c_str(), FILE_READ_ATTRIBUTES,
                                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                    BY_HANDLE_FILE_INFORMATION information{};
                    if (!directory || !GetFileInformationByHandle(directory.get(), &information))
                        return ManifestStatus::MissingRequiredContent;
                    if (!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                        || information.dwVolumeSerialNumber != rootVolume)
                        return ManifestStatus::UnsafeFilesystemEntry;
                    std::string name;
                    if (!utf8Name(segment.c_str(), name)) return ManifestStatus::InvalidLogicalPath;
                    if (!logical.empty()) logical += '/';
                    logical += name;
                    std::wstring expected = rootFinal + L"\\";
                    std::wstring logicalWide(logical.begin(), logical.end());
                    std::replace(logicalWide.begin(), logicalWide.end(), L'/', L'\\');
                    expected += logicalWide;
                    const std::wstring resolved = finalPath(directory.get());
                    if (!insideFinalRoot(rootFinal, resolved) || !ordinalEqual(expected, resolved))
                        return ManifestStatus::UnsafeFilesystemEntry;
                    if (!notify(ManifestFilesystemStage::DirectoryPinned, logical))
                        return ManifestStatus::ReadFailed;
                    pinned.push_back(std::move(directory));
                }
                return ManifestStatus::Valid;
            }

            ManifestStatus enumerateDirectory(const fs::path &directory, const std::string &logicalPrefix,
                                               std::size_t depth, std::size_t &directories, std::size_t &examined,
                                               std::vector<std::string> &paths) const {
                if (depth > Trust::MaxLogicalPathSegments || ++directories > MaxManifestDirectories)
                    return ManifestStatus::TooManyEntries;
                Handle directoryHandle(CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES,
                                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                                   nullptr));
                BY_HANDLE_FILE_INFORMATION directoryInfo{};
                if (!directoryHandle || !GetFileInformationByHandle(directoryHandle.get(), &directoryInfo))
                    return ManifestStatus::MissingRequiredContent;
                if (!(directoryInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    || (directoryInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    || directoryInfo.dwVolumeSerialNumber != rootVolume)
                    return ManifestStatus::UnsafeFilesystemEntry;
                std::wstring expected = rootFinal + L"\\";
                std::wstring logicalWide(logicalPrefix.begin(), logicalPrefix.end());
                std::replace(logicalWide.begin(), logicalWide.end(), L'/', L'\\');
                expected += logicalWide;
                const std::wstring resolvedDirectory = finalPath(directoryHandle.get());
                if (!insideFinalRoot(rootFinal, resolvedDirectory)
                    || !ordinalEqual(expected, resolvedDirectory)) return ManifestStatus::UnsafeFilesystemEntry;
                if (!notify(ManifestFilesystemStage::DirectoryPinned, logicalPrefix))
                    return ManifestStatus::ReadFailed;

                WIN32_FIND_DATAW data{};
                HANDLE raw = FindFirstFileW((directory / L"*").c_str(), &data);
                if (raw == INVALID_HANDLE_VALUE) return ManifestStatus::ReadFailed;
                struct FindCloser { HANDLE value; ~FindCloser() { FindClose(value); } } closer{raw};
                do {
                    if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
                    if (++examined > MaxManifestTraversalEntries) return ManifestStatus::TooManyEntries;
                    std::string name;
                    if (!utf8Name(data.cFileName, name)) return ManifestStatus::InvalidLogicalPath;
                    const std::string logical = logicalPrefix + "/" + name;
                    if (!notify(ManifestFilesystemStage::EntryExamined, logical))
                        return ManifestStatus::ReadFailed;
                    if (!Trust::validLogicalPath(logical)) return ManifestStatus::InvalidLogicalPath;
                    if (data.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
                        return ManifestStatus::UnsafeFilesystemEntry;
                    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        const ManifestStatus status = enumerateDirectory(directory / data.cFileName, logical,
                                                                         depth + 1, directories, examined, paths);
                        if (status != ManifestStatus::Valid) return status;
                    } else {
                        if (paths.size() >= Trust::MaxManifestEntries) return ManifestStatus::TooManyEntries;
                        paths.push_back(logical);
                    }
                } while (FindNextFileW(raw, &data));
                return GetLastError() == ERROR_NO_MORE_FILES ? ManifestStatus::Valid : ManifestStatus::ReadFailed;
            }

            fs::path rootPath;
            Handle root;
            std::wstring rootFinal;
            DWORD rootVolume = 0;
            bool validRoot = false;
            ManifestFilesystemObserver observer;

            bool notify(ManifestFilesystemStage stage, const std::string &logical) const {
                if (!observer) return true;
                try { return observer(stage, logical); } catch (...) { return false; }
            }
        };
#else
        class FileDescriptor {
        public:
            explicit FileDescriptor(int value = -1) : value(value) {}
            ~FileDescriptor() { if (value >= 0) ::close(value); }
            FileDescriptor(const FileDescriptor &) = delete;
            FileDescriptor &operator=(const FileDescriptor &) = delete;
            FileDescriptor(FileDescriptor &&other) noexcept : value(other.value) { other.value = -1; }
            FileDescriptor &operator=(FileDescriptor &&other) noexcept {
                if (this != &other) { if (value >= 0) ::close(value); value = other.value; other.value = -1; }
                return *this;
            }
            int get() const { return value; }
            explicit operator bool() const { return value >= 0; }
        private:
            int value;
        };

        bool unchanged(const struct stat &left, const struct stat &right) {
            return left.st_dev == right.st_dev && left.st_ino == right.st_ino && left.st_mode == right.st_mode
                   && left.st_nlink == right.st_nlink && left.st_size == right.st_size
                   && left.st_mtim.tv_sec == right.st_mtim.tv_sec && left.st_mtim.tv_nsec == right.st_mtim.tv_nsec
                   && left.st_ctim.tv_sec == right.st_ctim.tv_sec && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
        }

        class SecureFilesystem {
        public:
            SecureFilesystem(std::string rootText, ManifestFilesystemObserver observer)
                    : observer(std::move(observer)) {
                while (rootText.size() > 1 && (rootText.back() == '/' || rootText.back() == '\\')) rootText.pop_back();
                struct stat pathStatus{};
                if (::lstat(rootText.c_str(), &pathStatus) != 0 || S_ISLNK(pathStatus.st_mode)) return;
                root = FileDescriptor(::open(rootText.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
                if (!root || ::fstat(root.get(), &rootStatus) != 0 || !S_ISDIR(rootStatus.st_mode)) return;
                validRoot = notify(ManifestFilesystemStage::RootPinned, {});
            }

            bool valid() const { return validRoot; }

            ManifestStatus enumerate(std::vector<std::string> &paths) const {
                FileDescriptor levels(::openat(root.get(), "levels", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
                struct stat status{};
                if (!levels || ::fstat(levels.get(), &status) != 0) return ManifestStatus::MissingRequiredContent;
                if (!S_ISDIR(status.st_mode) || status.st_dev != rootStatus.st_dev)
                    return ManifestStatus::UnsafeFilesystemEntry;
                std::size_t directories = 0, examined = 0;
                return enumerateDirectory(levels.get(), "levels", 1, directories, examined, paths);
            }

            ManifestStatus size(const std::string &logical, std::uintmax_t &value) const {
                FileDescriptor file;
                struct stat status{};
                const ManifestStatus opened = openFile(logical, file, status);
                if (opened != ManifestStatus::Valid) return opened;
                if (status.st_size < 0) return ManifestStatus::ReadFailed;
                value = static_cast<std::uintmax_t>(status.st_size);
                return value > MaxGameplayContentFileBytes ? ManifestStatus::ContentTooLarge : ManifestStatus::Valid;
            }

            ManifestStatus hash(const std::string &logical, std::uintmax_t expected, ContentIdentity &identity) const {
                FileDescriptor file;
                struct stat before{};
                ManifestStatus opened = openFile(logical, file, before);
                if (opened != ManifestStatus::Valid || before.st_size < 0
                    || static_cast<std::uintmax_t>(before.st_size) != expected)
                    return opened == ManifestStatus::Valid ? ManifestStatus::ReadFailed : opened;
                if (!notify(ManifestFilesystemStage::HashStarted, logical)) return ManifestStatus::ReadFailed;
                Sha256 sha;
                std::array<std::uint8_t, 64 * 1024> buffer{};
                std::uintmax_t readBytes = 0;
                for (;;) {
                    if (!notify(ManifestFilesystemStage::BeforeRead, logical)) return ManifestStatus::ReadFailed;
                    const ssize_t count = ::read(file.get(), buffer.data(), buffer.size());
                    if (count < 0) { if (errno == EINTR) continue; return ManifestStatus::ReadFailed; }
                    if (count == 0) break;
                    readBytes += static_cast<std::uintmax_t>(count);
                    if (readBytes > expected) return ManifestStatus::ReadFailed;
                    sha.update(buffer.data(), static_cast<std::size_t>(count));
                }
                struct stat after{};
                if (readBytes != expected || ::fstat(file.get(), &after) != 0 || !unchanged(before, after))
                    return ManifestStatus::ReadFailed;
                if (!notify(ManifestFilesystemStage::HashCompleted, logical)) return ManifestStatus::ReadFailed;
                identity = sha.finish();
                return ManifestStatus::Valid;
            }

        private:
            ManifestStatus openFile(const std::string &logical, FileDescriptor &file, struct stat &status) const {
                if (!approvedLogicalPath(logical)) return ManifestStatus::InvalidLogicalPath;
                FileDescriptor directory(::dup(root.get()));
                if (!directory) return ManifestStatus::ReadFailed;
                std::size_t start = 0;
                while (true) {
                    const std::size_t slash = logical.find('/', start);
                    const std::string segment = logical.substr(start, slash == std::string::npos
                                                                       ? std::string::npos : slash - start);
                    if (slash == std::string::npos) {
                        file = FileDescriptor(::openat(directory.get(), segment.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
                        break;
                    }
                    FileDescriptor next(::openat(directory.get(), segment.c_str(),
                                                 O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
                    struct stat directoryStatus{};
                    if (!next || ::fstat(next.get(), &directoryStatus) != 0 || !S_ISDIR(directoryStatus.st_mode)
                        || directoryStatus.st_dev != rootStatus.st_dev)
                        return ManifestStatus::UnsafeFilesystemEntry;
                    directory = std::move(next);
                    start = slash + 1;
                }
                if (!file) {
                    if (errno == ENOENT) return ManifestStatus::MissingRequiredContent;
                    if (errno == ELOOP || errno == ENOTDIR) return ManifestStatus::UnsafeFilesystemEntry;
                    return ManifestStatus::ReadFailed;
                }
                if (::fstat(file.get(), &status) != 0) return ManifestStatus::ReadFailed;
                if (!S_ISREG(status.st_mode) || status.st_nlink != 1 || status.st_dev != rootStatus.st_dev)
                    return ManifestStatus::UnsafeFilesystemEntry;
                return notify(ManifestFilesystemStage::FileOpened, logical) ? ManifestStatus::Valid
                                                                            : ManifestStatus::ReadFailed;
            }

            ManifestStatus enumerateDirectory(int descriptor, const std::string &prefix, std::size_t depth,
                                               std::size_t &directories, std::size_t &examined,
                                               std::vector<std::string> &paths) const {
                if (depth > Trust::MaxLogicalPathSegments || ++directories > MaxManifestDirectories)
                    return ManifestStatus::TooManyEntries;
                if (!notify(ManifestFilesystemStage::DirectoryPinned, prefix)) return ManifestStatus::ReadFailed;
                const int duplicate = ::dup(descriptor);
                if (duplicate < 0) return ManifestStatus::ReadFailed;
                DIR *raw = ::fdopendir(duplicate);
                if (!raw) { ::close(duplicate); return ManifestStatus::ReadFailed; }
                struct DirectoryCloser { DIR *value; ~DirectoryCloser() { ::closedir(value); } } closer{raw};
                errno = 0;
                while (dirent *entry = ::readdir(raw)) {
                    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
                    if (++examined > MaxManifestTraversalEntries) return ManifestStatus::TooManyEntries;
                    const std::string name(entry->d_name);
                    const std::string logical = prefix + "/" + name;
                    if (!notify(ManifestFilesystemStage::EntryExamined, logical))
                        return ManifestStatus::ReadFailed;
                    if (!Trust::validLogicalPath(logical)) return ManifestStatus::InvalidLogicalPath;
                    struct stat status{};
                    if (::fstatat(descriptor, entry->d_name, &status, AT_SYMLINK_NOFOLLOW) != 0)
                        return ManifestStatus::ReadFailed;
                    if (S_ISLNK(status.st_mode) || status.st_dev != rootStatus.st_dev)
                        return ManifestStatus::UnsafeFilesystemEntry;
                    if (S_ISDIR(status.st_mode)) {
                        FileDescriptor child(::openat(descriptor, entry->d_name,
                                                      O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
                        if (!child) return ManifestStatus::UnsafeFilesystemEntry;
                        const ManifestStatus nested = enumerateDirectory(child.get(), logical, depth + 1,
                                                                         directories, examined, paths);
                        if (nested != ManifestStatus::Valid) return nested;
                    } else if (S_ISREG(status.st_mode)) {
                        if (status.st_nlink != 1) return ManifestStatus::UnsafeFilesystemEntry;
                        if (paths.size() >= Trust::MaxManifestEntries) return ManifestStatus::TooManyEntries;
                        paths.push_back(logical);
                    } else {
                        return ManifestStatus::UnsafeFilesystemEntry;
                    }
                    errno = 0;
                }
                return errno == 0 ? ManifestStatus::Valid : ManifestStatus::ReadFailed;
            }

            FileDescriptor root;
            struct stat rootStatus{};
            bool validRoot = false;
            ManifestFilesystemObserver observer;

            bool notify(ManifestFilesystemStage stage, const std::string &logical) const {
                if (!observer) return true;
                try { return observer(stage, logical); } catch (...) { return false; }
            }
        };
#endif

        class NativeManifestSource final : public ManifestSource {
        public:
            explicit NativeManifestSource(ManifestFilesystemObserver observer)
                    : observer(std::move(observer)) {}

            ManifestBuildResult build(const std::string &resourceRoot,
                                       const std::vector<std::string> &enabledScripts) const override {
                ManifestBuildResult result;
                SecureFilesystem filesystem(resourceRoot, observer);
                if (!filesystem.valid()) return result;

                std::vector<std::string> paths;
                result.status = filesystem.enumerate(paths);
                if (result.status != ManifestStatus::Valid) return result;
                paths.emplace_back("data/blocks.json");
                paths.emplace_back("data/config.script");
                for (const std::string &logical: enabledScripts) {
                    if (!Trust::validLogicalPath(logical) || logical.compare(0, 8, "scripts/") != 0) {
                        result.status = ManifestStatus::InvalidLogicalPath;
                        return result;
                    }
                    paths.push_back(logical);
                }
                if (paths.empty() || paths.size() > Trust::MaxManifestEntries) {
                    result.status = paths.empty() ? ManifestStatus::MissingRequiredContent : ManifestStatus::TooManyEntries;
                    return result;
                }
                std::sort(paths.begin(), paths.end());
                if (std::adjacent_find(paths.begin(), paths.end()) != paths.end()) {
                    result.status = ManifestStatus::DuplicateLogicalPath;
                    return result;
                }

                std::vector<std::uintmax_t> sizes;
                sizes.reserve(paths.size());
                std::uintmax_t total = 0;
                for (const std::string &logical: paths) {
                    if (!approvedLogicalPath(logical)) {
                        result.status = ManifestStatus::InvalidLogicalPath;
                        return result;
                    }
                    std::uintmax_t size = 0;
                    result.status = filesystem.size(logical, size);
                    if (result.status != ManifestStatus::Valid) return result;
                    if (total > MaxGameplayContentTotalBytes - size) {
                        result.status = ManifestStatus::ContentTooLarge;
                        return result;
                    }
                    total += size;
                    sizes.push_back(size);
                }

                result.manifest.reserve(paths.size());
                for (std::size_t index = 0; index < paths.size(); ++index) {
                    GameplayManifestEntry entry;
                    entry.logicalPath = paths[index];
                    result.status = filesystem.hash(entry.logicalPath, sizes[index], entry.contentIdentity);
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

        private:
            ManifestFilesystemObserver observer;
        };
    }

    CompatibilityManifestBuilder::CompatibilityManifestBuilder(std::string resourceRoot,
                                                                 std::vector<std::string> enabledGameplayScripts,
                                                                 std::shared_ptr<const ManifestSource> source,
                                                                 ManifestFilesystemObserver filesystemObserver)
            : resourceRoot(std::move(resourceRoot)), enabledGameplayScripts(std::move(enabledGameplayScripts)),
              source(source ? std::move(source)
                            : std::make_shared<NativeManifestSource>(std::move(filesystemObserver))) {}

    ManifestBuildResult CompatibilityManifestBuilder::build() const {
        try {
            return source ? source->build(resourceRoot, enabledGameplayScripts) : ManifestBuildResult{};
        } catch (...) {
            return {};
        }
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
