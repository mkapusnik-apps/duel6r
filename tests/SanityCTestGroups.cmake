# Primary CI groups are explicit: descriptive labels and source filenames do not
# distinguish offline gameplay from hosted solo, or model checks from journeys.
# Include after all registrations. Platform suites remain unfiltered overlays.
set(D6R_SANITY_UNIT_TESTS
    application-mode
    darwin-cleanup-state
    duel6r-tests
    duel6r-directory-origin-configured-tests
    duel6r-directory-origin-unchannelled-tests
    duel6r-directory-origin-nightly-tests
    duel6r-directory-origin-release-tests
    ctest-diagnostics-collection
    shared-arena-image-assertions
    async-menu-background-behavior
    menu-redesign-behavior
    mac-pointer-behavior
    duel6r-font-utf8-tests
    console-configuration
)
set(D6R_SANITY_COMMON_TESTS
    shared-arena-behavior
    menu-local-session-behavior
    round-summary-progress-behavior
    final-team-summary-behavior
    safe-empty-match-start
    renderer-batch-behavior
    mac-drawable-viewport-behavior
    duel6r-quick-liquid-sparse-team-tests
    duel6r-quick-liquid-behavior-tests
    duel6r-local-play-pick-animation-tests
    duel6r-local-play-shit-thrower-sanitizer-tests
)
set(D6R_SANITY_NETWORK_TESTS
    application-mode-process
    openssl-certificate-fixture
    networking-prototype-behavior
    networking-windows-command-line-contract
    duel6r-dedicated-service-process-tests
    staging-allocation-exception
    native-secure-session
    duel6r-hardware-crypto-tests
    duel6r-session-transport-tests
    duel6r-authoritative-match-behavior-tests
    duel6r-authoritative-player-input-tests
    duel6r-state-replication-tests
    duel6r-network-responsiveness-tests
    duel6r-network-trust-policy-tests
    duel6r-session-lifecycle-tests
    duel6r-network-session-runtime-tests
    public-crypto-composed-coverage
    duel6r-public-dedicated-process-tests
    duel6r-public-dedicated-reuse-tests
    duel6r-public-dedicated-end-eof-regressions
    duel6r-public-dedicated-review-regressions
    duel6r-public-dedicated-authorization-tests
    host-end-boundary-host-end
    host-end-boundary-ordinary-close
    duel6r-admission-compatibility-tests
    duel6r-host-service-supervisor-tests
    duel6r-host-service-linux-process-tests
    duel6r-authoritative-match-process-tests
    duel6r-session-transport-process-tests
    duel6r-admission-process-tests
    duel6r-host-service-process-tests
    duel6r-host-service-orphan-process-tests
    duel6r-resolver-helper-process-tests
    directory-independence
    darwin-hardware-crypto
    darwin-hardware-admission
    darwin-secure-session
    darwin-trust-policy
    darwin-system-trust
    darwin-trust-negatives
    darwin-process-boundaries
    darwin-process-containment
    darwin-resolver-ownership
    darwin-session-transport
    darwin-AdmissionCompatibility
    darwin-HostServiceSupervisor
    darwin-SessionLifecycle
    darwin-AuthoritativePlayerInput
    darwin-StateReplication
    darwin-NetworkResponsiveness
    darwin-production-adapters
    darwin-system-random
    darwin-authoritative-fixtures
)

foreach(group UNIT COMMON NETWORK)
    string(TOLOWER "${group}" label)
    foreach(test_name IN LISTS D6R_SANITY_${group}_TESTS)
        # Keep native-only, renderer, transport-only and sanitizer conditions.
        if(TEST "${test_name}")
            set_property(TEST "${test_name}" APPEND PROPERTY LABELS "sanity-${label}")
        endif()
    endforeach()
endforeach()
