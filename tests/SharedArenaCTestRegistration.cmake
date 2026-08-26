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

foreach(D6R_SHARED_ARENA_TOOL Xvfb xdotool import identify convert compare python3 timeout)
    find_program(D6R_SHARED_ARENA_${D6R_SHARED_ARENA_TOOL} ${D6R_SHARED_ARENA_TOOL})
    if (NOT D6R_SHARED_ARENA_${D6R_SHARED_ARENA_TOOL})
        message(FATAL_ERROR "${D6R_SHARED_ARENA_TOOL} is required for the shared-arena behavior test")
    endif ()
endforeach()

add_test(
    NAME shared-arena-behavior
    COMMAND ${D6R_BASH_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/SharedArenaBehaviorTests.sh
)
set_tests_properties(
    shared-arena-behavior
    PROPERTIES
    ENVIRONMENT
        "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/shared-arena-behavior;DISPLAY=:98"
    LABELS "application;regression;e2e;shared-arena"
    RUN_SERIAL TRUE
    TIMEOUT 420
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
        "WORKSPACE_DIR=${CMAKE_SOURCE_DIR};BUILD_DIR=${CMAKE_BINARY_DIR};RESOURCE_DIR=${CMAKE_SOURCE_DIR}/resources;TEST_ROOT=${CMAKE_BINARY_DIR}/menu-redesign-behavior;DISPLAY=:96"
    LABELS "application;regression;e2e;menu"
    RUN_SERIAL TRUE
    TIMEOUT 300
)
