#include "tests/TestHarness.h"
#include "source/network/AdmissionProtocol.h"
#include "source/network/SessionLifecycle.h"
#include "source/server/AdmissionSession.h"
#include "source/server/ServerConfig.h"

using namespace Duel6;

D6R_TEST_CASE("common admission hosting kind is mandatory bounded and exactly echoed") {
    using namespace Network;
    for (auto kind : {HostingKind::PlayerHosted, HostingKind::Dedicated}) {
        AdmissionIdentitySet ids{7, {8, 9}, 42, kind};
        const auto offer = serializeAdmissionOffer(ids);
        D6R_REQUIRE(sameAdmissionIdentitySet(ids, deserializeAdmissionOffer(offer)));
        D6R_REQUIRE(sameAdmissionIdentitySet(ids, deserializeAdmissionAcceptance(serializeAdmissionAcceptance(ids))));
        D6R_REQUIRE(sameAdmissionIdentitySet(ids, deserializeAdmissionConfirmation(serializeAdmissionConfirmation(ids))));
        for (unsigned invalid : {0u, 3u, 255u}) {
            auto bytes = offer; bytes.back() = invalid;
            D6R_REQUIRE_THROW(deserializeAdmissionOffer(bytes), std::invalid_argument);
        }
        auto missing = offer; missing.pop_back();
        D6R_REQUIRE_THROW(deserializeAdmissionOffer(missing), std::exception);
        auto trailing = offer; trailing.push_back(0);
        D6R_REQUIRE_THROW(deserializeAdmissionOffer(trailing), std::exception);
        auto changed = ids;
        changed.hostingKind = kind == HostingKind::Dedicated ? HostingKind::PlayerHosted : HostingKind::Dedicated;
        D6R_REQUIRE(!sameAdmissionIdentitySet(ids, changed));
    }
}

D6R_TEST_CASE("browser kind and session mismatch reject before provisional allocation without granting controller") {
    using namespace Network;
    const GameplayManifest content{{"levels/arena.json", {}}};
    for (bool dedicated : {false, true}) {
        Server::AdmissionPolicy policy(content, dedicated ? 0 : 1, {}, {}, {}, dedicated);
        Server::AdmissionContext context; context.sessionId = 42;
        auto request = makeLocalAdmissionRequest(1, content);
        request.expectedSessionId = 42;
        request.expectedHostingKind = dedicated ? HostingKind::PlayerHosted : HostingKind::Dedicated;
        const auto mismatch = policy.offer(request, context);
        D6R_REQUIRE(!mismatch.pending());
        D6R_REQUIRE(mismatch.result.code == AdmissionResultCode::SelectedSessionChanged);
        D6R_REQUIRE_EQ(0u, policy.allocation().pendingParticipantCount());
        D6R_REQUIRE(!policy.authorize(10, Trust::AuthorityAction::HostOnly));
        request.expectedHostingKind = dedicated ? HostingKind::Dedicated : HostingKind::PlayerHosted;
        request.expectedSessionId = 43;
        D6R_REQUIRE(!policy.offer(request, context).pending());
        request.expectedSessionId = 42;
        auto bytes = serializeAdmissionRequest(request);
        D6R_REQUIRE(deserializeAdmissionRequest(bytes).expectedHostingKind == request.expectedHostingKind);
        bytes.pop_back();
        D6R_REQUIRE_THROW(deserializeAdmissionRequest(bytes), std::exception);
        request.protocolVersion = 2;
        const auto old = policy.offer(request, context);
        D6R_REQUIRE(old.result.code == AdmissionResultCode::ProtocolIncompatible);
        D6R_REQUIRE_EQ(0u, policy.allocation().pendingParticipantCount());
        request.protocolVersion = AdmissionProtocolVersion;
        request.expectedSessionId = 0; request.expectedHostingKind = HostingKind::Unbound;
        const auto direct = policy.offer(request, context);
        D6R_REQUIRE(direct.pending());
        D6R_REQUIRE(!policy.authorize(10, Trust::AuthorityAction::HostOnly));
        D6R_REQUIRE(policy.rollback(direct.transactionId));
    }
}

D6R_TEST_CASE("common reconnect grant and response bind a strict retained hosting kind") {
    using namespace Network;
    using namespace Network::Lifecycle;
    for (auto kind : {HostingKind::PlayerHosted, HostingKind::Dedicated}) {
        ReconnectGrant grant; grant.sessionId = 42; grant.participantId = 7; grant.reservationId = 3;
        grant.credential.bytes.fill(17); grant.hostingKind = kind;
        auto bytes = serializeReconnectGrant(grant);
        D6R_REQUIRE(deserializeReconnectGrant(bytes)->hostingKind == kind);
        bytes.pop_back(); D6R_REQUIRE(!deserializeReconnectGrant(bytes));
        bytes = serializeReconnectGrant(grant); bytes.back() = 0;
        D6R_REQUIRE(!deserializeReconnectGrant(bytes));
        auto response = serializeReconnectResponse({42, 7, ReconnectOutcome::Accepted, grant});
        D6R_REQUIRE(deserializeReconnectResponse(response)->nextGrant->hostingKind == kind);
        response.back() = 3; D6R_REQUIRE(!deserializeReconnectResponse(response));
        GuestSessionRecovery recovery(42, 7, 1, true);
        D6R_REQUIRE(recovery.acceptGrant(grant));
        const auto now = TimePoint{};
        D6R_REQUIRE(recovery.transportClosed(now, true));
        grant.hostingKind = kind == HostingKind::Dedicated ? HostingKind::PlayerHosted : HostingKind::Dedicated;
        D6R_REQUIRE(!recovery.applyReconnectResult(ReconnectOutcome::Accepted, now, true, 2, grant));
        D6R_REQUIRE(recovery.journey() == GuestJourney::Reconnecting);
    }
}

D6R_TEST_CASE("dedicated default bind is wildcard and explicit binding is retained without proxy") {
#ifdef D6R_DEDICATED_SERVICE
    std::string executable = "duel6r", dedicated = "--dedicated", transport = "--transport";
    std::string ready = "--readiness-socket=/tmp/not-created-by-parser", host = "--host=127.0.0.1";
    char *defaults[]{executable.data(), dedicated.data(), transport.data(), ready.data()};
    D6R_REQUIRE_EQ(std::string("0.0.0.0"), Server::parseServerConfig(4, defaults).listenEndpoint.host);
    char *explicitHost[]{executable.data(), dedicated.data(), transport.data(), ready.data(), host.data()};
    D6R_REQUIRE_EQ(std::string("127.0.0.1"), Server::parseServerConfig(5, explicitHost).listenEndpoint.host);
#endif
}
