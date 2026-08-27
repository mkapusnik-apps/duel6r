#include "SessionTransport.h"
#include "ResolverProtocol.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using SocketHandle = SOCKET;
static constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <spawn.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
using SocketHandle = int;
static constexpr SocketHandle InvalidSocket = -1;
#endif

namespace Duel6::Network {
    namespace {
        using Clock = std::chrono::steady_clock;
        constexpr auto StartupDeadline = std::chrono::seconds(10);
        constexpr auto ProgressDeadline = std::chrono::seconds(5);
        constexpr auto ReceiveIdleDeadline = std::chrono::seconds(30);
        constexpr auto LivenessInterval = std::chrono::seconds(10);
        constexpr auto GracefulCloseDeadline = std::chrono::seconds(2);
        constexpr std::uint16_t ApplicationFrame = 0;
        constexpr std::uint16_t LivenessPing = 1;
        constexpr std::uint16_t LivenessPong = 2;
        constexpr std::size_t MaxResolverResponseBytes = ResolverProtocol::HeaderBytes
                                                         + ResolverProtocol::MaxAddresses * 4;

#ifdef D6R_TRANSPORT_WINDOWS
        class SocketRuntime {
        public:
            SocketRuntime() {
                WSADATA data{};
                valid = WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }
            ~SocketRuntime() { if (valid) WSACleanup(); }
            bool ready() const { return valid; }
        private:
            bool valid = false;
        };

        int socketError() { return WSAGetLastError(); }
        bool wouldBlock(int error) { return error == WSAEWOULDBLOCK; }
        bool interrupted(int error) { return error == WSAEINTR; }
        void closeSocket(SocketHandle socket) { if (socket != InvalidSocket) closesocket(socket); }
        void shutdownSocket(SocketHandle socket) { if (socket != InvalidSocket) ::shutdown(socket, SD_BOTH); }
        bool setNonBlocking(SocketHandle socket) {
            u_long enabled = 1;
            return ioctlsocket(socket, FIONBIO, &enabled) == 0;
        }
        bool preventInheritance(SocketHandle socket) {
            return SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0) != 0;
        }
        bool connectPending(int error) {
            return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINVAL;
        }
        TransportFailure connectFailure(int error) {
            if (error == WSAECONNREFUSED) return TransportFailure::ConnectionRefused;
            if (error == WSAEHOSTUNREACH || error == WSAENETUNREACH) return TransportFailure::Unreachable;
            return TransportFailure::SystemError;
        }
#else
        class SocketRuntime {
        public:
            bool ready() const { return true; }
        };

        int socketError() { return errno; }
        bool wouldBlock(int error) { return error == EAGAIN || error == EWOULDBLOCK; }
        bool interrupted(int error) { return error == EINTR; }
        void closeSocket(SocketHandle socket) { if (socket != InvalidSocket) ::close(socket); }
        void shutdownSocket(SocketHandle socket) { if (socket != InvalidSocket) ::shutdown(socket, SHUT_RDWR); }
        bool setNonBlocking(SocketHandle socket) {
            int flags = fcntl(socket, F_GETFL, 0);
            return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
        }
        bool preventInheritance(SocketHandle socket) {
            int flags = fcntl(socket, F_GETFD, 0);
            return flags >= 0 && fcntl(socket, F_SETFD, flags | FD_CLOEXEC) == 0;
        }
        bool connectPending(int error) { return error == EINPROGRESS; }
        TransportFailure connectFailure(int error) {
            if (error == ECONNREFUSED) return TransportFailure::ConnectionRefused;
            if (error == EHOSTUNREACH || error == ENETUNREACH) return TransportFailure::Unreachable;
            return TransportFailure::SystemError;
        }
#endif

        SocketRuntime &socketRuntime() {
            static SocketRuntime runtime;
            return runtime;
        }

        void writeU16(std::uint8_t *target, std::uint16_t value) {
            target[0] = static_cast<std::uint8_t>(value >> 8u);
            target[1] = static_cast<std::uint8_t>(value);
        }

        void writeU32(std::uint8_t *target, std::uint32_t value) {
            target[0] = static_cast<std::uint8_t>(value >> 24u);
            target[1] = static_cast<std::uint8_t>(value >> 16u);
            target[2] = static_cast<std::uint8_t>(value >> 8u);
            target[3] = static_cast<std::uint8_t>(value);
        }

        std::uint16_t readU16(const std::uint8_t *source) {
            return static_cast<std::uint16_t>((source[0] << 8u) | source[1]);
        }

        std::uint32_t readU32(const std::uint8_t *source) {
            return (static_cast<std::uint32_t>(source[0]) << 24u)
                   | (static_cast<std::uint32_t>(source[1]) << 16u)
                   | (static_cast<std::uint32_t>(source[2]) << 8u)
                   | source[3];
        }

