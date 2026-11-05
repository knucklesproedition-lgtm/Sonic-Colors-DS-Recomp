// ds_system.h - La consola: dos CPU (ARM9 + ARM7), memoria, E/S, IRQ, temporizadores, DMA,
// FIFO entre CPU, lectura de la ROM por "cartucho" y BIOS emulada por software (HLE).
// Todavia SIN: GPU 2D/3D, audio, pantalla tactil/SPI, WiFi (ver README).
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <vector>
#include "arm_cpu.h"
#include "gpu3d.h"

namespace nds {

class Console;

struct ConsoleBus : Bus {
    Console* c; int id;                       // id 0 = ARM9, 1 = ARM7
    ConsoleBus(Console* con, int i) : c(con), id(i) {}
    uint32_t read32(uint32_t a) override; uint32_t read16(uint32_t a) override; uint32_t read8(uint32_t a) override;
    void write32(uint32_t a, uint32_t v) override; void write16(uint32_t a, uint32_t v) override; void write8(uint32_t a, uint32_t v) override;
    bool swi(Cpu& cpu, uint32_t n) override;
    uint32_t cp15_read(uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2) override;
    void cp15_write(uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2, uint32_t v) override;
};

struct Timer { uint16_t reload = 0, ctrl = 0, counter = 0; uint32_t acc = 0; };
struct Dma { uint32_t sad = 0, dad = 0, cnt = 0; uint16_t ctrl = 0; uint32_t isad = 0, idad = 0, icnt = 0; bool armed_card = false; };

class Console {
public:
    Console();
    bool load_rom(const std::vector<uint8_t>& rom);       // copia cabecera, carga ARM9/ARM7 (tal cual, sin descomprimir)
    void run_units(uint64_t units);                       // 1 unidad = 1 ciclo de bus (33,5 MHz): ARM9 hace 2 instr, ARM7 1
    // Estado visible para depuracion / front-end
    ConsoleBus bus9{this, 0}, bus7{this, 1};
    Cpu cpu9{Cpu::ARM9_V5TE, &bus9}, cpu7{Cpu::ARM7_V4T, &bus7};
    uint32_t keys = 0;
    void set_touch(int x, int y, bool down) { touch_x_ = x < 0 ? 0 : x > 255 ? 255 : x; touch_y_ = y < 0 ? 0 : y > 191 ? 191 : y; touch_down_ = down; }   // coordenadas de la pantalla tactil 0..255, 0..191                                    // mascara nds::Key (1 = pulsado)
    uint64_t frame = 0;
    // Memoria
    std::vector<uint8_t> rom;
    uint8_t main_ram[0x400000] = {}, shared_wram[0x8000] = {}, wram7[0x10000] = {};
    uint8_t itcm[0x8000] = {}, dtcm[0x4000] = {}, vram[0xA4000] = {}, pal[0x800] = {}, oam[0x800] = {};
    uint8_t bios9[0x1000] = {}, bios7[0x1000] = {};
    // Video: bancos de VRAM mapeados, motor 3D y las dos pantallas finales (RGBA, byte 0 = R)
    Gpu3D gpu3d;
    void set_graphics(int scale, int filter) { gfx_scale = scale < 1 ? 1 : scale > 4 ? 4 : scale; gfx_filter = filter; gpu3d.set_scale(gfx_scale); }
    int gfx_scale = 1, gfx_filter = 2;
    const uint32_t* screen_pixels(int which) const { return fb[which].data(); }   // 0 = pantalla superior, 1 = inferior
    void render_now() { render_frame(); }                  // fuerza la composicion (tests)
    int screen_w() const { return 256 * gfx_scale; }
    int screen_h() const { return 192 * gfx_scale; }
    std::map<uint32_t, uint64_t>* rd_log[2] = {nullptr, nullptr};   // depuracion: histograma de direcciones leidas por CPU
    uint64_t video_frames = 0, unsupported_bg = 0, unsupported_obj = 0;
    // Diagnostico
    std::map<uint32_t, uint64_t> io_unknown[2];           // "cpu<<32|direccion" -> veces
    std::map<uint32_t, uint64_t> swi_unhandled[2];
    uint64_t irq_taken[2] = {0, 0};
    std::map<uint32_t, uint64_t> pc_samples[2];            // muestreo de PC para ver en que bucle se atasca
    std::map<uint32_t, uint64_t> card_cmd_count;           // comando del cartucho -> veces
    std::map<uint32_t, uint64_t> card_reg_access[2];       // accesos a 0x040001A0-0x040001BF por CPU (direccion|0x10000 si escritura)
    uint64_t fifo_pushed[2] = {0, 0}, fifo_popped[2] = {0, 0};
    std::vector<std::pair<int,uint32_t>>* fifo_log = nullptr;   // depuracion: (cpu emisor, palabra)
    std::map<uint32_t, uint64_t> fw_reads;                  // depuracion: direccion de inicio de cada lectura del firmware -> veces
    // Memoria de guardado del cartucho (EEPROM de 64 Kbit = 8 KB por AUXSPI). Persistir con backup() / set_backup().
    const std::vector<uint8_t>& backup() const { return backup_; }
    void set_backup(const std::vector<uint8_t>& d) { if (d.size() == backup_.size()) backup_ = d; }
    bool backup_dirty = false;                              // true si el juego escribio algo desde la ultima vez que se guardo
    std::vector<std::pair<uint16_t,uint8_t>>* auxspi_log = nullptr;   // depuracion: (AUXSPICNT, byte enviado)
    struct DmaEv { int cpu, ch; uint32_t src, dst, cnt; uint16_t ctrl; uint64_t seq; uint32_t pc, lr; };
    std::vector<DmaEv> dma_log;                              // depuracion: DMAs activados (se guardan los ultimos)
    uint64_t dma_seq_ = 0;
    uint64_t card_blocks = 0, spi_transfers[3] = {0, 0, 0};                               // lecturas de la ROM por "cartucho"

private:
    friend struct ConsoleBus;
    uint8_t* ptr(int cpu, uint32_t a, bool write);
    uint32_t mem_read(int cpu, uint32_t a, int size);
    void mem_write(int cpu, uint32_t a, uint32_t v, int size);
    uint16_t io_r16(int cpu, uint32_t a);
    void io_w16(int cpu, uint32_t a, uint16_t v);
    void io_w32(int cpu, uint32_t a, uint32_t v);
    uint32_t io_r32(int cpu, uint32_t a);
    void raise_irq(int cpu, uint32_t bit) { IF[cpu] |= (1u << bit); }
    void update_irq_lines();
    void tick_timers(int cpu, uint32_t units);
    void timer_overflow(int cpu, int t);
    void scanline_step();
    void hblank_start();
    void dma_vblank(int cpu); void dma_hblank(int cpu);
    void div_update(); void sqrt_update();
    void io_w8(int cpu, uint32_t a, uint8_t v);
    uint64_t steps_ = 0;
    void dma_write_ctrl(int cpu, int ch, uint16_t v);
    void dma_run(int cpu, int ch);
    void card_start(int cpu);
    void ipc_send(int cpu, uint32_t v);
    uint32_t ipc_recv(int cpu);
    uint16_t ipc_cnt(int cpu);
    void ipc_cnt_write(int cpu, uint16_t v);
    bool hle_swi(int cpu, Cpu& c, uint32_t n);
    void make_bios_stubs();
    void update_fast();
    void make_firmware();
    void spi_transfer(uint8_t out);
    // video
    void rebuild_vram_map();
    void render_frame();
    void render_engine(int eng, std::vector<uint32_t>& out);
    uint8_t vramcnt_[9] = {};
    uint8_t zero_page_[0x4000] = {};
    uint8_t *map_bga_[32] = {}, *map_obja_[16] = {}, *map_bgb_[8] = {}, *map_objb_[8] = {}, *map_lcdc_[41] = {}, *map_tex_[32] = {}, *map_texpal_[6] = {};
    const uint8_t* tex_cpages_[32] = {}; const uint8_t* pal_cpages_[6] = {};
    std::vector<uint32_t> fb[2];

