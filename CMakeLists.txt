cmake_minimum_required(VERSION 4.2)
project(UnChat C)

set(CMAKE_C_STANDARD 23)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET
        gtk4>=4.14
        gmodule-export-2.0
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(client client.c)
add_executable(server server.c)
target_link_libraries(client PRIVATE PkgConfig::GTK4)