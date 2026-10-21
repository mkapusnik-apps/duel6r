# CTest registration for the real SDL/OpenGL shared-arena regression.
# Include this file from the root BUILD_TESTING/UNIX block after
# D6R_BASH_EXECUTABLE has been resolved.

add_executable(
    duel6r-networking-prototype-tests
    ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
    ${CMAKE_SOURCE_DIR}/tests/NetworkingPrototypeTests.cpp
)
target_include_directories(duel6r-networking-prototype-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-networking-prototype-tests duel6r-network-scaffold)

add_test(NAME networking-prototype-behavior COMMAND duel6r-networking-prototype-tests)
set_tests_properties(
    networking-prototype-behavior
    PROPERTIES
    LABELS "application;networking;prototype;regression"
    TIMEOUT 60
)

# LocalServerLauncher's Windows command-line formatter has no Windows API
# dependency. Select the Windows branch only for the formatter and its tests;
# transport and TLS must retain the real platform headers and linkage. This is
# a CRT quoting contract check, not a substitute for native Windows execution.
add_executable(
    duel6r-networking-windows-contract-tests
    ${CMAKE_SOURCE_DIR}/tests/TestMain.cpp
    ${CMAKE_SOURCE_DIR}/tests/NetworkingPrototypeTests.cpp
    ${CMAKE_SOURCE_DIR}/source/client/LocalServerLauncher.cpp
)
target_include_directories(duel6r-networking-windows-contract-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(duel6r-networking-windows-contract-tests PRIVATE duel6r-network-scaffold)
target_compile_definitions(duel6r-networking-windows-contract-tests PRIVATE _WIN32)

add_test(NAME networking-windows-command-line-contract COMMAND duel6r-networking-windows-contract-tests)
set_tests_properties(
    networking-windows-command-line-contract
    PROPERTIES
    LABELS "application;networking;prototype;regression;windows-contract"
    TIMEOUT 60
)

foreach(D6R_SHARED_ARENA_TOOL Xvfb xdotool import identify convert compare python3 timeout)
    find_program(D6R_SHARED_ARENA_${D6R_SHARED_ARENA_TOOL} ${D6R_SHARED_ARENA_TOOL})
    if (NOT D6R_SHARED_ARENA_${D6R_SHARED_ARENA_TOOL})
        message(FATAL_ERROR "${D6R_SHARED_ARENA_TOOL} is required for the shared-arena behavior test")
    endif ()
endforeach()

find_program(D6R_ASYNC_MENU_CC_EXECUTABLE cc)
if (NOT D6R_ASYNC_MENU_CC_EXECUTABLE)
    message(FATAL_ERROR "cc is required for the async menu background behavior test")
endif ()

add_test(
    NAME shared-arena-image-assertions
    COMMAND ${D6R_SHARED_ARENA_python3} ${CMAKE_SOURCE_DIR}/tests/SharedArenaImageAssertionsTests.py
)
add_test(NAME menu-transfer-frame-assertions
    COMMAND ${D6R_SHARED_ARENA_python3} ${CMAKE_SOURCE_DIR}/tests/MenuTransferFrameTests.py)
set_tests_properties(menu-transfer-frame-assertions PROPERTIES
    LABELS "application;regression;menu;sanity-unit" TIMEOUT 10)
set_tests_properties(shared-arena-image-assertions PROPERTIES
    LABELS "application;regression;shared-arena"
    TIMEOUT 30
)

foreach(scenario deathmatch-2 predator-3 team2-ff-off-4 team2-ff-on-4
        team3-ff-off-6 team3-ff-on-6 team4-ff-off-8 team4-ff-on-15)
    set(test_name "shared-arena-behavior-${scenario}")
    add_test(NAME ${test_name}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/OwnedProcessRunner.py
                --timeout 100 --log ${CMAKE_BINARY_DIR}/${test_name}/process.log --
                ${D6R_BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/SharedArenaBehaviorTests.sh ${scenario})
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/${test_name};DISPLAY=:98"
        LABELS "application;regression;e2e;shared-arena;sanity-common"
        RUN_SERIAL TRUE TIMEOUT 115)
endforeach()

add_test(
    NAME async-menu-background-behavior
    COMMAND ${D6R_BASH_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/AsyncMenuBackgroundBehaviorTests.sh
)
set_tests_properties(
    async-menu-background-behavior
    PROPERTIES
    ENVIRONMENT
        "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/async-menu-background-behavior;DISPLAY=:95"
    LABELS "application;regression;e2e;menu;async"
    RUN_SERIAL TRUE
    TIMEOUT 120
)

add_test(
    NAME menu-redesign-behavior
    COMMAND ${D6R_BASH_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/MenuRedesignBehaviorTests.sh
)
set_tests_properties(
    menu-redesign-behavior
    PROPERTIES
    ENVIRONMENT
        "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/menu-redesign-behavior;DISPLAY=:96;MENU_REDESIGN_GROUP=basic"
    LABELS "application;regression;e2e;menu"
    RUN_SERIAL TRUE
    TIMEOUT 300
)

add_test(NAME menu-local-session-behavior
    COMMAND ${D6R_BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/MenuRedesignBehaviorTests.sh)
set_tests_properties(menu-local-session-behavior PROPERTIES
    ENVIRONMENT "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/menu-local-session-behavior;DISPLAY=:96;MENU_REDESIGN_GROUP=local"
    LABELS "application;regression;e2e;menu"
    RUN_SERIAL TRUE TIMEOUT 300)

# This existing fixture uses profile Lua callbacks to complete real rounds.
if (D6R_WITH_LUA)
    add_test(NAME final-team-summary-behavior
        COMMAND ${D6R_BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/FinalTeamSummaryBehaviorTests.sh)
    set_tests_properties(final-team-summary-behavior PROPERTIES
        ENVIRONMENT "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/final-team-summary;DISPLAY=:93"
        LABELS "application;regression;e2e;round-summary"
        RUN_SERIAL TRUE TIMEOUT 300)
endif ()

add_test(
    NAME round-summary-progress-behavior
    COMMAND ${D6R_BASH_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/RoundSummaryProgressBehaviorTests.sh
)
set_tests_properties(
    round-summary-progress-behavior
    PROPERTIES
    ENVIRONMENT
        "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/round-summary-progress;DISPLAY=:94"
    LABELS "application;regression;e2e;round-summary"
    RUN_SERIAL TRUE
    TIMEOUT 300
)
include(${CMAKE_SOURCE_DIR}/tests/SessionTransportCTestRegistration.cmake)
