#include "source/platform/DarwinProcess.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const std::string owner = argv[2];
    const bool leaf = owner.rfind("--resolver-owner=", 0) == 0;
    if (!leaf && owner.rfind("--guardian-parent=", 0) != 0) return 2;
    auto monitor = Duel6::Platform::Darwin::ParentMonitor::start(
        static_cast<pid_t>(std::stol(owner.substr(leaf ? 17 : 18))), 3, !leaf);
    if (!monitor) return 3;
    const int identity[] = {getpid(), getpgrp()};
    if (send(4, identity, sizeof(identity), 0) != sizeof(identity)) return 4;
    // Observable entry precedes the deliberately blocked resolver operation.
    // No production fault flag or detached lookup thread is involved.
    if (std::string(argv[1]) == "exit") return 0;
    char command = 0;
    return recv(5, &command, 1, 0) == 1 ? 0 : 5;
}