        bool waitSocket(SocketHandle socket, bool writing, std::chrono::milliseconds timeout) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(socket, &set);
            timeval value{};
            value.tv_sec = static_cast<long>(timeout.count() / 1000);
            value.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
            int result = writing
                         ? select(static_cast<int>(socket + 1), nullptr, &set, nullptr, &value)
                         : select(static_cast<int>(socket + 1), &set, nullptr, nullptr, &value);
            return result > 0;
        }

        bool configureTransportSocket(SocketHandle socket) {
            return preventInheritance(socket) && setNonBlocking(socket);
        }

        bool configureConnectedSocket(SocketHandle socket) {
            if (!configureTransportSocket(socket)) return false;
            int enabled = 1;
            setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
            return true;
        }

        TransportTimePoint realNow() {
            return Clock::now();
        }

        TransportTimePoint dependencyNow(const SessionTransportDependencies &dependencies) {
            return dependencies.now ? dependencies.now() : realNow();
        }

        sockaddr_in socketAddress(const ResolvedIpv4Endpoint &endpoint) {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(endpoint.port);
            std::memcpy(&address.sin_addr.s_addr, endpoint.address.data(), endpoint.address.size());
            return address;
        }

        std::vector<std::uint8_t> resolverRequest(const std::string &host) {
            std::vector<std::uint8_t> request(ResolverProtocol::HeaderBytes + host.size());
            ResolverProtocol::writeU32(request.data(), ResolverProtocol::RequestMagic);
            ResolverProtocol::writeU32(request.data() + 4, static_cast<std::uint32_t>(host.size()));
            ResolverProtocol::writeU32(request.data() + 8, 0);
            std::memcpy(request.data() + ResolverProtocol::HeaderBytes, host.data(), host.size());
            return request;
        }

        ResolveOutcome resolverResponse(const std::vector<std::uint8_t> &response, std::uint16_t port) {
            if (response.size() < ResolverProtocol::HeaderBytes
                || ResolverProtocol::readU32(response.data()) != ResolverProtocol::ResponseMagic
                || ResolverProtocol::readU32(response.data() + 4) != 0) return {};
            std::uint32_t count = ResolverProtocol::readU32(response.data() + 8);
            if (count == 0 || count > ResolverProtocol::MaxAddresses
                || response.size() != ResolverProtocol::HeaderBytes + count * 4u) return {};
            ResolveOutcome outcome;
            outcome.status = ResolveStatus::Resolved;
            for (std::uint32_t index = 0; index < count; ++index) {
                ResolvedIpv4Endpoint endpoint;
                std::memcpy(endpoint.address.data(),
                            response.data() + ResolverProtocol::HeaderBytes + index * 4u, 4);
                endpoint.port = port;
                outcome.endpoints.push_back(endpoint);
            }
            return outcome;
        }

#ifdef D6R_TRANSPORT_WINDOWS
        std::wstring resolverExecutablePath() {
            std::array<wchar_t, 32768> path{};
            DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0 || length >= path.size()) return {};
            std::wstring executable(path.data(), length);
            std::size_t separator = executable.find_last_of(L"/\\");
            if (separator == std::wstring::npos) return L"duel6r-resolver.exe";
            return executable.substr(0, separator + 1) + L"duel6r-resolver.exe";
        }

        ResolveOutcome realResolve(const std::string &host, std::uint16_t port, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now) {
            SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE childInputRead = nullptr, parentInputWrite = nullptr;
            HANDLE parentOutputRead = nullptr, childOutputWrite = nullptr;
            if (!CreatePipe(&childInputRead, &parentInputWrite, &attributes, 0)
                || !CreatePipe(&parentOutputRead, &childOutputWrite, &attributes, 0)) {
                if (childInputRead) CloseHandle(childInputRead);
                if (parentInputWrite) CloseHandle(parentInputWrite);
                if (parentOutputRead) CloseHandle(parentOutputRead);
                if (childOutputWrite) CloseHandle(childOutputWrite);
                return {};
            }
            SetHandleInformation(parentInputWrite, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(parentOutputRead, HANDLE_FLAG_INHERIT, 0);
            SIZE_T attributeBytes = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
            std::vector<std::uint8_t> attributeStorage(attributeBytes);
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdInput = childInputRead;
            startup.StartupInfo.hStdOutput = childOutputWrite;
            startup.StartupInfo.hStdError = childOutputWrite;
            startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
            HANDLE inheritedHandles[] = {childInputRead, childOutputWrite};
            bool attributesReady = InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes)
                                   && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                                               PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles,
                                                               sizeof(inheritedHandles), nullptr, nullptr);
            PROCESS_INFORMATION process{};
            std::wstring executable = resolverExecutablePath();
            std::wstring command = L'"' + executable + L'"';
            std::vector<wchar_t> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back('\0');
            BOOL created = attributesReady && !executable.empty()
                           && CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                                             CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                             &startup.StartupInfo, &process);
            if (startup.lpAttributeList) DeleteProcThreadAttributeList(startup.lpAttributeList);
            CloseHandle(childInputRead);
            CloseHandle(childOutputWrite);
            if (!created) {
                CloseHandle(parentInputWrite);
                CloseHandle(parentOutputRead);
                return {};
            }
            CloseHandle(process.hThread);
            std::vector<std::uint8_t> request = resolverRequest(host);
            DWORD written = 0;
            bool requestSent = WriteFile(parentInputWrite, request.data(), static_cast<DWORD>(request.size()),
                                         &written, nullptr) && written == request.size();
            CloseHandle(parentInputWrite);
            std::vector<std::uint8_t> response;
            response.reserve(MaxResolverResponseBytes);
            bool responseValid = true;
            while (requestSent && !cancelled() && now() < deadline) {
                DWORD available = 0;
                if (!PeekNamedPipe(parentOutputRead, nullptr, 0, nullptr, &available, nullptr)) break;
                if (available > 0) {
                    std::array<std::uint8_t, 256> buffer{};
                    DWORD count = 0;
                    if (!ReadFile(parentOutputRead, buffer.data(),
                                  std::min<DWORD>(available, static_cast<DWORD>(buffer.size())), &count, nullptr)) break;
                    if (response.size() + count > MaxResolverResponseBytes) {
                        responseValid = false;
                        break;
                    }
                    response.insert(response.end(), buffer.begin(), buffer.begin() + count);
                }
                if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
                    while (true) {
                        DWORD remaining = 0;
                        if (!PeekNamedPipe(parentOutputRead, nullptr, 0, nullptr, &remaining, nullptr)
                            || remaining == 0) break;
                        std::array<std::uint8_t, 256> buffer{};
                        DWORD count = 0;
                        if (!ReadFile(parentOutputRead, buffer.data(),
                                      std::min<DWORD>(remaining, static_cast<DWORD>(buffer.size())), &count, nullptr)) break;
                        if (response.size() + count > MaxResolverResponseBytes) {
                            responseValid = false;
                            break;
                        }
                        response.insert(response.end(), buffer.begin(), buffer.begin() + count);
                    }
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            bool wasCancelled = cancelled();
            bool wasTimedOut = now() >= deadline;
            if (WaitForSingleObject(process.hProcess, 0) != WAIT_OBJECT_0) {
                TerminateProcess(process.hProcess, 3);
                WaitForSingleObject(process.hProcess, 900);
            }
            CloseHandle(parentOutputRead);
            CloseHandle(process.hProcess);
            if (wasCancelled) return {ResolveStatus::Cancelled, {}};
            if (wasTimedOut) return {ResolveStatus::TimedOut, {}};
            return requestSent && responseValid ? resolverResponse(response, port) : ResolveOutcome{};
        }
