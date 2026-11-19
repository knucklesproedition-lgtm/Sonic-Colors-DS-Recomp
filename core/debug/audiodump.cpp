// Ejecuta la ROM con pulsaciones y guarda el audio como WAV: ./audiodump rom unidadesM salida.wav "pulsaciones"
#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.set_graphics(1, 0); c.set_frameskip(true);
    uint64_t units = atoll(argv[2]) * 1000000ull;
    std::vector<std::tuple<uint64_t,uint64_t,uint32_t>> pr; if (argc > 4) { std::string a = argv[4]; size_t p = 0; while (p < a.size()) { size_t e = a.find(',', p); if (e == std::string::npos) e = a.size(); unsigned t, d, k; if (std::sscanf(a.substr(p, e - p).c_str(), "%u:%u:%x", &t, &d, &k) == 3) pr.push_back({t * 1000000ull, d * 1000000ull, k}); p = e + 1; } }
    std::vector<int16_t> pcm; int16_t buf[4096 * 2]; uint64_t done = 0;
    while (done < units) { c.keys = 0; for (auto& x : pr) if (done >= std::get<0>(x) && done < std::get<0>(x) + std::get<1>(x)) c.keys |= std::get<2>(x);
        c.run_units(500000); done += 500000; size_t n; while ((n = c.audio_read(buf, 4096)) > 0) pcm.insert(pcm.end(), buf, buf + n * 2); }
    FILE* o = std::fopen(argv[3], "wb"); uint32_t rate = Console::kAudioRate, bytes = (uint32_t)pcm.size() * 2, br = rate * 4; uint16_t fmt = 1, ch = 2, al = 4, bits = 16; uint32_t v16 = 16, sz = 36 + bytes;
    std::fwrite("RIFF", 1, 4, o); std::fwrite(&sz, 4, 1, o); std::fwrite("WAVEfmt ", 1, 8, o); std::fwrite(&v16, 4, 1, o); std::fwrite(&fmt, 2, 1, o); std::fwrite(&ch, 2, 1, o); std::fwrite(&rate, 4, 1, o); std::fwrite(&br, 4, 1, o); std::fwrite(&al, 2, 1, o); std::fwrite(&bits, 2, 1, o);
    std::fwrite("data", 1, 4, o); std::fwrite(&bytes, 4, 1, o); std::fwrite(pcm.data(), 2, pcm.size(), o); std::fclose(o);
    std::printf("muestras estereo: %zu (%.1f s a %d Hz)\n", pcm.size() / 2, pcm.size() / 2.0 / rate, (int)rate);
}
