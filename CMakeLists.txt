cmake_minimum_required(VERSION 4.2)
project(UnChat C)

set(CMAKE_C_STANDARD 23)

add_executable(client client.c)
add_executable(server server.c)