// Arranca la ROM en el interprete y muestra donde se atasca.
// g++ -std=c++17 -O2 arm_cpu.cpp ds_system.cpp boot_trace.cpp -o boot_trace && ./boot_trace rom.nds [unidades_en_millones]
#include "ds_system.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;

static void top(const char* title, const std::map<uint32_t, uint64_t>& m, int n) {
    std::vector<std::pair<uint64_t, uint32_t>> v; for (auto& kv : m) v.push_back({kv.second, kv.first});
    std::sort(v.rbegin(), v.rend()); std::printf("  %s\n", title);
    for (int i = 0; i < n && i < (int)v.size(); i++) std::printf("    %08x  x%llu\n", v[i].second, (unsigned long long)v[i].first);
}
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c;
    if (!c.load_rom(rom)) { std::puts("ROM no valida"); return 2; }
    uint64_t total = (argc > 2 ? atoll(argv[2]) : 20) * 1000000ull;
    uint64_t chunk = 1000000, done = 0;
    while (done < total) {
        c.run_units(chunk); done += chunk;
        if (done % 5000000 == 0) std::printf("[%3lluM] ARM9 pc=%08x%s  ARM7 pc=%08x%s  frame=%llu  IPC fifo9->7:%zu 7->9:%zu\n", (unsigned long long)(done / 1000000),
            c.cpu9.pc(), c.cpu9.halted ? "(halt)" : "", c.cpu7.pc(), c.cpu7.halted ? "(halt)" : "", (unsigned long long)c.frame, (size_t)0, (size_t)0);
    }
    std::printf("\n== Resultado tras %lluM unidades (%llu fotogramas simulados) ==\n", (unsigned long long)(done / 1000000), (unsigned long long)c.frame);
    std::printf("ARM9: pc=%08x mode=%02x halted=%d  | ARM7: pc=%08x mode=%02x halted=%d\n", c.cpu9.pc(), c.cpu9.mode, c.cpu9.halted, c.cpu7.pc(), c.cpu7.mode, c.cpu7.halted);
    std::printf("video: %llu composiciones, %llu fotogramas 3D (%llu poligonos), fondos no soportados vistos: %llu, sprites afines: %llu\n", (unsigned long long)c.video_frames, (unsigned long long)c.gpu3d.frames, (unsigned long long)c.gpu3d.polys_drawn, (unsigned long long)c.unsupported_bg, (unsigned long long)c.unsupported_obj);
    std::printf("lecturas de ROM por cartucho: %llu bloques\n", (unsigned long long)c.card_blocks);
    { std::map<uint32_t, uint64_t> m(c.card_cmd_count.begin(), c.card_cmd_count.end()); top("comandos de cartucho vistos (byte de comando)", m, 8); }
    top("ARM9: PC mas muestreados", c.pc_samples[0], 6);
    top("ARM7: PC mas muestreados", c.pc_samples[1], 6);
    { std::map<uint32_t,uint64_t> m; for (auto& kv : c.io_unknown[0]) if ((kv.first & 0xFFF) >= 0x1A0 && (kv.first & 0xFFF) < 0x1C0) m[kv.first] = kv.second; top("ARM9: E/S de cartucho/SPI sin implementar", m, 8); }
    top("ARM9: registros de E/S sin implementar (|0x10000 = escritura)", c.io_unknown[0], 12);
    top("ARM7: registros de E/S sin implementar (|0x10000 = escritura)", c.io_unknown[1], 12);
    top("ARM9: SWI sin implementar", c.swi_unhandled[0], 5);
    top("ARM7: SWI sin implementar", c.swi_unhandled[1], 5);
    return 0;
}