    uint8_t io[2][0x2000] = {};
public:
    uint32_t IME[2] = {0, 0}, IE[2] = {0, 0}, IF[2] = {0, 0};
    // true cuando el crt0 del ARM9 ya descomprimio su codigo (el SDK pone a 0 'compressed_static_end' al terminar).
    // Los trucos y mods solo deben aplicarse a partir de aqui: antes, la memoria aun contiene datos comprimidos.
    bool arm9_booted();
    uint8_t gxmode() const { return gx_irq_mode_; }
    bool gxpending() const { return gx_irq_pending_; }
    const Dma& dma_state(int cpu, int ch) const { return dma[cpu][ch]; }
    const Timer& timer(int cpu, int t) const { return timers[cpu][t]; }
    uint16_t dispstat_(int c) const { return dispstat[c]; }
    size_t fifo_size(int c) const { return fifo[c].size(); }
private:
    uint8_t wramcnt = 3;
    // pantalla / tiempo
    uint32_t line_cycles = 0, vcount = 0; uint16_t dispstat[2] = {0, 0};
    // TCM del ARM9
    uint32_t cp15_ctrl = 0x00052078, dtcm_reg = 0x027C000A, itcm_reg = 0x00000020;
    bool itcm_on = true, dtcm_on = true; uint32_t itcm_size = 0x8000 * 0 + 0x2000000, dtcm_base = 0x027C0000, dtcm_size = 0x4000;
    Timer timers[2][4]; Dma dma[2][4];
    // IPC
    uint16_t ipcsync_out[2] = {0, 0}; uint16_t ipcsync_cnt[2] = {0, 0};
    std::deque<uint32_t> fifo[2];                         // fifo[i] = cola enviada por la CPU i
    uint16_t fifocnt[2] = {0x0101, 0x0101}; uint32_t fifo_last[2] = {0, 0};
    // cartucho
    uint8_t card_cmd[8] = {}; uint32_t romctrl = 0; uint16_t auxspicnt = 0; std::deque<uint32_t> card_words; uint32_t card_total = 0;
    int card_owner = 0;
    // divisor/raiz (ARM9)
    // SPI del ARM7: 0 = energia, 1 = firmware, 2 = pantalla tactil
    uint16_t spicnt_ = 0; uint8_t spi_out_ = 0;
    uint8_t pm_regs_[8] = {}; int pm_state_ = 0, pm_reg_ = 0;
    std::vector<uint8_t> fw_; int fw_state_ = 0; uint32_t fw_addr_ = 0, fw_cmd_ = 0; bool fw_wel_ = false;   // fw_wel_ = 'write enable latch' de la flash
    int touch_state_ = 0; uint16_t touch_val_ = 0; int touch_x_ = 0, touch_y_ = 0; bool touch_down_ = false;
    // chip de guardado
    std::vector<uint8_t> backup_ = std::vector<uint8_t>(8192, 0xFF); bool bk_wel_ = false; int bk_state_ = 0; uint32_t bk_addr_ = 0; uint8_t bk_cmd_ = 0, aux_out_ = 0;
    void auxspi_transfer(uint8_t out);
public:
    uint64_t gx_raised = 0, gx_stat_writes = 0; uint16_t gx_last_stat = 0; std::map<uint32_t,uint64_t> if_clears[2];
private:
    uint32_t boot_probe_addr_ = 0; uint8_t boot_probe_[32] = {}; bool booted_ = false;   // sonda: ultimos bytes que escribe el descompresor del ARM9
    uint8_t gx_irq_mode_ = 0; bool gx_irq_pending_ = false; uint32_t gx_delay_ = 0;   // retardo hasta que la cola 'termina de procesar'   // GXSTAT bits 30-31: 1 = cola menos de la mitad, 2 = vacia
    uint64_t div_numer = 0, div_denom = 0, div_res = 0, div_rem = 0; uint16_t divcnt = 0; uint32_t sqrt_param64[2] = {0, 0}; uint32_t sqrt_res = 0; uint16_t sqrtcnt = 0;
};

}  // namespace nds
