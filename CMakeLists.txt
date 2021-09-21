cmake_minimum_required(VERSION 3.10)

project(obs-league-spectator VERSION 1.0.0)

set(PLUGIN_AUTHOR "pixxon")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(LibObs REQUIRED)
find_package(Poco CONFIG REQUIRED Net NetSSLWin)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
)

configure_file(
    installer/installer.iss.in
    ../installer/installer.generated.iss
)

set(PLUGIN_SOURCES
    src/gdi.cpp
    src/lcu.cpp
    src/plugin.cpp
    src/plugin-main.cpp
)

set(PLUGIN_HEADERS
    src/gdi.h
    src/lcu.h
    src/plugin.h
    src/plugin-macros.generated.h
)

add_library(${CMAKE_PROJECT_NAME} MODULE
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    libobs
    Poco::Net
    Poco::NetSSLWin
    gdiplus
)

set_target_properties(${CMAKE_PROJECT_NAME}
PROPERTIES
    PREFIX ""
)

target_compile_options(${CMAKE_PROJECT_NAME}
PRIVATE
    "$<$<CONFIG:Release>:/Zi>"
)

target_link_options(${CMAKE_PROJECT_NAME}
PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>"
    "$<$<CONFIG:Release>:/OPT:REF>"
    "$<$<CONFIG:Release>:/OPT:ICF>"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_NAME "64bit")
else()
    set(ARCH_NAME "32bit")
endif()

install(
TARGETS
    ${CMAKE_PROJECT_NAME}
LIBRARY
    DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/${ARCH_NAME}"
)

install(
FILES
    $<TARGET_PDB_FILE:${PROJECT_NAME}>
DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/${ARCH_NAME}"
)
