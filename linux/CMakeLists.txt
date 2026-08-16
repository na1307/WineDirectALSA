cmake_minimum_required(VERSION 4.4)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(WineDirectALSALinux LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
find_package(ALSA REQUIRED)
pkg_check_modules(LIBSYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_library(winedirectalsa SHARED
        src/main.cpp
        src/realtime_thread.cpp
        src/audio_reservation.cpp
        src/alsa_control.cpp
)

target_sources(winedirectalsa
        PRIVATE FILE_SET modules TYPE CXX_MODULES FILES
        src/realtime_thread.cppm
        src/audio_reservation.cppm
        src/alsa_control.cppm
)

set_target_properties(winedirectalsa PROPERTIES PREFIX "")

target_compile_definitions(winedirectalsa PRIVATE WINE_UNIX_LIB)

target_include_directories(winedirectalsa
        PRIVATE /usr/include/wine
        PRIVATE /usr/include/wine/windows
        PRIVATE /opt/wine-devel/include/wine
        PRIVATE /opt/wine-devel/include/wine/windows
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(winedirectalsa PRIVATE PkgConfig::LIBSYSTEMD PRIVATE ALSA::ALSA)

install(
        TARGETS winedirectalsa
        LIBRARY DESTINATION x86_64-unix
)
