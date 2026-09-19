# Tests disabled until full implementation
# Run with: cmake --build build && ctest --test-dir build

# TODO: Add Google Test or Catch2
# FetchContent_Declare(
#     googletest
#     GIT_REPOSITORY https://github.com/google/googletest.git
#     GIT_TAG release-1.12.1
# )
# FetchContent_MakeAvailable(googletest)

# add_executable(crdl_tests
#     test_main.cpp
#     test_config.cpp
#     test_http_client.cpp
# )

# target_link_libraries(crdl_tests
#     PRIVATE
#         crdl_core
#         gtest_main
# )

# add_test(NAME crdl_tests COMMAND crdl_tests)
