cmake_minimum_required(VERSION 4.4)

project(WineDirectALSA LANGUAGES NONE)

include(ExternalProject)

ExternalProject_Add(windows
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/windows"
    CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows-clang-cl.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DXWIN_SPLAT_PATH=${XWIN_SPLAT_PATH}
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
)

ExternalProject_Add(linux
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linux"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/linux"
    CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux-clang.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
)

install(CODE "
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\"
            --install \"${CMAKE_BINARY_DIR}/windows\"
            --prefix \"\${CMAKE_INSTALL_PREFIX}\"
        COMMAND_ERROR_IS_FATAL ANY
    )
")

install(CODE "
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\"
            --install \"${CMAKE_BINARY_DIR}/linux\"
            --prefix \"\${CMAKE_INSTALL_PREFIX}\"
        COMMAND_ERROR_IS_FATAL ANY
    )
")

install(
        PROGRAMS wda-install
        DESTINATION .
)

install(
        FILES
        README.md
        README.ko.md
        LICENSE
        DESTINATION .
)