#else
        std::string resolverExecutablePath() {
            std::array<char, 4096> path{};
            ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1);
            if (length <= 0 || static_cast<std::size_t>(length) >= path.size()) return {};
            std::string executable(path.data(), static_cast<std::size_t>(length));
            std::size_t separator = executable.find_last_of('/');
            if (separator == std::string::npos) return "duel6r-resolver";
            return executable.substr(0, separator + 1) + "duel6r-resolver";
        }

        void stopResolverProcess(pid_t process) {
            if (process <= 0) return;
            kill(process, SIGKILL);
            while (waitpid(process, nullptr, 0) < 0 && errno == EINTR) {}
        }

        bool writeAll(int descriptor, const std::vector<std::uint8_t> &data) {
            std::size_t offset = 0;
            while (offset < data.size()) {
                ssize_t count = send(descriptor, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
                if (count > 0) offset += static_cast<std::size_t>(count);
                else if (count < 0 && errno == EINTR) continue;
                else return false;
            }
            return true;
        }

        bool moveAboveStandardDescriptors(int &descriptor) {
            if (descriptor > STDERR_FILENO) return true;
            int moved = fcntl(descriptor, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
            if (moved < 0) return false;
            close(descriptor);
            descriptor = moved;
            return true;
        }

        ResolveOutcome realResolve(const std::string &host, std::uint16_t port, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now) {
            int requestPipe[2], responsePipe[2];
            if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, requestPipe) != 0) return {};
            if (pipe2(responsePipe, O_CLOEXEC) != 0) {
                close(requestPipe[0]); close(requestPipe[1]);
                return {};
            }
            if (!moveAboveStandardDescriptors(requestPipe[0]) || !moveAboveStandardDescriptors(requestPipe[1])
                || !moveAboveStandardDescriptors(responsePipe[0]) || !moveAboveStandardDescriptors(responsePipe[1])) {
                close(requestPipe[0]); close(requestPipe[1]);
                close(responsePipe[0]); close(responsePipe[1]);
                return {};
            }
            posix_spawn_file_actions_t actions;
            int actionStatus = posix_spawn_file_actions_init(&actions);
            bool actionsInitialized = actionStatus == 0;
            if (actionStatus == 0)
                actionStatus = posix_spawn_file_actions_adddup2(&actions, requestPipe[0], STDIN_FILENO);
            if (actionStatus == 0)
                actionStatus = posix_spawn_file_actions_adddup2(&actions, responsePipe[1], STDOUT_FILENO);
            if (actionStatus == 0)
                actionStatus = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
            if (actionStatus == 0)
                actionStatus = posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);
            std::string executable = resolverExecutablePath();
            char *arguments[] = {executable.empty() ? nullptr : executable.data(), nullptr};
            pid_t process = 0;
            int spawned = actionStatus != 0 ? actionStatus
                                            : (executable.empty() ? ENOENT
                                                                  : posix_spawn(&process, executable.c_str(), &actions,
                                                                                nullptr, arguments, environ));
            if (actionsInitialized) posix_spawn_file_actions_destroy(&actions);
            close(requestPipe[0]);
            close(responsePipe[1]);
            if (spawned != 0) {
                close(requestPipe[1]); close(responsePipe[0]);
                return {};
            }
            std::vector<std::uint8_t> request = resolverRequest(host);
            bool requestSent = writeAll(requestPipe[1], request);
            close(requestPipe[1]);
            bool responseConfigured = setNonBlocking(responsePipe[0]);
            std::vector<std::uint8_t> response;
            response.reserve(MaxResolverResponseBytes);
            bool responseValid = true;
            bool exited = false;
            while (requestSent && responseConfigured && !cancelled() && now() < deadline) {
                if (waitSocket(responsePipe[0], false, std::chrono::milliseconds(10))) {
                    std::array<std::uint8_t, 256> buffer{};
                    ssize_t count = read(responsePipe[0], buffer.data(), buffer.size());
                    if (count > 0) {
                        if (response.size() + static_cast<std::size_t>(count) > MaxResolverResponseBytes) {
                            responseValid = false;
                            break;
                        }
                        response.insert(response.end(), buffer.begin(), buffer.begin() + count);
                    }
                }
                pid_t status = waitpid(process, nullptr, WNOHANG);
                if (status == process) { exited = true; break; }
                if (status < 0 && errno != EINTR) break;
            }
            if (exited) {
                while (true) {
                    std::array<std::uint8_t, 256> buffer{};
                    ssize_t count = read(responsePipe[0], buffer.data(), buffer.size());
                    if (count > 0) {
                        if (response.size() + static_cast<std::size_t>(count) > MaxResolverResponseBytes) {
                            responseValid = false;
                            break;
                        }
                        response.insert(response.end(), buffer.begin(), buffer.begin() + count);
                    }
                    else break;
                }
            } else {
                stopResolverProcess(process);
            }
            close(responsePipe[0]);
            if (cancelled()) return {ResolveStatus::Cancelled, {}};
            if (now() >= deadline) return {ResolveStatus::TimedOut, {}};
            return requestSent && responseConfigured && responseValid && exited
                   ? resolverResponse(response, port) : ResolveOutcome{};
        }
