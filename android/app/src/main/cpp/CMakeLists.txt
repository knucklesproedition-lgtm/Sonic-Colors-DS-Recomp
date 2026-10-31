cmake_minimum_required(VERSION 3.22.1)
project(sonicrecomp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apunta a la carpeta core copiada localmente por el workflow en android/app/src/main/cpp/core
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/core)

add_library(sonicrecomp SHARED
    native-lib.cpp
    ${CORE}/nds_rom.cpp
    ${CORE}/layout.cpp
    ${CORE}/cheats.cpp
    ${CORE}/extract_main.cpp
    ${CORE}/selftest.cpp
)

target_include_directories(sonicrecomp PRIVATE ${CORE})
target_link_libraries(sonicrecomp log android GLESv3)

