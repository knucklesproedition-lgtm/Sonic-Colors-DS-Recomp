#include "nds_rom.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace nds {

static inline uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }

bool blz_decompress(const uint8_t* data, size_t n, std::vector<uint8_t>& out) {
    out.assign(data, data + n);
    if (n < 8) return true;
    uint32_t inc = rd32(data + n - 4);
    if (inc == 0) return true;  // no estaba comprimido
    uint32_t hdr = data[n - 5];
    uint32_t enc = rd32(data + n - 8) & 0xFFFFFF;
    if (hdr > n || enc > n) return false;
    size_t dec_len = n + inc;
    out.assign(dec_len, 0);
    std::memcpy(out.data(), data, n);
    long src = (long)n - hdr, dst = (long)dec_len, stop = (long)n - enc;
    while (src > stop) {
        uint8_t flags = data[--src];
        for (int i = 0; i < 8; i++) {
            if (src <= stop) break;
            if (flags & 0x80) {
                if (src < 2) return false;
                uint8_t b1 = data[--src];
                uint8_t b2 = data[--src];
                int len = (b1 >> 4) + 3;
                long disp = (((b1 & 0xF) << 8) | b2) + 3;
                if (dst - len < 0 || dst + disp > (long)dec_len) return false;
                for (int k = 0; k < len; k++) {
                    --dst;
                    out[dst] = out[dst + disp];
                }
            } else {
                if (dst <= 0) return false;
                out[--dst] = data[--src];
            }
            flags <<= 1;
        }
    }
    return true;
}

// ---- SHA-1 minimo ----
static inline uint32_t rol(uint32_t v, int s) { return (v << s) | (v >> (32 - s)); }
std::string sha1_hex(const uint8_t* data, size_t n) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    auto block = [&](const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)p[i*4] << 24) | (p[i*4+1] << 16) | (p[i*4+2] << 8) | p[i*4+3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                     k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
    };
    size_t full = n / 64;
    for (size_t i = 0; i < full; i++) block(data + i * 64);
    uint8_t tail[128] = {0};
    size_t rem = n - full * 64;
    std::memcpy(tail, data + full * 64, rem);
    tail[rem] = 0x80;
    size_t tl = (rem < 56) ? 64 : 128;
    uint64_t bits = (uint64_t)n * 8;
    for (int i = 0; i < 8; i++) tail[tl - 1 - i] = (uint8_t)(bits >> (8 * i));
    block(tail);
    if (tl == 128) block(tail + 64);
    char buf[41];
    for (int i = 0; i < 5; i++) std::snprintf(buf + i * 8, 9, "%08x", h[i]);
    return std::string(buf);
}

static bool write_file(const fs::path& p, const uint8_t* d, size_t n) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f.write((const char*)d, (std::streamsize)n);
    return (bool)f;
}

static void walk(const std::vector<uint8_t>& rom, const Header& h, uint16_t did,
                 const std::string& path, std::vector<std::pair<uint32_t, std::string>>& out,
                 int depth) {
    if (depth > 32) return;
    const uint8_t* fnt = rom.data() + h.fnt_off;
    uint32_t off = rd32(fnt + (did - 0xF000) * 8);
    uint16_t fid = rd16(fnt + (did - 0xF000) * 8 + 4);
    uint32_t p = off;
    while (p < h.fnt_size) {
        uint8_t t = fnt[p++];
        if (t == 0) break;
        uint32_t ln = t & 0x7F;
        std::string name((const char*)fnt + p, ln);
        p += ln;
        if (t & 0x80) {
            uint16_t sid = rd16(fnt + p);
            p += 2;
            walk(rom, h, sid, path + name + "/", out, depth + 1);
        } else {
            out.emplace_back(fid++, path + name);
        }
    }
}

