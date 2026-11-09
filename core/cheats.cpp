#include "cheats.h"

#include <cctype>
#include <sstream>

namespace nds {

static bool is_hex(char c) { return std::isxdigit((unsigned char)c) != 0; }

bool parse_code_text(const std::string& text, std::vector<uint32_t>& words) {
    words.clear();
    std::string hex;
    for (char c : text) {
        if (is_hex(c)) hex += c;
        else if (std::isspace((unsigned char)c) || c == '"' || c == ',' || c == ';') continue;
        else return false;
    }
    if (hex.empty() || hex.size() % 16 != 0) return false;  // cada linea AR = 2 palabras = 16 digitos
    for (size_t i = 0; i < hex.size(); i += 8)
        words.push_back((uint32_t)std::stoul(hex.substr(i, 8), nullptr, 16));
    return true;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string unquote(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    return s;
}
// Una linea "de codigo": solo hex/espacios y al menos 8 digitos
static bool looks_like_code(const std::string& line) {
    int digits = 0;
    for (char c : line) {
        if (is_hex(c)) digits++;
        else if (!std::isspace((unsigned char)c)) return false;
    }
    return digits >= 16 && digits % 8 == 0;
}

std::vector<Cheat> parse_cheat_file(const std::string& text) {
    std::vector<Cheat> out;
    std::istringstream in(text);
    std::string line;
    // ---- formato (1): clave/valor ----
    {
        std::vector<std::string> names, codes, enables;
        auto slot = [&](std::vector<std::string>& v, size_t i) -> std::string& {
            if (v.size() <= i) v.resize(i + 1);
            return v[i];
        };
        bool any = false;
        std::istringstream in1(text);
        while (std::getline(in1, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(line.substr(0, eq)), val = unquote(line.substr(eq + 1));
            if (key.rfind("cheat", 0) != 0) continue;
            size_t us = key.find('_');
            if (us == std::string::npos || us <= 5) continue;
            std::string num = key.substr(5, us - 5), field = key.substr(us + 1);
            if (num.empty() || num.find_first_not_of("0123456789") != std::string::npos) continue;
            size_t idx = (size_t)std::stoul(num);
            if (idx > 100000) continue;
            if (field == "desc" || field == "name") slot(names, idx) = val, any = true;
            else if (field == "code") slot(codes, idx) = val, any = true;
            else if (field == "enable") slot(enables, idx) = val, any = true;
        }
        if (any) {
            for (size_t i = 0; i < codes.size(); i++) {
                Cheat c;
                if (!parse_code_text(codes[i], c.words)) continue;
                c.name = i < names.size() && !names[i].empty() ? names[i] : "Cheat " + std::to_string(i);
                c.enabled = i < enables.size() && (enables[i] == "true" || enables[i] == "1");
                out.push_back(std::move(c));
            }
            return out;
        }
    }
    // ---- formato (2): texto plano ----
    Cheat cur; std::string codetext;
    auto flush = [&]() {
        if (!codetext.empty() && parse_code_text(codetext, cur.words)) {
            if (cur.name.empty()) cur.name = "Cheat " + std::to_string(out.size());
            out.push_back(cur);
        }
        cur = Cheat(); codetext.clear();
    };
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';' || t.rfind("//", 0) == 0) continue;
        if (looks_like_code(t)) { codetext += t + " "; continue; }
        flush();
        if (t.size() >= 2 && t.front() == '[' && t.back() == ']') t = trim(t.substr(1, t.size() - 2));
        cur.name = t;
    }
    flush();
    return out;
}

// ---------------- ejecucion AR ----------------
void run_ar_code(const std::vector<uint32_t>& w, MemIf& m) {
    uint32_t offset = 0, data = 0;
    bool skipping = false;                                      // condicional falso: se saltan las lineas hasta D0 / D2
    size_t loop_start = 0; uint32_t loop_left = 0;
    size_t n = w.size() / 2;
    for (size_t i = 0; i < n; i++) {
        uint32_t a = w[2 * i], b = w[2 * i + 1];
        uint32_t type = a >> 28;
        size_t extra = 0;                                       // lineas de datos que acompanan (tipo E)
        if (type == 0xE) extra = (b + 7) / 8;
        if (skipping) {
            if (type == 0xD && (((a >> 24) & 0xF) == 0x0 || ((a >> 24) & 0xF) == 0x2)) {
                skipping = false;                               // D0 cierra el condicional; D2 tambien, y ademas se EJECUTA (reinicia offset/datos)
                if (((a >> 24) & 0xF) == 0x0) continue;
            } else { i += extra; continue; }
        }
        uint32_t addr = (a & 0x0FFFFFFF) + offset;
        switch (type) {
        case 0x0: m.w32(addr, b); break;
        case 0x1: m.w16(addr, b & 0xFFFF); break;
        case 0x2: m.w8(addr, b & 0xFF); break;
        case 0x3: if (!(b > m.r32(addr))) skipping = true; break;
        case 0x4: if (!(b < m.r32(addr))) skipping = true; break;
        case 0x5: if (!(b == m.r32(addr))) skipping = true; break;
        case 0x6: if (!(b != m.r32(addr))) skipping = true; break;
        case 0x7: case 0x8: case 0x9: case 0xA: {
            uint32_t z = b >> 16, y = b & 0xFFFF;
            uint32_t v = m.r16(addr) & ~z & 0xFFFF;
            bool ok = (type == 7) ? y > v : (type == 8) ? y < v : (type == 9) ? y == v : y != v;
            if (!ok) skipping = true;
            break;
        }
        case 0xB: offset = m.r32(a & 0x0FFFFFFF); break;
        case 0xC: loop_left = b; loop_start = i + 1; break;
        case 0xD: {
            uint32_t sub = (a >> 24) & 0xF;
            switch (sub) {
            case 0x0: break;                                  // fin de condicional
            case 0x1:                                         // fin de bucle
                if (loop_left > 1) { loop_left--; i = loop_start - 1; } else loop_left = 0;
                break;
            case 0x2: offset = 0; data = 0; loop_left = 0; break;
            case 0x3: offset = b; break;
            case 0x4: data += b; break;
            case 0x5: data = b; break;
            case 0x6: m.w32(b + offset, data); offset += 4; break;
            case 0x7: m.w16(b + offset, data & 0xFFFF); offset += 2; break;
            case 0x8: m.w8(b + offset, data & 0xFF); offset += 1; break;
            case 0x9: data = m.r32(b + offset); break;
            case 0xA: data = m.r16(b + offset); break;
            case 0xB: data = m.r8(b + offset); break;
            case 0xC: offset += b; break;
            default: break;
            }
            break;
        }
        case 0xE:
            for (uint32_t k = 0; k < b; k++) {
                if (2 * (i + 1) + k / 4 >= w.size()) break;
                uint32_t wd = w[2 * (i + 1) + k / 4];          // los datos van en palabras little-endian
                m.w8(addr + k, (wd >> (8 * (k % 4))) & 0xFF);
            }
            break;
        default: break;                                       // F y otros: sin soporte
        }
        i += extra;
    }
}

int CheatEngine::add(const std::string& name, const std::vector<uint32_t>& words, bool enabled) {
    Cheat c; c.name = name; c.words = words; c.enabled = enabled;
    cheats_.push_back(std::move(c));
    return (int)cheats_.size() - 1;
}
void CheatEngine::set_enabled(int id, bool on) {
    if (id >= 0 && (size_t)id < cheats_.size()) cheats_[id].enabled = on;
}
void CheatEngine::apply_frame(MemIf& mem) {
    for (auto& c : cheats_) if (c.enabled) run_ar_code(c.words, mem);
}

}  // namespace nds
