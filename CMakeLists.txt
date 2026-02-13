cmake_minimum_required(VERSION 3.20)

project(socketpoll LANGUAGES CXX)

set(SOCKETPOLL_IS_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(SOCKETPOLL_IS_TOP_LEVEL ON)
endif()

# Platform detection
include(CheckIncludeFiles)
check_include_files("windows.h;winsock2.h;ws2tcpip.h" HAVE_WINSOCK_HEADERS)
check_include_files("sys/socket.h;netinet/in.h;arpa/inet.h" HAVE_POSIX_SOCKET_HEADERS)
check_include_files("sys/epoll.h" HAVE_EPOLL_HEADERS)
check_include_files("sys/types.h;sys/event.h" HAVE_KQUEUE_HEADERS)

# Determine platform defaults
if(HAVE_WINSOCK_HEADERS)
    set(DEFAULT_SOCKET "win")
    set(DEFAULT_POLL "winsock")
elseif(HAVE_POSIX_SOCKET_HEADERS)
    set(DEFAULT_SOCKET "posix")
    if(HAVE_EPOLL_HEADERS)
        set(DEFAULT_POLL "epoll")
    elseif(HAVE_KQUEUE_HEADERS)
        set(DEFAULT_POLL "kqueue")
    else()
        message(FATAL_ERROR "No poll headers were found.")
    endif()
else()
    message(FATAL_ERROR "No socket headers were found.")
endif()

# Allowed implementations
set(ALLOWED_POLL_IMPLS "winsock" "kqueue" "epoll")
set(ALLOWED_SOCKET_IMPLS "win" "posix")

# User can configure the cache variables
set(POLL_IMPL "${DEFAULT_POLL}" CACHE STRING "Poll implementation to use")
set(SOCKET_IMPL "${DEFAULT_SOCKET}" CACHE STRING "Socket implementation to use")
set_property(CACHE POLL_IMPL PROPERTY STRINGS ${ALLOWED_POLL_IMPLS})
set_property(CACHE SOCKET_IMPL PROPERTY STRINGS ${ALLOWED_SOCKET_IMPLS})

# Validate user selections
if(NOT POLL_IMPL IN_LIST ALLOWED_POLL_IMPLS)
    message(FATAL_ERROR "Invalid POLL_IMPL: '${POLL_IMPL}'. Valid options are: ${ALLOWED_POLL_IMPLS}")
endif()
if(NOT SOCKET_IMPL IN_LIST ALLOWED_SOCKET_IMPLS)
    message(FATAL_ERROR "Invalid SOCKET_IMPL: '${SOCKET_IMPL}'. Valid options are: ${ALLOWED_SOCKET_IMPLS}")
endif()

# Validate that selected implementations are supported
if(POLL_IMPL STREQUAL "winsock" AND NOT HAVE_WINSOCK_HEADERS)
    message(FATAL_ERROR "POLL_IMPL='winsock' selected but winsock headers not found on this platform")
endif()
if(POLL_IMPL STREQUAL "epoll" AND NOT HAVE_EPOLL_HEADERS)
    message(FATAL_ERROR "POLL_IMPL='epoll' selected but epoll headers not found on this platform")
endif()
if(POLL_IMPL STREQUAL "kqueue" AND NOT HAVE_KQUEUE_HEADERS)
    message(FATAL_ERROR "POLL_IMPL='kqueue' selected but kqueue headers not found on this platform")
endif()
if(SOCKET_IMPL STREQUAL "win" AND NOT HAVE_WINSOCK_HEADERS)
    message(FATAL_ERROR "SOCKET_IMPL='win' selected but windows socket headers not found on this platform")
endif()
if(SOCKET_IMPL STREQUAL "posix" AND NOT HAVE_POSIX_SOCKET_HEADERS)
    message(FATAL_ERROR "SOCKET_IMPL='posix' selected but POSIX socket headers not found on this platform")
endif()

# Poll implementation selection
if(POLL_IMPL STREQUAL "winsock")
    set(POLL_SRC "src/poll/poll_winsock.cpp")
    list(APPEND POLL_LIBS ws2_32)
elseif(POLL_IMPL STREQUAL "kqueue")
    set(POLL_SRC "src/poll/poll_kqueue.cpp")
elseif(POLL_IMPL STREQUAL "epoll")
    set(POLL_SRC "src/poll/poll_epoll.cpp")
else()
    message(FATAL_ERROR "Invalid POLL_IMPL: ${POLL_IMPL}. Choose from: ${ALLOWED_POLL_IMPLS}")
endif()

# Socket implementation selection
if(SOCKET_IMPL STREQUAL "win")
    set(SOCKET_SRC "src/socket/socket_win.cpp")
elseif(SOCKET_IMPL STREQUAL "posix")
    set(SOCKET_SRC "src/socket/socket_posix.cpp")
else()
    message(FATAL_ERROR "Invalid SOCKET_IMPL: ${SOCKET_IMPL}. Choose from: ${ALLOWED_SOCKET_IMPLS}")
endif()

# Configuration summary
if (SOCKETPOLL_IS_TOP_LEVEL)
    message(STATUS " SocketPoll configuration:")
    message(STATUS "   Poll Impl:         ${POLL_IMPL}")
    message(STATUS "   Socket Impl:       ${SOCKET_IMPL}")
endif()

# Build library
add_library(socketpoll STATIC ${POLL_SRC} ${SOCKET_SRC})
target_include_directories(socketpoll PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(socketpoll PUBLIC ${POLL_LIBS})

# Alias for modern CMake
add_library(socketpoll::socketpoll ALIAS socketpoll)