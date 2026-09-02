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

add_executable(duel6r-authoritative-match-behavior-tests
        ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
        ${CMAKE_SOURCE_DIR}/tests/AuthoritativeMatchBehaviorTests.cpp)
target_include_directories(duel6r-authoritative-match-behavior-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-authoritative-match-behavior-tests duel6r-network-scaffold)
if (MINGW)
    set_property(TARGET duel6r-authoritative-match-behavior-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
endif ()
add_test(NAME duel6r-authoritative-match-behavior-tests COMMAND duel6r-authoritative-match-behavior-tests)
set_tests_properties(duel6r-authoritative-match-behavior-tests PROPERTIES
        LABELS "application;integration;network;authoritative-match;determinism"
        TIMEOUT 180)

if (NOT D6R_TRANSPORT_ONLY)
    add_executable(duel6r-local-play-pick-animation-tests
            ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
            ${CMAKE_SOURCE_DIR}/tests/LocalPlayPickAnimationTests.cpp
            ${CMAKE_SOURCE_DIR}/source/Sprite.cpp
            ${CMAKE_SOURCE_DIR}/source/math/Vector.cpp
            ${CMAKE_SOURCE_DIR}/source/aseprite/aseprite.cpp
            ${CMAKE_SOURCE_DIR}/source/aseprite/aseprite_to_animation.cpp
            ${CMAKE_SOURCE_DIR}/source/aseprite/tinf/tinf.cpp)
    target_include_directories(duel6r-local-play-pick-animation-tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(duel6r-local-play-pick-animation-tests PRIVATE
            D6R_HEADLESS_CORE
            D6R_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if (MINGW)
        set_property(TARGET duel6r-local-play-pick-animation-tests APPEND_STRING PROPERTY LINK_FLAGS " -mconsole")
    endif ()
    add_test(NAME duel6r-local-play-pick-animation-tests COMMAND duel6r-local-play-pick-animation-tests)
    set_tests_properties(duel6r-local-play-pick-animation-tests PROPERTIES
            LABELS "application;local-play;animation;weapon-pick"
            TIMEOUT 30)
endif ()

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

if (UNIX)
    add_executable(duel6r-host-service-linux-process-tests
            ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
            ${CMAKE_SOURCE_DIR}/tests/HostServiceLinuxProcessTests.cpp)
    target_include_directories(duel6r-host-service-linux-process-tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(duel6r-host-service-linux-process-tests duel6r-network-scaffold)
    target_compile_definitions(duel6r-host-service-linux-process-tests PRIVATE
            D6R_HOST_SERVICE_TEST_CHILD="$<TARGET_FILE:duel6r-host-service-test-child>")
    add_dependencies(duel6r-host-service-linux-process-tests duel6r-host-service-test-child)
    add_test(NAME duel6r-host-service-linux-process-tests COMMAND duel6r-host-service-linux-process-tests)
    set_tests_properties(duel6r-host-service-linux-process-tests PROPERTIES
            LABELS "application;integration;network;host-service;process;linux"
            TIMEOUT 30)
endif ()

if (UNIX OR WIN32)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if (NOT D6R_TRANSPORT_ONLY)
        add_test(
                NAME duel6r-authoritative-match-process-tests
                COMMAND ${Python3_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/tests/AuthoritativeMatchProcessTests.py
                        $<TARGET_FILE:${D6R_SERVER_APP_NAME}>
        )
        set_tests_properties(duel6r-authoritative-match-process-tests PROPERTIES
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                LABELS "application;integration;network;authoritative-match;headless;process"
                TIMEOUT 120)
    endif ()

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

    if (UNIX)
        add_test(
                NAME duel6r-host-service-orphan-process-tests
                COMMAND ${Python3_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/tests/HostServiceProcessTests.py
                        $<TARGET_FILE:${D6R_HOST_SUPERVISOR_APP_NAME}>
                        $<TARGET_FILE:duel6r-host-service-test-child>
                        --orphan-stress
        )
        set_tests_properties(duel6r-host-service-orphan-process-tests PROPERTIES
                LABELS "application;integration;network;host-service;process;linux;orphan"
                TIMEOUT 30)
    endif ()
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
