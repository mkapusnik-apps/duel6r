include_guard(GLOBAL)

add_executable(duel6r-session-transport-tests
        ${CMAKE_SOURCE_DIR}/tests/SessionTransportTests.cpp)
target_include_directories(duel6r-session-transport-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-session-transport-tests duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-session-transport-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()
add_test(NAME duel6r-session-transport-tests COMMAND duel6r-session-transport-tests)
set_tests_properties(duel6r-session-transport-tests PROPERTIES
        LABELS "application;network;transport"
        TIMEOUT 180)

add_executable(duel6r-network-trust-policy-tests
        ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
        ${CMAKE_SOURCE_DIR}/tests/NetworkTrustPolicyTests.cpp)
target_include_directories(duel6r-network-trust-policy-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-network-trust-policy-tests duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-network-trust-policy-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()
add_test(NAME duel6r-network-trust-policy-tests COMMAND duel6r-network-trust-policy-tests)
set_tests_properties(duel6r-network-trust-policy-tests PROPERTIES
        LABELS "application;network;security"
        TIMEOUT 30)

add_executable(duel6r-admission-compatibility-tests
        ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
        ${CMAKE_SOURCE_DIR}/tests/AdmissionCompatibilityTests.cpp)
target_include_directories(duel6r-admission-compatibility-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-admission-compatibility-tests duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-admission-compatibility-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()
add_test(NAME duel6r-admission-compatibility-tests COMMAND duel6r-admission-compatibility-tests)
set_tests_properties(duel6r-admission-compatibility-tests PROPERTIES
        LABELS "application;integration;network;admission;compatibility"
        TIMEOUT 120)

add_executable(duel6r-host-service-supervisor-tests
        ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
        ${CMAKE_SOURCE_DIR}/tests/HostServiceSupervisorTests.cpp)
target_include_directories(duel6r-host-service-supervisor-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-host-service-supervisor-tests duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-host-service-supervisor-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()
add_test(NAME duel6r-host-service-supervisor-tests COMMAND duel6r-host-service-supervisor-tests)
set_tests_properties(duel6r-host-service-supervisor-tests PROPERTIES
        LABELS "application;integration;network;host-service;lifecycle"
        TIMEOUT 60)

add_executable(duel6r-host-service-test-child
        ${CMAKE_SOURCE_DIR}/tests/HostServiceTestChild.cpp)
target_include_directories(duel6r-host-service-test-child PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-host-service-test-child duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-host-service-test-child APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()

if (UNIX OR WIN32)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
            NAME duel6r-session-transport-process-tests
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/SessionTransportProcessTests.py
                    $<TARGET_FILE:${D6R_SERVER_APP_NAME}>
    )
    set_tests_properties(duel6r-session-transport-process-tests PROPERTIES
            LABELS "application;integration;network;transport"
            TIMEOUT 90)

    add_test(
            NAME duel6r-admission-process-tests
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/AdmissionProcessTests.py
                    $<TARGET_FILE:${D6R_SERVER_APP_NAME}>
    )
    set_tests_properties(duel6r-admission-process-tests PROPERTIES
            LABELS "application;integration;network;admission;process"
            TIMEOUT 45)

    add_test(
            NAME duel6r-host-service-process-tests
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/HostServiceProcessTests.py
                    $<TARGET_FILE:${D6R_HOST_SUPERVISOR_APP_NAME}>
                    $<TARGET_FILE:duel6r-host-service-test-child>
    )
    set_tests_properties(duel6r-host-service-process-tests PROPERTIES
            LABELS "application;integration;network;host-service;process"
            TIMEOUT 45)
endif ()

if (UNIX)
    add_test(
            NAME duel6r-resolver-helper-process-tests
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/ResolverHelperProcessTests.py
                    $<TARGET_FILE:duel6r-session-transport-tests>
                    $<TARGET_FILE:${D6R_RESOLVER_APP_NAME}>
    )
    set_tests_properties(duel6r-resolver-helper-process-tests PROPERTIES
            LABELS "application;integration;network;resolver"
            TIMEOUT 45)
endif ()
