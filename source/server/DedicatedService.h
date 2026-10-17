#ifndef DUEL6_SERVER_DEDICATEDSERVICE_H
#define DUEL6_SERVER_DEDICATEDSERVICE_H
#include <memory>
#include <string>
#include "../network/SecureSession.h"

namespace Duel6::Server {
    std::shared_ptr<const Network::SessionPassword> loadPassword(const std::string &path);
    class DedicatedReadiness final {
    public:
        explicit DedicatedReadiness(const std::string &path);
        ~DedicatedReadiness();
        DedicatedReadiness(const DedicatedReadiness &) = delete;
        DedicatedReadiness &operator=(const DedicatedReadiness &) = delete;
        void poll(bool ready);
        static bool check(const std::string &path);
    private:
        std::string path;
        int socket = -1;
    };
}
#endif
