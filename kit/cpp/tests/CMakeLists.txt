# The protocol core and the client, against FakeTransport. Runs on any host.
add_executable(ecr17_kit_tests
    test_lrc.cpp
    test_packet_codec.cpp
    test_protocol.cpp
    test_protocol_commands.cpp
    test_response.cpp
    test_session.cpp
    test_retry_policy.cpp
    test_client.cpp
    test_integration_terminal.cpp
    test_flows.cpp
)
target_include_directories(ecr17_kit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(ecr17_kit_tests PRIVATE /W4 /utf-8)
else()
    target_compile_options(ecr17_kit_tests PRIVATE -Wall -Wextra)
endif()
target_link_libraries(ecr17_kit_tests PRIVATE Ecr17Kit::Ecr17Kit GTest::gtest_main)
gtest_discover_tests(ecr17_kit_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 60)
