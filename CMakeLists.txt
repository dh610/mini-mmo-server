cmake_minimum_required(VERSION 3.16)
project(mini-mmo-server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

file(GLOB_RECURSE MMO_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(mmo-server ${MMO_SOURCES})
target_include_directories(mmo-server PRIVATE src)
target_link_libraries(mmo-server PRIVATE pthread)

add_executable(mmo-bot bot/main.cpp src/net/Packet.cpp src/net/Socket.cpp)
target_include_directories(mmo-bot PRIVATE src)
target_link_libraries(mmo-bot PRIVATE pthread)

enable_testing()
add_executable(mmo-tests tests/test_packet.cpp tests/test_world.cpp
               src/net/Packet.cpp src/world/World.cpp)
target_include_directories(mmo-tests PRIVATE src third_party/doctest)
add_test(NAME mmo-tests COMMAND mmo-tests)
