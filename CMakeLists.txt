cmake_minimum_required(VERSION 3.22)
project(bulletin-board)

set(CMAKE_CXX_STANDARD 14)

add_executable(super-server main.cpp tcp-utils.cpp tcp-utils.h utilities.cpp utilities.h board-server.cpp board-server.h sync-server.cpp sync-server.h)

add_executable(board-server board-server.cpp board-server.h tcp-utils.cpp tcp-utils.h utilities.cpp utilities.h)

add_executable(sync-server sync-server.cpp sync-server.h tcp-utils.cpp tcp-utils.h utilities.cpp utilities.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(super-server Threads::Threads)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(board-server Threads::Threads)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(sync-server Threads::Threads)
