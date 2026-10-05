# The Winsock transport against a loopback TCP server: the money-safety contract the
# client relies on (pre-send probe, never writes, never consumes, one drop signal).
# Windows only; no Nitro or React Native needed.
add_executable(ecr17_kit_winsock_tests test_winsock_transport.cpp)
target_compile_options(ecr17_kit_winsock_tests PRIVATE /W4 /utf-8)
target_compile_definitions(ecr17_kit_winsock_tests PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(ecr17_kit_winsock_tests PRIVATE Ecr17Kit::Ecr17Kit GTest::gtest_main)
# A hung probe (e.g. a blocking peek after the reader drained the socket) must fail, not hang.
gtest_discover_tests(ecr17_kit_winsock_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 30)
