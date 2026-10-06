# Real production network dependencies. GUI/Xvfb tests remain in their existing
# platform registration; no transport/trust cases are trimmed for Darwin.
add_executable(duel6r-darwin-session-transport-tests tests/SessionTransportTests.cpp)
target_include_directories(duel6r-darwin-session-transport-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-darwin-session-transport-tests PRIVATE duel6r-network-scaffold)
add_test(NAME darwin-session-transport COMMAND duel6r-darwin-session-transport-tests)
set_tests_properties(darwin-session-transport PROPERTIES TIMEOUT 180 LABELS "application;network;darwin;transport")

foreach(suite IN ITEMS AdmissionCompatibility HostServiceSupervisor SessionLifecycle AuthoritativePlayerInput StateReplication NetworkResponsiveness)
    add_executable(duel6r-darwin-${suite}-tests tests/TestMain.cpp tests/${suite}Tests.cpp)
    target_include_directories(duel6r-darwin-${suite}-tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(duel6r-darwin-${suite}-tests PRIVATE duel6r-network-scaffold)
    add_test(NAME darwin-${suite} COMMAND duel6r-darwin-${suite}-tests)
    set_tests_properties(darwin-${suite} PROPERTIES TIMEOUT 180 LABELS "application;network;darwin;regression")
endforeach()

add_executable(duel6r-darwin-adapter-tests tests/TestMain.cpp tests/DarwinAdapterTests.cpp)
target_include_directories(duel6r-darwin-adapter-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-darwin-adapter-tests PRIVATE duel6r-network-scaffold)
target_compile_definitions(duel6r-darwin-adapter-tests PRIVATE
    D6R_ADAPTER_SERVER="$<TARGET_FILE:${D6R_SERVER_APP_NAME}>"
    D6R_ADAPTER_RESOURCES="${CMAKE_SOURCE_DIR}/resources")
add_dependencies(duel6r-darwin-adapter-tests ${D6R_SERVER_APP_NAME})
add_test(NAME darwin-production-adapters COMMAND duel6r-darwin-adapter-tests)
set_tests_properties(darwin-production-adapters PROPERTIES TIMEOUT 90 LABELS "application;network;darwin;process")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME darwin-authoritative-fixtures COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/AuthoritativeMatchProcessTests.py $<TARGET_FILE:${D6R_SERVER_APP_NAME}>)
set_tests_properties(darwin-authoritative-fixtures PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    TIMEOUT 180 LABELS "application;network;darwin;native-semantics")

if(D6R_NATIVE_DIAGNOSTICS)
    function(d6r_diagnostic name target filter)
        add_test(NAME darwin-diag-${name} COMMAND $<TARGET_FILE:${target}>)
        set_tests_properties(darwin-diag-${name} PROPERTIES
            ENVIRONMENT "D6R_TEST_FILTER=${filter};D6R_TEST_EXACT=1"
            LABELS "native-diagnostic" TIMEOUT 90)
    endfunction()
    d6r_diagnostic(anchor-loss duel6r-darwin-resolver-ownership-tests
        "Darwin lost resolver ownership permanently quarantines PID operations and slot")
    d6r_diagnostic(observation-invariance duel6r-darwin-NetworkResponsiveness-tests
        "Diagnostic observations do not advance probes or replace terminal causes")
    d6r_diagnostic(production-refusal duel6r-darwin-session-transport-tests "lifecycle and failures")
    d6r_diagnostic(production-queue duel6r-darwin-session-transport-tests "queue boundaries")
    d6r_diagnostic(observed-queue duel6r-darwin-session-transport-tests "diagnostic native writer (not acceptance)")
    d6r_diagnostic(raw-refusal duel6r-darwin-session-transport-tests "diagnostic raw refusal (not acceptance)")
    d6r_diagnostic(admission duel6r-darwin-AdmissionCompatibility-tests
        "AC-002 REP-008 REP-038 production admission gates success and validates exact confirmed snapshot ownership")
    d6r_diagnostic(input duel6r-darwin-AdmissionCompatibility-tests
        "NIN production transport fairly drains four-player host and guest input at 60 Hz")
    d6r_diagnostic(summary-removal duel6r-darwin-AdmissionCompatibility-tests
        "PR83 production final-summary Leave and expiry remove membership on direct lobby return")
    d6r_diagnostic(summary-return duel6r-darwin-AdmissionCompatibility-tests
        "AHM-AC-029 REP-013 REP-017 production Headless following lobby disconnect and admission preserve explicit readiness and result ranking")
endif()
