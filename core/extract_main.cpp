// Prueba en PC:  g++ -std=c++17 -O2 nds_rom.cpp extract_main.cpp -o nds_extract
#include "nds_rom.h"
#include <cstdio>
int main(int argc, char** argv) {
    if (argc != 3) { std::puts("uso: nds_extract rom.nds salida/"); return 1; }
    std::string err = nds::extract_rom(argv[1], argv[2], [](int p, const std::string& m) {
        std::printf("[%3d%%] %s\n", p, m.c_str());
    });
    if (!err.empty()) { std::printf("ERROR: %s\n", err.c_str()); return 2; }
    return 0;
}