std::string extract_rom(const std::string& rom_path, const std::string& out_dir,
                        const Progress& progress, bool extract_files) {
    auto report = [&](int pc, const std::string& m) { if (progress) progress(pc, m); };

    report(0, "Leyendo ROM");
    std::ifstream f(rom_path, std::ios::binary | std::ios::ate);
    if (!f) return "No se pudo abrir la ROM";
    size_t size = (size_t)f.tellg();
    if (size < 0x200) return "El archivo no parece una ROM de DS";
    std::vector<uint8_t> rom(size);
    f.seekg(0);
    f.read((char*)rom.data(), (std::streamsize)size);

    Header h;
    h.title.assign((const char*)rom.data(), strnlen((const char*)rom.data(), 12));
    h.gamecode.assign((const char*)rom.data() + 12, 4);
    const uint8_t* b = rom.data() + 0x20;
    uint32_t* fields[] = {&h.arm9_off, &h.arm9_entry, &h.arm9_load, &h.arm9_size,
                          &h.arm7_off, &h.arm7_entry, &h.arm7_load, &h.arm7_size,
                          &h.fnt_off, &h.fnt_size, &h.fat_off, &h.fat_size,
                          &h.ovt9_off, &h.ovt9_size};
    for (int i = 0; i < 14; i++) *fields[i] = rd32(b + i * 4);

    if (h.gamecode != kExpectedGameCode)
        return "ROM incorrecta: se necesita Sonic Colors (USA), codigo BXSE. Esta es " + h.gamecode;

    report(5, "Comprobando version (SHA-1)");
    std::string sha = sha1_hex(rom.data(), rom.size());
    if (sha != kExpectedSha1)
        return "Version de ROM no soportada (SHA-1 " + sha + ")";

    fs::path out(out_dir);
    std::error_code ec;
    fs::create_directories(out, ec);

    report(15, "Descomprimiendo ARM9");
    std::vector<uint8_t> a9;
    if (!blz_decompress(rom.data() + h.arm9_off, h.arm9_size, a9)) return "Fallo al descomprimir ARM9";
    if (!write_file(out / "arm9.bin", a9.data(), a9.size())) return "No se pudo escribir arm9.bin";
    if (!write_file(out / "arm7.bin", rom.data() + h.arm7_off, h.arm7_size)) return "No se pudo escribir arm7.bin";

    report(25, "Descomprimiendo overlays");
    uint32_t nov = h.ovt9_size / 32;
    for (uint32_t i = 0; i < nov; i++) {
        const uint8_t* e = rom.data() + h.ovt9_off + i * 32;
        uint32_t id = rd32(e), fid = rd32(e + 24), comp = rd32(e + 28);
        uint32_t s = rd32(rom.data() + h.fat_off + fid * 8);
        uint32_t en = rd32(rom.data() + h.fat_off + fid * 8 + 4);
        std::vector<uint8_t> dec;
        if ((comp >> 24) & 1) {
            if (!blz_decompress(rom.data() + s, en - s, dec)) return "Fallo al descomprimir overlay " + std::to_string(id);
        } else {
            dec.assign(rom.data() + s, rom.data() + en);
        }
        char name[32];
        std::snprintf(name, sizeof name, "overlays/ov%02u.bin", id);
        if (!write_file(out / name, dec.data(), dec.size())) return "No se pudo escribir un overlay";
    }

    if (!extract_files) { report(100, "Listo"); return ""; }          // el motor lee los archivos directamente de la ROM
    report(40, "Extrayendo archivos del juego");
    std::vector<std::pair<uint32_t, std::string>> files;
    walk(rom, h, 0xF000, "", files, 0);
    size_t k = 0;
    for (auto& [fid, path] : files) {
        uint32_t s = rd32(rom.data() + h.fat_off + fid * 8);
        uint32_t en = rd32(rom.data() + h.fat_off + fid * 8 + 4);
        if (en < s || en > rom.size()) return "Tabla de archivos corrupta";
        if (!write_file(out / "files" / path, rom.data() + s, en - s)) return "No se pudo escribir " + path;
        if (++k % 16 == 0) report(40 + (int)(59 * k / files.size()), path);
    }
    report(100, "Listo");
    return "";
}

}  // namespace nds
