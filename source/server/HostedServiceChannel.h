#ifndef DUEL6_SERVER_HOSTEDSERVICECHANNEL_H
#define DUEL6_SERVER_HOSTEDSERVICECHANNEL_H

#include <memory>

#include "../network/HostServiceControlProtocol.h"

namespace Duel6::Server {
    class HostedServiceChannel {
    public:
        static std::shared_ptr<HostedServiceChannel> fromCommandLine(int argumentCount, char **arguments);

        ~HostedServiceChannel();
        bool active() const noexcept;
        bool send(Network::HostServiceStatusCode status) noexcept;
        bool stopRequested() noexcept;

    private:
        HostedServiceChannel();

#ifdef D6R_TRANSPORT_WINDOWS
        void *statusHandle = nullptr;
        void *controlHandle = nullptr;
#else
        int statusDescriptor = -1;
        int controlDescriptor = -1;
#endif
        bool stopped = false;
    };
}

#endif
