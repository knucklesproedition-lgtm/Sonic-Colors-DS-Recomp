// cheats.h - Motor de trucos estilo Action Replay (AR) de DS + lector de archivos de trucos.
//
// IMPORTANTE (recomp): los trucos que modifican DATOS en RAM (vidas, anillos, vida, etc.) funcionan.
// Los que parchean CODIGO del juego (escribir instrucciones) NO pueden funcionar en un recomp estatico,
// porque el codigo ya no se interpreta desde la memoria: esta convertido a C.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nds {

// Acceso a la memoria emulada de DS (lo implementara la capa de hardware).
struct MemIf {
    virtual ~MemIf() = default;
    virtual uint32_t r32(uint32_t a) = 0;
    virtual uint32_t r16(uint32_t a) = 0;
    virtual uint32_t r8(uint32_t a) = 0;
    virtual void w32(uint32_t a, uint32_t v) = 0;
    virtual void w16(uint32_t a, uint32_t v) = 0;
    virtual void w8(uint32_t a, uint32_t v) = 0;
};

struct Cheat {
    std::string name;
    std::vector<uint32_t> words;  // pares (A,B) de AR
    bool enabled = false;
};

// Texto de codigo -> palabras de 32 bits. Acepta "94000130 FFFB0000", con saltos de linea o sin espacios.
// Devuelve false si hay caracteres invalidos o un numero impar de palabras.
bool parse_code_text(const std::string& text, std::vector<uint32_t>& words);

// Lee un archivo de trucos de texto. Formatos tolerados:
//  (1) clave/valor:   cheat0_desc = "Nombre"  /  cheat0_code = "AAAAAAAA BBBBBBBB ..."  /  cheat0_enable = true
//      (tambien cheatN_name)
//  (2) texto plano:   una linea con el nombre (opcionalmente [Nombre]) y debajo lineas hexadecimales.
std::vector<Cheat> parse_cheat_file(const std::string& text);

// Ejecuta un codigo AR una vez (hay que llamarlo cada fotograma). Subconjunto soportado:
// 0,1,2 (escrituras), 3-6 y 7-A (condicionales: si es FALSO se salta TODO hasta el siguiente D0/D2), B, C/D1 (bucle),
// D0,D2-DC (registros de offset/datos), E (parche de bytes). Tipo F y otros: ignorados.
void run_ar_code(const std::vector<uint32_t>& words, MemIf& mem);

class CheatEngine {
public:
    void clear() { cheats_.clear(); }
    int add(const std::string& name, const std::vector<uint32_t>& words, bool enabled);
    void set_enabled(int id, bool on);
    void apply_frame(MemIf& mem);
    size_t size() const { return cheats_.size(); }
private:
    std::vector<Cheat> cheats_;
};

}  // namespace nds
