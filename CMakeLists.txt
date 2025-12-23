cmake_minimum_required(VERSION 3.20)
project(chatThroughThePipeForLinux)

set(CMAKE_CXX_STANDARD 23)

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

include_directories(src/network src/ui)

add_executable(chatThroughThePipeForLinux
        src/main.cpp
        src/network/Sender.cpp
        src/network/Receiver.cpp
        src/network/Chat.cpp
        src/network/TCP.h
        src/network/Sender.h
        src/network/Receiver.h
        src/network/Chat.h
        src/ui/ChatUI.cpp
        src/ui/ChatUI.h
)

target_link_libraries(chatThroughThePipeForLinux PRIVATE raylib glfw OpenGL::GL Threads::Threads)