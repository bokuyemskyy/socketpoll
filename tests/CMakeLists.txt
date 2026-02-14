include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

enable_testing()

add_executable(tests test_main.cpp test_socket.cpp test_poll.cpp)

target_link_libraries(tests PRIVATE Catch2::Catch2 socketpoll)

include(Catch)
catch_discover_tests(tests)