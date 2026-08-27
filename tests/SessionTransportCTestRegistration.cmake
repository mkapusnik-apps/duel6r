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
        TIMEOUT 120)

if (UNIX)
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
endif ()
