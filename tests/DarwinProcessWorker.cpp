#include "source/platform/DarwinProcess.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <libproc.h>
#include <netdb.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <unistd.h>

extern char **environ;

namespace {
    bool identity(pid_t pid, std::ostream &out) {
        proc_bsdinfo value{};
        if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &value, sizeof(value)) != sizeof(value)) return false;
        out << value.pbi_pid << ' ' << value.pbi_start_tvsec << ' ' << value.pbi_start_tvusec
            << ' ' << value.pbi_pgid << ' ' << value.pbi_status << '\n';
        return true;
    }
}

int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--identity") {
        proc_bsdinfo value{};
        errno = 0;
        const int count = proc_pidinfo(std::atoi(argv[2]), PROC_PIDTBSDINFO, 0, &value, sizeof(value));
        if (count == sizeof(value)) {
            std::printf("%u %llu %llu %u %u\n", value.pbi_pid,
                static_cast<unsigned long long>(value.pbi_start_tvsec),
                static_cast<unsigned long long>(value.pbi_start_tvusec), value.pbi_pgid, value.pbi_status);
            return 0;
        }
        if (count == 0 && errno == ESRCH) { std::puts("absent"); return 0; }
        return 2; // An inspection failure is not absence.
    }
    if (argc == 2 && std::string(argv[1]) == "--descendant") {
        for (;;) pause(); // Deliberately does not cooperate with the service.
    }
    if (argc != 4 || std::string(argv[3]).rfind("--guardian-parent=", 0) != 0) return 2;
    // Verify sanitized inheritance before the monitor itself creates an FD.
    if (environ && environ[0]) return 3;
    for (int fd = 4; fd < 1024; ++fd) if (fcntl(fd, F_GETFD) >= 0) return 4;
    const pid_t parent = static_cast<pid_t>(std::stol(std::string(argv[3]).substr(18)));
    auto monitor = Duel6::Platform::Darwin::ParentMonitor::start(parent, 3);
    if (!monitor) return 5;
    const std::string mode = argv[2];
    if (mode == "resolve") {
        addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        addrinfo *result = nullptr;
        if (getaddrinfo("localhost", "26660", &hints, &result) != 0 || !result) return 6;
        freeaddrinfo(result);
        std::ofstream done(argv[1]);
        done << "resolved\n";
        return done ? 0 : 7;
    }
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (listener < 0 || bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
        || listen(listener, 1) != 0) return 8;
    socklen_t size = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &size) != 0) return 9;
    pid_t descendant = -1;
    posix_spawnattr_t attributes;
    if (posix_spawnattr_init(&attributes) != 0) return 10;
    int error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
    char *childArgs[] = {argv[0], const_cast<char *>("--descendant"), nullptr};
    char *environment[] = {nullptr};
    if (!error) error = posix_spawn(&descendant, argv[0], nullptr, &attributes, childArgs, environment);
    posix_spawnattr_destroy(&attributes);
    if (error) return 11;
    {
        std::ofstream marker(argv[1]);
        if (!identity(getpid(), marker) || !identity(descendant, marker)) return 12;
        marker << ntohs(address.sin_port) << '\n';
        if (!marker) return 13;
    }
    // All waiting here is outside the independent liveness monitor. The
    // blocking mode also represents a resolver worker stuck inside libc DNS.
    if (mode == "stopped") {
        kill(descendant, SIGSTOP);
        raise(SIGSTOP);
    }
    for (;;) pause();
}