#endif

        ResolveOutcome resolveEndpoint(const SessionTransportDependencies &dependencies, const std::string &host,
                                       std::uint16_t port, TransportTimePoint deadline,
                                       const std::function<bool()> &cancelled) {
            if (dependencies.resolve) return dependencies.resolve(host, port, deadline, cancelled);
            auto now = [&dependencies] { return dependencyNow(dependencies); };
            return realResolve(host, port, deadline, cancelled, now);
        }

        class PendingSocket {
        public:
            void publish(SocketHandle socket) {
                std::lock_guard<std::mutex> lock(mutex);
                handle = socket;
            }

            void interrupt() {
                std::lock_guard<std::mutex> lock(mutex);
                shutdownSocket(handle);
            }

            void release(SocketHandle socket) {
                std::lock_guard<std::mutex> lock(mutex);
                if (handle == socket) handle = InvalidSocket;
            }

            void closeOwned(SocketHandle socket) {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (handle != socket) return;
                    handle = InvalidSocket;
                }
                closeSocket(socket);
            }

        private:
            std::mutex mutex;
            SocketHandle handle = InvalidSocket;
        };

        ConnectOutcome realConnect(const std::vector<ResolvedIpv4Endpoint> &addresses, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now,
                                   PendingSocket &pendingSocket) {
            TransportFailure lastFailure = TransportFailure::Unreachable;
            for (const auto &resolved: addresses) {
                if (cancelled()) return {ConnectStatus::Cancelled, -1};
                if (now() >= deadline) return {ConnectStatus::TimedOut, -1};
                sockaddr_in address = socketAddress(resolved);
                SocketHandle socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (socket == InvalidSocket) continue;
                if (!configureTransportSocket(socket)) {
                    closeSocket(socket);
                    return {ConnectStatus::Failed, -1};
                }
                pendingSocket.publish(socket);
                int result = ::connect(socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address));
                if (result != 0 && !connectPending(socketError())) {
                    lastFailure = connectFailure(socketError());
                    pendingSocket.closeOwned(socket);
                    continue;
                }
                while (result != 0 && !cancelled() && now() < deadline) {
                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now());
                    if (!waitSocket(socket, true, std::min(remaining, std::chrono::milliseconds(20)))) continue;
                    int error = 0;
#ifdef D6R_TRANSPORT_WINDOWS
                    int length = sizeof(error);
#else
                    socklen_t length = sizeof(error);
#endif
                    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length) != 0
                        || error != 0) {
                        lastFailure = connectFailure(error == 0 ? socketError() : error);
                        break;
                    }
                    result = 0;
                }
                if (result == 0 && !cancelled() && now() < deadline) {
                    pendingSocket.release(socket);
                    return {ConnectStatus::Connected, static_cast<std::intptr_t>(socket)};
                }
                pendingSocket.closeOwned(socket);
            }
            if (cancelled()) return {ConnectStatus::Cancelled, -1};
            if (now() >= deadline) return {ConnectStatus::TimedOut, -1};
            if (lastFailure == TransportFailure::ConnectionRefused)
                return {ConnectStatus::ConnectionRefused, -1};
            if (lastFailure == TransportFailure::Unreachable)
                return {ConnectStatus::Unreachable, -1};
            return {ConnectStatus::Failed, -1};
        }

        bool terminal(ClientState state) {
            return state == ClientState::Closed || state == ClientState::Failed
                   || state == ClientState::Cancelled || state == ClientState::TimedOut;
        }
    }

    class TcpConnection::Impl {
    public:
        explicit Impl(SocketHandle socket) : socket(socket), lastReceive(Clock::now()), lastLivenessPing(Clock::now()) {
            if (!configureConnectedSocket(socket)) {
                failure.store(TransportFailure::SystemError);
                state.store(ClientState::Failed);
                closeSocketOnce();
                return;
            }
            reader = std::thread([this] { readLoop(); });
            writer = std::thread([this] { writeLoop(); });
        }

        ~Impl() { close(); }

        SendResult send(std::vector<std::uint8_t> payload) {
            if (payload.size() > MaxPayloadBytes) return SendResult::PayloadTooLarge;
            std::lock_guard<std::mutex> lock(outputMutex);
            ClientState current = state.load();
            if (current == ClientState::Closing) return SendResult::Closing;
            if (current != ClientState::Connected) return SendResult::NotConnected;
            if (output.size() + activeTransportFrames >= MaxQueuedTransportFrames
                || outputBytes + activeApplicationBytes + payload.size() > MaxQueuedTransportPayloadBytes) {
                return SendResult::Backpressure;
            }
            outputBytes += payload.size();
            output.push_back({ApplicationFrame, std::move(payload)});
            outputChanged.notify_one();
            return SendResult::Accepted;
        }

        bool receive(TransportFrame &frame) {
            std::lock_guard<std::mutex> lock(inputMutex);
            if (input.empty()) return false;
            frame = std::move(input.front());
            inputBytes -= frame.payload.size();
            input.pop_front();
            return true;
        }

        void close() {
            requestClose();

            if (writer.joinable() && writer.get_id() != std::this_thread::get_id()) writer.join();
            stop.store(true);
            shutdownSocket(socket);
            if (reader.joinable() && reader.get_id() != std::this_thread::get_id()) reader.join();
            closeSocketOnce();
            ClientState current = state.load();
            if (current == ClientState::Closing || current == ClientState::Connected) state.store(ClientState::Closed);
        }

        void requestClose() {
            std::lock_guard<std::mutex> terminalLock(terminalMutex);
            ClientState expected = ClientState::Connected;
            if (state.compare_exchange_strong(expected, ClientState::Closing)) {
                closeDeadline = Clock::now() + GracefulCloseDeadline;
                closeRequested.store(true);
                outputChanged.notify_all();
            } else if (state.load() == ClientState::Closing) {
                closeRequested.store(true);
                outputChanged.notify_all();
            }
        }

        std::atomic<ClientState> state{ClientState::Connected};
        std::atomic<TransportFailure> failure{TransportFailure::None};

    private:
        struct PendingFrame {
            std::uint16_t kind;
            std::vector<std::uint8_t> payload;
        };

        SocketHandle socket;
        std::atomic<bool> socketClosed{false};
        std::atomic<bool> stop{false};
        std::atomic<bool> closeRequested{false};
        std::mutex terminalMutex;
        std::thread reader;
        std::thread writer;
        std::mutex inputMutex;
        std::deque<TransportFrame> input;
        std::size_t inputBytes = 0;
        std::mutex outputMutex;
        std::condition_variable outputChanged;
        std::deque<PendingFrame> output;
        std::size_t outputBytes = 0;
        std::size_t activeTransportFrames = 0;
        std::size_t activeApplicationBytes = 0;
        Clock::time_point closeDeadline = Clock::time_point::max();
        std::atomic<Clock::time_point> lastReceive;
        Clock::time_point lastLivenessPing;

        void closeSocketOnce() {
            if (!socketClosed.exchange(true)) closeSocket(socket);
        }

        void fail(TransportFailure reason, bool timedOut = false) {
            std::lock_guard<std::mutex> terminalLock(terminalMutex);
            ClientState current = state.load();
            if (terminal(current) || current == ClientState::Closing) return;
            failure.store(reason);
            state.store(timedOut ? ClientState::TimedOut : ClientState::Failed);
            stop.store(true);
            shutdownSocket(socket);
            outputChanged.notify_all();
        }

        void queueControl(std::uint16_t kind) {
            std::lock_guard<std::mutex> lock(outputMutex);
            if (!stop.load() && output.size() + activeTransportFrames < MaxQueuedTransportFrames) {
                output.push_back({kind, {}});
                outputChanged.notify_one();
            }
        }

        bool readExact(std::uint8_t *target, std::size_t size) {
            std::size_t offset = 0;
            Clock::time_point progress = Clock::now();
            while (offset < size && !stop.load()) {
                if (!waitSocket(socket, false, std::chrono::milliseconds(100))) {
                    auto now = Clock::now();
                    if (now - lastReceive.load() >= ReceiveIdleDeadline) {
                        fail(TransportFailure::IdleTimedOut, true);
                        return false;
                    }
                    if (now - lastReceive.load() >= LivenessInterval
                        && now - lastLivenessPing >= LivenessInterval) {
                        queueControl(LivenessPing);
                        lastLivenessPing = now;
                    }
                    if (offset > 0 && now - progress >= ProgressDeadline) {
                        fail(TransportFailure::InboundStalled, true);
                        return false;
                    }
                    continue;
                }
#ifdef D6R_TRANSPORT_WINDOWS
                int count = recv(socket, reinterpret_cast<char *>(target + offset), static_cast<int>(size - offset), 0);
#else
                ssize_t count = recv(socket, target + offset, size - offset, 0);
#endif
                if (count > 0) {
                    offset += static_cast<std::size_t>(count);
                    progress = Clock::now();
                    lastReceive.store(progress);
                } else if (count == 0) {
                    fail(TransportFailure::PeerClosed);
                    return false;
                } else {
                    int error = socketError();
                    if (!wouldBlock(error) && !interrupted(error)) {
                        fail(TransportFailure::SystemError);
                        return false;
                    }
                }
            }
            return offset == size;
        }

        void readLoop() {
            while (!stop.load() && state.load() == ClientState::Connected) {
                std::array<std::uint8_t, TransportEnvelopeBytes> header{};
                if (!readExact(header.data(), header.size())) break;
                std::uint32_t identifier = readU32(header.data());
                std::uint16_t version = readU16(header.data() + 4);
                std::uint16_t kind = readU16(header.data() + 6);
                std::uint32_t payloadSize = readU32(header.data() + 8);
                if (identifier != TransportFramingIdentifier || version != TransportFramingVersion
                    || kind > LivenessPong || payloadSize > MaxPayloadBytes
                    || (kind != ApplicationFrame && payloadSize != 0)) {
                    fail(TransportFailure::ProtocolViolation);
                    break;
                }
                std::vector<std::uint8_t> payload(payloadSize);
                if (payloadSize > 0 && !readExact(payload.data(), payload.size())) break;
                if (kind == LivenessPing) {
                    queueControl(LivenessPong);
                    continue;
                }
                if (kind == LivenessPong) continue;

                std::unique_lock<std::mutex> lock(inputMutex);
                const auto blockedSince = Clock::now();
                while ((input.size() >= MaxQueuedTransportFrames
                        || inputBytes + payload.size() > MaxQueuedTransportPayloadBytes) && !stop.load()) {
                    if (Clock::now() - blockedSince >= ProgressDeadline) {
                        lock.unlock();
                        fail(TransportFailure::InboundStalled, true);
                        return;
                    }
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    lock.lock();
                }
                if (stop.load()) break;
                inputBytes += payload.size();
                input.push_back({std::move(payload)});
            }
        }

        bool writeFrame(const PendingFrame &frame) {
            std::array<std::uint8_t, TransportEnvelopeBytes> header{};
            writeU32(header.data(), TransportFramingIdentifier);
            writeU16(header.data() + 4, TransportFramingVersion);
            writeU16(header.data() + 6, frame.kind);
            writeU32(header.data() + 8, static_cast<std::uint32_t>(frame.payload.size()));
            std::size_t total = header.size() + frame.payload.size();
            std::size_t offset = 0;
            Clock::time_point progress = Clock::now();
            while (offset < total && !stop.load()) {
                if (closeRequested.load() && Clock::now() >= closeDeadline) return false;
                if (!waitSocket(socket, true, std::chrono::milliseconds(100))) {
                    if (Clock::now() - progress >= ProgressDeadline) {
                        fail(TransportFailure::OutboundStalled, true);
                        return false;
                    }
                    continue;
                }
                const std::uint8_t *data = offset < header.size()
                                           ? header.data() + offset
                                           : frame.payload.data() + (offset - header.size());
                std::size_t remaining = offset < header.size() ? header.size() - offset : total - offset;
#ifdef D6R_TRANSPORT_WINDOWS
                int count = ::send(socket, reinterpret_cast<const char *>(data), static_cast<int>(remaining), 0);
#else
                ssize_t count = ::send(socket, data, remaining, MSG_NOSIGNAL);
#endif
                if (count > 0) {
                    offset += static_cast<std::size_t>(count);
                    progress = Clock::now();
                } else if (count < 0) {
                    int error = socketError();
                    if (!wouldBlock(error) && !interrupted(error)) {
                        fail(TransportFailure::SystemError);
                        return false;
                    }
                }
            }
            return offset == total;
        }

        void writeLoop() {
            while (!stop.load()) {
                PendingFrame frame;
                {
                    std::unique_lock<std::mutex> lock(outputMutex);
                    outputChanged.wait_for(lock, std::chrono::milliseconds(100), [this] {
                        return !output.empty() || closeRequested.load() || stop.load();
                    });
                    if (stop.load()) break;
                    if (output.empty()) {
                        if (closeRequested.load()) break;
                        continue;
                    }
                    frame = std::move(output.front());
                    output.pop_front();
                    outputBytes -= frame.payload.size();
                    activeTransportFrames = 1;
                    if (frame.kind == ApplicationFrame) {
                        activeApplicationBytes = frame.payload.size();
                    }
                }
                bool written = writeFrame(frame);
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    activeTransportFrames = 0;
                    activeApplicationBytes = 0;
                }
                if (!written) break;
            }
            if (closeRequested.load()) {
                shutdownSocket(socket);
                if (state.load() == ClientState::Closing) state.store(ClientState::Closed);
            }
        }
    };

    TcpConnection::TcpConnection(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}
    TcpConnection::~TcpConnection() = default;
    SendResult TcpConnection::send(std::vector<std::uint8_t> payload) { return impl->send(std::move(payload)); }
    bool TcpConnection::receive(TransportFrame &frame) { return impl->receive(frame); }
    ClientState TcpConnection::state() const { return impl->state.load(); }
    TransportFailure TcpConnection::failure() const { return impl->failure.load(); }
    void TcpConnection::requestClose() { impl->requestClose(); }
    void TcpConnection::close() { impl->close(); }

    class TcpClient::Impl {
    public:
        explicit Impl(SessionTransportDependencies dependencies) : dependencies(std::move(dependencies)) {}

        ~Impl() {
            close();
            cancel();
        }

        bool start(const Endpoint &value) {
            ClientState expected = ClientState::NotStarted;
            if (!state.compare_exchange_strong(expected, ClientState::Resolving)) return false;
            endpoint = value;
            worker = std::thread([this] { connectLoop(); });
            return true;
        }

        void cancel() {
            cancelled.store(true);
            ClientState current = state.load();
            while (current == ClientState::Resolving || current == ClientState::Connecting
                   || current == ClientState::Connected) {
                if (state.compare_exchange_weak(current, ClientState::Cancelled)) break;
            }
            pendingSocket.interrupt();
            changed.notify_all();
            if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) worker.join();
            std::shared_ptr<TcpConnection> active;
            {
                std::lock_guard<std::mutex> lock(mutex);
                active = connection;
            }
            if (state.load() == ClientState::Cancelled && active) active->close();
        }

        void close() {
            std::shared_ptr<TcpConnection> active;
            {
                std::lock_guard<std::mutex> lock(mutex);
                active = connection;
            }
            if (!active) return;
            ClientState activeState = active->state();
            if (activeState == ClientState::Failed || activeState == ClientState::TimedOut
                || activeState == ClientState::Closed) {
                state.store(activeState);
                active->close();
                changed.notify_all();
                return;
            }
            ClientState expected = ClientState::Connected;
            state.compare_exchange_strong(expected, ClientState::Closing);
            active->close();
            if (state.load() == ClientState::Closing) state.store(ClientState::Closed);
            changed.notify_all();
        }

        void connectLoop() {
            const auto deadline = dependencyNow(dependencies) + StartupDeadline;
            if (!socketRuntime().ready() || endpoint.host.empty() || endpoint.host.find('\0') != std::string::npos
                || endpoint.host.size() > MaxProtocolStringBytes || endpoint.port == 0) {
                finishFailure(TransportFailure::InvalidEndpoint);
                return;
            }
            const auto isCancelled = [this] { return cancelled.load(); };
            ResolveOutcome resolution = resolveEndpoint(dependencies, endpoint.host, endpoint.port, deadline, isCancelled);
            if (cancelled.load() || resolution.status == ResolveStatus::Cancelled) return;
            if (resolution.status == ResolveStatus::TimedOut || dependencyNow(dependencies) >= deadline) {
                finishTimeout();
                return;
            }
            if (resolution.status != ResolveStatus::Resolved || resolution.endpoints.empty()) {
                finishFailure(TransportFailure::ResolveFailed);
                return;
            }
            ClientState expectedResolving = ClientState::Resolving;
            if (!state.compare_exchange_strong(expectedResolving, ClientState::Connecting)) return;
            changed.notify_all();
            ConnectOutcome outcome;
            if (dependencies.connect) outcome = dependencies.connect(resolution.endpoints, deadline, isCancelled);
            else {
                auto now = [this] { return dependencyNow(dependencies); };
                outcome = realConnect(resolution.endpoints, deadline, isCancelled, now, pendingSocket);
            }
            if (cancelled.load() || outcome.status == ConnectStatus::Cancelled) {
                if (outcome.nativeSocket != -1) closeSocket(static_cast<SocketHandle>(outcome.nativeSocket));
                return;
            }
            if (outcome.status == ConnectStatus::Connected && outcome.nativeSocket != -1
                && dependencyNow(dependencies) < deadline) {
                auto active = std::shared_ptr<TcpConnection>(new TcpConnection(
                        std::make_unique<TcpConnection::Impl>(static_cast<SocketHandle>(outcome.nativeSocket))));
                if (active->state() != ClientState::Connected) {
                    active->close();
                    finishFailure(TransportFailure::SystemError);
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    connection = active;
                }
                ClientState expected = ClientState::Connecting;
                if (state.compare_exchange_strong(expected, ClientState::Connected)) {
                    changed.notify_all();
                    return;
                }
                active->close();
                return;
            }
            if (outcome.nativeSocket != -1) closeSocket(static_cast<SocketHandle>(outcome.nativeSocket));
            if (outcome.status == ConnectStatus::TimedOut || dependencyNow(dependencies) >= deadline) finishTimeout();
            else if (outcome.status == ConnectStatus::ConnectionRefused)
                finishFailure(TransportFailure::ConnectionRefused);
            else if (outcome.status == ConnectStatus::Unreachable)
                finishFailure(TransportFailure::Unreachable);
            else finishFailure(TransportFailure::SystemError);
        }

        void finishFailure(TransportFailure reason) {
            ClientState current = state.load();
            while (current == ClientState::Resolving || current == ClientState::Connecting) {
                failure.store(reason);
                if (state.compare_exchange_weak(current, ClientState::Failed)) {
                    changed.notify_all();
                    return;
                }
                failure.store(TransportFailure::None);
            }
        }

        void finishTimeout() {
            ClientState current = state.load();
            while (current == ClientState::Resolving || current == ClientState::Connecting) {
                if (state.compare_exchange_weak(current, ClientState::TimedOut)) {
                    changed.notify_all();
                    return;
                }
            }
        }

        Endpoint endpoint;
        SessionTransportDependencies dependencies;
        std::atomic<ClientState> state{ClientState::NotStarted};
        std::atomic<TransportFailure> failure{TransportFailure::None};
        std::atomic<bool> cancelled{false};
        PendingSocket pendingSocket;
        mutable std::mutex mutex;
        std::condition_variable changed;
        std::shared_ptr<TcpConnection> connection;
        std::thread worker;
    };

    TcpClient::TcpClient() : TcpClient(SessionTransportDependencies{}) {}
    TcpClient::TcpClient(SessionTransportDependencies dependencies)
            : impl(std::make_unique<Impl>(std::move(dependencies))) {}
    TcpClient::~TcpClient() = default;
    bool TcpClient::start(const Endpoint &endpoint) { return impl->start(endpoint); }
    void TcpClient::cancel() { impl->cancel(); }
    void TcpClient::close() { impl->close(); }
    ClientState TcpClient::state() const {
        ClientState current = impl->state.load();
        if (current != ClientState::Connected) return current;
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->connection) return current;
        ClientState connectionState = impl->connection->state();
        if (connectionState == ClientState::Closing || connectionState == ClientState::Closed
            || connectionState == ClientState::Failed || connectionState == ClientState::TimedOut) {
            return connectionState;
        }
        return current;
    }
    TransportFailure TcpClient::failure() const {
        if (impl->state.load() == ClientState::Failed) return impl->failure.load();
        std::lock_guard<std::mutex> lock(impl->mutex);
        return impl->connection ? impl->connection->failure() : TransportFailure::None;
    }
    std::shared_ptr<TcpConnection> TcpClient::connection() const {
        std::lock_guard<std::mutex> lock(impl->mutex);
        return impl->connection;
    }
    bool TcpClient::waitForConnected(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(impl->mutex);
        impl->changed.wait_for(lock, timeout, [this] {
            ClientState current = impl->state.load();
            return current == ClientState::Connected || terminal(current);
        });
        return impl->state.load() == ClientState::Connected;
    }

    class TcpListener::Impl {
    public:
        Impl(std::size_t maxConnections, SessionTransportDependencies dependencies)
                : maxConnections(std::min(maxConnections, MaxTransportConnections)),
                  dependencies(std::move(dependencies)) {}
        ~Impl() { shutdown(); }

        bool start(const Endpoint &value) {
            ListenerState expected = ListenerState::NotStarted;
            if (!state.compare_exchange_strong(expected, ListenerState::Starting)) return false;
            endpoint = value;
            worker = std::thread([this] { listenLoop(); });
            return true;
        }

        void cancel() {
            cancelled.store(true);
            ListenerState current = state.load();
            while (current == ListenerState::Starting || current == ListenerState::Ready) {
                if (state.compare_exchange_weak(current, ListenerState::Cancelled)) break;
            }
            stop.store(true);
            closeListener();
            changed.notify_all();
            if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) worker.join();
        }

        void shutdown() {
            ListenerState current = state.load();
            while (current == ListenerState::Starting || current == ListenerState::Ready) {
                if (state.compare_exchange_weak(current, ListenerState::Stopping)) break;
            }
            if (current == ListenerState::Starting) cancelled.store(true);
            stop.store(true);
            closeListener();
            if (worker.joinable()) worker.join();
            std::vector<std::shared_ptr<TcpConnection>> snapshot;
            {
                std::lock_guard<std::mutex> lock(mutex);
                snapshot.assign(connections.begin(), connections.end());
                pending.clear();
            }
            for (const auto &connection: snapshot) connection->requestClose();
            for (const auto &connection: snapshot) connection->close();
            {
                std::lock_guard<std::mutex> lock(mutex);
                connections.clear();
            }
            current = state.load();
            if (current == ListenerState::Stopping || current == ListenerState::Ready
                || current == ListenerState::Starting) state.store(ListenerState::Stopped);
            changed.notify_all();
        }

        void listenLoop() {
            const auto deadline = dependencyNow(dependencies) + StartupDeadline;
            if (!socketRuntime().ready() || endpoint.host.empty() || endpoint.host.find('\0') != std::string::npos
                || endpoint.host.size() > MaxProtocolStringBytes || endpoint.port == 0 || maxConnections == 0) {
                fail(TransportFailure::InvalidEndpoint);
                return;
            }
            const auto isCancelled = [this] { return cancelled.load(); };
            ResolveOutcome resolution = resolveEndpoint(dependencies, endpoint.host, endpoint.port, deadline, isCancelled);
            if (cancelled.load() || resolution.status == ResolveStatus::Cancelled) return;
            if (resolution.status == ResolveStatus::TimedOut || dependencyNow(dependencies) >= deadline) {
                timeout();
                return;
            }
            if (resolution.status != ResolveStatus::Resolved || resolution.endpoints.empty()) {
                fail(TransportFailure::ResolveFailed);
                return;
            }
            SocketHandle bound = InvalidSocket;
            for (const auto &resolved: resolution.endpoints) {
                sockaddr_in address = socketAddress(resolved);
                bound = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (bound == InvalidSocket) continue;
                int enabled = 1;
                setsockopt(bound, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
                if (!configureTransportSocket(bound)) {
                    closeSocket(bound);
                    fail(TransportFailure::SystemError);
                    return;
                }
                if (::bind(bound, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0
                    && ::listen(bound, static_cast<int>(maxConnections)) == 0) break;
                closeSocket(bound);
                bound = InvalidSocket;
            }
            if (cancelled.load()) { closeSocket(bound); return; }
            if (dependencyNow(dependencies) >= deadline) { closeSocket(bound); timeout(); return; }
            if (bound == InvalidSocket) { fail(TransportFailure::BindFailed); return; }
            listener.publish(bound);
            ListenerState expected = ListenerState::Starting;
            if (!state.compare_exchange_strong(expected, ListenerState::Ready)) {
                listener.closeOwned(bound);
                return;
            }
            changed.notify_all();

            while (!stop.load()) {
                if (!waitSocket(bound, false, std::chrono::milliseconds(100))) {
                    reapClosed();
                    continue;
                }
                sockaddr_in peer{};
#ifdef D6R_TRANSPORT_WINDOWS
                int length = sizeof(peer);
#else
                socklen_t length = sizeof(peer);
#endif
                SocketHandle accepted = ::accept(bound, reinterpret_cast<sockaddr *>(&peer), &length);
                if (accepted == InvalidSocket) continue;
                if (!configureTransportSocket(accepted)) {
                    closeSocket(accepted);
                    continue;
                }
                std::lock_guard<std::mutex> lock(mutex);
                reapClosedLocked();
                if (connections.size() >= maxConnections) {
                    closeSocket(accepted);
                    continue;
                }
                auto connection = std::shared_ptr<TcpConnection>(new TcpConnection(std::make_unique<TcpConnection::Impl>(accepted)));
                connections.push_back(connection);
                pending.push_back(connection);
            }
            listener.closeOwned(bound);
        }

        void reapClosed() {
            std::lock_guard<std::mutex> lock(mutex);
            reapClosedLocked();
        }

        void reapClosedLocked() {
            connections.erase(std::remove_if(connections.begin(), connections.end(), [](const auto &connection) {
                return terminal(connection->state());
            }), connections.end());
        }

        void closeListener() {
            listener.interrupt();
        }

        void fail(TransportFailure reason) {
            ListenerState expected = ListenerState::Starting;
            failure.store(reason);
            if (!state.compare_exchange_strong(expected, ListenerState::Failed))
                failure.store(TransportFailure::None);
            changed.notify_all();
        }

        void timeout() {
            ListenerState expected = ListenerState::Starting;
            state.compare_exchange_strong(expected, ListenerState::TimedOut);
            changed.notify_all();
        }

        Endpoint endpoint;
        const std::size_t maxConnections;
        SessionTransportDependencies dependencies;
        std::atomic<ListenerState> state{ListenerState::NotStarted};
        std::atomic<TransportFailure> failure{TransportFailure::None};
        std::atomic<bool> stop{false};
        std::atomic<bool> cancelled{false};
        PendingSocket listener;
        mutable std::mutex mutex;
        std::condition_variable changed;
        std::deque<std::shared_ptr<TcpConnection>> pending;
        std::vector<std::shared_ptr<TcpConnection>> connections;
        std::thread worker;
    };

    TcpListener::TcpListener(std::size_t maxConnections)
            : TcpListener(maxConnections, SessionTransportDependencies{}) {}
    TcpListener::TcpListener(std::size_t maxConnections, SessionTransportDependencies dependencies)
            : impl(std::make_unique<Impl>(maxConnections, std::move(dependencies))) {}
    TcpListener::~TcpListener() = default;
    bool TcpListener::start(const Endpoint &endpoint) { return impl->start(endpoint); }
    void TcpListener::cancel() { impl->cancel(); }
    void TcpListener::shutdown() { impl->shutdown(); }
    ListenerState TcpListener::state() const { return impl->state.load(); }
    TransportFailure TcpListener::failure() const {
        return impl->state.load() == ListenerState::Failed
               ? impl->failure.load() : TransportFailure::None;
    }
    std::shared_ptr<TcpConnection> TcpListener::acceptConnection() {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->pending.empty()) return {};
        auto connection = impl->pending.front();
        impl->pending.pop_front();
        return connection;
    }
    bool TcpListener::waitForReady(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(impl->mutex);
        impl->changed.wait_for(lock, timeout, [this] {
            ListenerState current = impl->state.load();
            return current == ListenerState::Ready || current == ListenerState::Stopped
                   || current == ListenerState::Failed || current == ListenerState::Cancelled
                   || current == ListenerState::TimedOut;
        });
        return impl->state.load() == ListenerState::Ready;
    }
}
