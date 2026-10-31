cmake_minimum_required(VERSION 3.22.1)
project(sonicrecomp CXX)
set(CMAKE_CXX_STANDARD 17)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core)

add_library(sonicrecomp SHARED
    native-lib.cpp
    ${CORE}/nds_rom.cpp
    ${CORE}/layout.cpp
    ${CORE}/cheats.cpp
    # Aqui se anadiran: lifted/*.c (codigo recompilado), hw/*.cpp (GPU, audio, IPC)
)
target_include_directories(sonicrecomp PRIVATE ${CORE})
target_link_libraries(sonicrecomp log android GLESv3)
