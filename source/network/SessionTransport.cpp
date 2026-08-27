#include "SessionTransport.h"

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
#if defined(__MINGW32__) && !defined(GetAddrInfoExCancel)
extern "C" INT WSAAPI GetAddrInfoExCancel(LPHANDLE cancellationHandle);
extern "C" INT WSAAPI GetAddrInfoExOverlappedResult(LPOVERLAPPED operation);
#endif
using SocketHandle = SOCKET;
static constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
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

        void configureConnectedSocket(SocketHandle socket) {
            setNonBlocking(socket);
            int enabled = 1;
            setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
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

        ResolvedIpv4Endpoint resolvedEndpoint(const sockaddr_in &address, std::uint16_t port) {
            ResolvedIpv4Endpoint endpoint;
            std::memcpy(endpoint.address.data(), &address.sin_addr.s_addr, endpoint.address.size());
            endpoint.port = port;
            return endpoint;
        }

#ifdef D6R_TRANSPORT_WINDOWS
        ResolveOutcome realResolve(const std::string &host, std::uint16_t port, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now) {
            ADDRINFOEXA hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            PADDRINFOEXA addresses = nullptr;
            OVERLAPPED operation{};
            operation.hEvent = WSACreateEvent();
            if (operation.hEvent == WSA_INVALID_EVENT) return {};
            HANDLE cancellationHandle = nullptr;
            std::string service = std::to_string(port);
            int result = GetAddrInfoExA(host.c_str(), service.c_str(), NS_ALL, nullptr, &hints, &addresses,
                                        nullptr, &operation, nullptr, &cancellationHandle);
            if (result == WSA_IO_PENDING) {
                while (!cancelled() && now() < deadline) {
                    DWORD wait = WSAWaitForMultipleEvents(1, &operation.hEvent, TRUE, 20, FALSE);
                    if (wait == WSA_WAIT_EVENT_0) break;
                    if (wait == WSA_WAIT_FAILED) {
                        GetAddrInfoExCancel(&cancellationHandle);
                        WSACloseEvent(operation.hEvent);
                        return {};
                    }
                }
                if (cancelled() || now() >= deadline) {
                    GetAddrInfoExCancel(&cancellationHandle);
                    WSAWaitForMultipleEvents(1, &operation.hEvent, TRUE, 500, FALSE);
                    if (addresses) FreeAddrInfoExA(addresses);
                    WSACloseEvent(operation.hEvent);
                    return {cancelled() ? ResolveStatus::Cancelled : ResolveStatus::TimedOut, {}};
                }
                result = GetAddrInfoExOverlappedResult(&operation);
            }

            ResolveOutcome outcome;
            if (cancelled()) outcome.status = ResolveStatus::Cancelled;
            else if (now() >= deadline) outcome.status = ResolveStatus::TimedOut;
            else if (result == 0 && addresses != nullptr) {
                outcome.status = ResolveStatus::Resolved;
                for (PADDRINFOEXA address = addresses; address != nullptr; address = address->ai_next) {
                    if (address->ai_family == AF_INET && address->ai_addrlen >= sizeof(sockaddr_in)) {
                        outcome.endpoints.push_back(resolvedEndpoint(
                                *reinterpret_cast<const sockaddr_in *>(address->ai_addr), port));
                    }
                }
                if (outcome.endpoints.empty()) outcome.status = ResolveStatus::Failed;
            }
            if (addresses) FreeAddrInfoExA(addresses);
            WSACloseEvent(operation.hEvent);
            return outcome;
        }
#else
        struct ResolverWireResult {
            std::int32_t status = -1;
            std::uint32_t count = 0;
            std::array<std::uint32_t, 64> addresses{};
        };

        void stopResolverProcess(pid_t process) {
            if (process <= 0) return;
            kill(process, SIGKILL);
            while (waitpid(process, nullptr, 0) < 0 && errno == EINTR) {}
        }

        ResolveOutcome realResolve(const std::string &host, std::uint16_t port, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now) {
            int channel[2];
            if (pipe(channel) != 0) return {};
            setNonBlocking(channel[0]);
            pid_t process = fork();
            if (process < 0) {
                close(channel[0]);
                close(channel[1]);
                return {};
            }
            if (process == 0) {
                close(channel[0]);
                ResolverWireResult wire;
                addrinfo hints{};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;
                addrinfo *addresses = nullptr;
                int result = getaddrinfo(host.c_str(), nullptr, &hints, &addresses);
                wire.status = result;
                if (result == 0) {
                    for (addrinfo *address = addresses; address != nullptr
                         && wire.count < wire.addresses.size(); address = address->ai_next) {
                        if (address->ai_family == AF_INET && address->ai_addrlen >= sizeof(sockaddr_in)) {
                            wire.addresses[wire.count++] =
                                    reinterpret_cast<const sockaddr_in *>(address->ai_addr)->sin_addr.s_addr;
                        }
                    }
                }
                if (addresses) freeaddrinfo(addresses);
                const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(&wire);
                std::size_t remaining = sizeof(wire);
                while (remaining > 0) {
                    ssize_t count = write(channel[1], data, remaining);
                    if (count > 0) {
                        data += count;
                        remaining -= static_cast<std::size_t>(count);
                    } else if (count < 0 && errno == EINTR) {
                        continue;
                    } else {
                        break;
                    }
                }
                close(channel[1]);
                _exit(0);
            }

            close(channel[1]);
            ResolverWireResult wire;
            std::uint8_t *target = reinterpret_cast<std::uint8_t *>(&wire);
            std::size_t received = 0;
            while (!cancelled() && now() < deadline && received < sizeof(wire)) {
                if (!waitSocket(channel[0], false, std::chrono::milliseconds(20))) continue;
                ssize_t count = read(channel[0], target + received, sizeof(wire) - received);
                if (count > 0) received += static_cast<std::size_t>(count);
                else if (count == 0) break;
                else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
            }
            close(channel[0]);
            if (cancelled() || now() >= deadline || received != sizeof(wire)) {
                stopResolverProcess(process);
                if (cancelled()) return {ResolveStatus::Cancelled, {}};
                if (now() >= deadline) return {ResolveStatus::TimedOut, {}};
                return {};
            }
            while (waitpid(process, nullptr, 0) < 0 && errno == EINTR) {}
            if (wire.status != 0 || wire.count == 0) return {};
            ResolveOutcome outcome;
            outcome.status = ResolveStatus::Resolved;
            for (std::uint32_t index = 0; index < wire.count; ++index) {
                sockaddr_in address{};
                address.sin_addr.s_addr = wire.addresses[index];
                outcome.endpoints.push_back(resolvedEndpoint(address, port));
            }
            return outcome;
        }
#endif

        ResolveOutcome resolveEndpoint(const SessionTransportDependencies &dependencies, const std::string &host,
                                       std::uint16_t port, TransportTimePoint deadline,
                                       const std::function<bool()> &cancelled) {
            if (dependencies.resolve) return dependencies.resolve(host, port, deadline, cancelled);
            auto now = [&dependencies] { return dependencyNow(dependencies); };
            return realResolve(host, port, deadline, cancelled, now);
        }

        ConnectOutcome realConnect(const std::vector<ResolvedIpv4Endpoint> &addresses, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled,
                                   const std::function<TransportTimePoint()> &now,
                                   std::atomic<SocketHandle> &pendingSocket) {
            TransportFailure lastFailure = TransportFailure::Unreachable;
            for (const auto &resolved: addresses) {
                if (cancelled()) return {ConnectStatus::Cancelled, -1};
                if (now() >= deadline) return {ConnectStatus::TimedOut, -1};
                sockaddr_in address = socketAddress(resolved);
                SocketHandle socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (socket == InvalidSocket) continue;
                pendingSocket.store(socket);
                setNonBlocking(socket);
                int result = ::connect(socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address));
                if (result != 0 && !connectPending(socketError())) {
                    lastFailure = connectFailure(socketError());
                    closeSocket(socket);
                    pendingSocket.store(InvalidSocket);
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
                    pendingSocket.store(InvalidSocket);
                    return {ConnectStatus::Connected, static_cast<std::intptr_t>(socket)};
                }
                closeSocket(socket);
                pendingSocket.store(InvalidSocket);
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
        explicit Impl(SocketHandle socket) : socket(socket), lastReceive(Clock::now()), lastSend(Clock::now()) {
            configureConnectedSocket(socket);
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
            inputProgress = Clock::now();
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
        std::thread reader;
        std::thread writer;
        std::mutex inputMutex;
        std::deque<TransportFrame> input;
        std::size_t inputBytes = 0;
        Clock::time_point inputProgress = Clock::now();
        std::mutex outputMutex;
        std::condition_variable outputChanged;
        std::deque<PendingFrame> output;
        std::size_t outputBytes = 0;
        std::size_t activeTransportFrames = 0;
        std::size_t activeApplicationBytes = 0;
        Clock::time_point closeDeadline = Clock::time_point::max();
        std::atomic<Clock::time_point> lastReceive;
        std::atomic<Clock::time_point> lastSend;

        void closeSocketOnce() {
            if (!socketClosed.exchange(true)) closeSocket(socket);
        }

        void fail(TransportFailure reason, bool timedOut = false) {
            ClientState current = state.load();
            while (!terminal(current) && current != ClientState::Closing) {
                ClientState target = timedOut ? ClientState::TimedOut : ClientState::Failed;
                if (state.compare_exchange_weak(current, target)) {
                    failure.store(reason);
                    stop.store(true);
                    shutdownSocket(socket);
                    outputChanged.notify_all();
                    return;
                }
            }
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
                        && now - lastSend.load() >= LivenessInterval) queueControl(LivenessPing);
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
                while ((input.size() >= MaxQueuedTransportFrames
                        || inputBytes + payload.size() > MaxQueuedTransportPayloadBytes) && !stop.load()) {
                    if (Clock::now() - inputProgress >= ProgressDeadline) {
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
                    lastSend.store(progress);
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
            SocketHandle currentSocket = pendingSocket.load();
            shutdownSocket(currentSocket);
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
            if (!socketRuntime().ready() || endpoint.host.empty() || endpoint.port == 0) {
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
        std::atomic<SocketHandle> pendingSocket{InvalidSocket};
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
            if (!socketRuntime().ready() || endpoint.host.empty() || endpoint.port == 0 || maxConnections == 0) {
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
                if (::bind(bound, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0
                    && ::listen(bound, static_cast<int>(maxConnections)) == 0 && setNonBlocking(bound)) break;
                closeSocket(bound);
                bound = InvalidSocket;
            }
            if (cancelled.load()) { closeSocket(bound); return; }
            if (dependencyNow(dependencies) >= deadline) { closeSocket(bound); timeout(); return; }
            if (bound == InvalidSocket) { fail(TransportFailure::BindFailed); return; }
            listener.store(bound);
            ListenerState expected = ListenerState::Starting;
            if (!state.compare_exchange_strong(expected, ListenerState::Ready)) {
                closeListener();
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
            SocketHandle value = listener.exchange(InvalidSocket);
            shutdownSocket(value);
            closeSocket(value);
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
        std::atomic<SocketHandle> listener{InvalidSocket};
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
