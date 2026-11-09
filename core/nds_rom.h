// nds_rom.h - Lectura de ROMs NDS, descompresion BLZ y extraccion (C++17).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nds {

struct Overlay9 {
    uint32_t id, ram, size, bss, file_id, comp_size;
    bool compressed;
};

struct Header {
    std::string title, gamecode;
    uint32_t arm9_off, arm9_entry, arm9_load, arm9_size;
    uint32_t arm7_off, arm7_entry, arm7_load, arm7_size;
    uint32_t fnt_off, fnt_size, fat_off, fat_size;
    uint32_t ovt9_off, ovt9_size;
};

// Descompresion BLZ (LZ77 hacia atras). Devuelve false si el flujo es invalido.
bool blz_decompress(const uint8_t* data, size_t n, std::vector<uint8_t>& out);

// SHA-1 (para comprobar que la ROM es la version correcta).
std::string sha1_hex(const uint8_t* data, size_t n);

// Version de ROM soportada por este proyecto.
constexpr const char* kExpectedGameCode = "BXSE";
constexpr const char* kExpectedSha1 = "fc76c4f3480b00f02646acb88d6713baf1b1d62a";

using Progress = std::function<void(int percent, const std::string& msg)>;

// Valida y extrae la ROM a out_dir (arm9.bin, arm7.bin, overlays/, files/).
// Devuelve "" si todo fue bien, o un mensaje de error.
std::string extract_rom(const std::string& rom_path, const std::string& out_dir,
                        const Progress& progress, bool extract_files = true);

}  // namespace nds
