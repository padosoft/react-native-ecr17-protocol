# The POSIX transport against a loopback TCP server: the money-safety contract the
# client relies on (pre-send probe, never writes, never consumes, one drop signal, no
# SIGPIPE). macOS and Linux; no Node.js needed.
add_executable(ecr17_posix_tests test_posix_transport.cpp)
target_compile_options(ecr17_posix_tests PRIVATE -Wall -Wextra)
target_link_libraries(ecr17_posix_tests PRIVATE ecr17::ecr17 GTest::gtest_main)
# A hung probe (e.g. a blocking peek after the reader drained the socket) must fail, not hang.
gtest_discover_tests(ecr17_posix_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 30)
