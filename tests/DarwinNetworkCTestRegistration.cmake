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
add_executable(duel6r-host-service-test-child tests/HostServiceTestChild.cpp)
target_include_directories(duel6r-host-service-test-child PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-host-service-test-child PRIVATE duel6r-network-scaffold)
target_include_directories(duel6r-darwin-adapter-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-darwin-adapter-tests PRIVATE duel6r-network-scaffold)
target_compile_definitions(duel6r-darwin-adapter-tests PRIVATE
    D6R_ADAPTER_SERVER="$<TARGET_FILE:${D6R_SERVER_APP_NAME}>"
    D6R_ADAPTER_TIMEOUT_CHILD="$<TARGET_FILE:duel6r-host-service-test-child>"
    D6R_ADAPTER_RESOURCES="${CMAKE_SOURCE_DIR}/resources")
add_dependencies(duel6r-darwin-adapter-tests ${D6R_SERVER_APP_NAME} duel6r-host-service-test-child)
add_test(NAME darwin-production-adapters COMMAND duel6r-darwin-adapter-tests)
set_tests_properties(darwin-production-adapters PROPERTIES TIMEOUT 90 LABELS "application;network;darwin;process")

add_library(duel6r-darwin-random-faults SHARED tests/DarwinRandomFaults.cpp)
add_executable(duel6r-darwin-random-tests tests/TestMain.cpp tests/DarwinRandomTests.cpp)
target_include_directories(duel6r-darwin-random-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(duel6r-darwin-random-tests PRIVATE D6R_RANDOM_RESOURCES="${CMAKE_SOURCE_DIR}/resources")
target_link_libraries(duel6r-darwin-random-tests PRIVATE duel6r-network-scaffold duel6r-canonical-gameplay-core
    duel6r-darwin-random-faults)
add_test(NAME darwin-system-random COMMAND duel6r-darwin-random-tests)
set_tests_properties(darwin-system-random PROPERTIES TIMEOUT 30 LABELS "application;network;darwin;security")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME darwin-authoritative-fixtures COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/AuthoritativeMatchProcessTests.py $<TARGET_FILE:${D6R_SERVER_APP_NAME}>)
set_tests_properties(darwin-authoritative-fixtures PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    TIMEOUT 180 LABELS "application;network;darwin;native-semantics")
