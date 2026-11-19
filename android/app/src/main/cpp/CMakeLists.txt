cmake_minimum_required(VERSION 3.22.1)
project(sonicrecomp CXX)
set(CMAKE_CXX_STANDARD 17)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core)

add_library(sonicrecomp SHARED
    native-lib.cpp
    ${CORE}/nds_rom.cpp
    ${CORE}/layout.cpp
    ${CORE}/cheats.cpp
    ${CORE}/arm_cpu.cpp
    ${CORE}/ds_system.cpp
    ${CORE}/mods.cpp
    ${CORE}/ds_video.cpp
    ${CORE}/gpu3d.cpp
    ${CORE}/gl_present.cpp
    # Aqui se anadiran: lifted/*.c (codigo recompilado), hw/*.cpp (GPU, audio, IPC)
)
target_include_directories(sonicrecomp PRIVATE ${CORE})
target_link_libraries(sonicrecomp log android GLESv3)
