#!/usr/bin/env python3
"""Autotest del lifter: compila las funciones traducidas y las compara con Unicorn (ARM real)."""
import sys, struct, random, subprocess, os, tempfile, ctypes
sys.path.insert(0, os.path.dirname(__file__))
import arm_lift as L
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_MEM_UNMAPPED
from unicorn.arm_const import *

data = open(sys.argv[1], 'rb').read(); base = 0x02000000
lf = L.Lifter(data, base)
end_lim = 0x800 + 0xB0000
fns = L.find_functions(data, base, end_lim)

# 1) funciones 100% traducidas y "puras" (sin llamadas ni memoria): comparables sin entorno
pure = []
allfull = []
for s, e in zip(fns, fns[1:] + [base + end_lim]):
    code, ok, tot = lf.lift_function(s, e)
    if ok != tot: continue
    allfull.append(code)
    if "fn_" not in code.replace(f"void fn_{s:08x}", "") and tot <= 150:
        pure.append((s, e, code))

# 2) comprobar que TODAS las completas compilan
src = L.PRELUDE + "\n".join(allfull) + "\n"
d = tempfile.mkdtemp(); cpath = os.path.join(d, "all.c")
open(cpath, "w").write(src)
r = subprocess.run(["gcc", "-fsyntax-only", "-w", cpath], capture_output=True, text=True)
print(f"[compilacion] {len(allfull)} funciones C -> {'OK, sin errores' if r.returncode == 0 else 'ERRORES'}")
if r.returncode: print(r.stderr[:1500]); sys.exit(1)

HARNESS = r"""
#include <stdint.h>
#include <string.h>
static uint8_t code[0x120000]; static uint8_t stk[0x20000];
#define CB 0x02000000u
#define SB 0x02EF0000u
int err = 0;
void set_code(const uint8_t* p, uint32_t n) { memcpy(code, p, n); }
uint8_t* get_stk(void) { return stk; }
static uint8_t* at(uint32_t a, int n) {
    if (a >= SB && a + n <= SB + sizeof stk) return stk + (a - SB);
    if (a >= CB && a + n <= CB + sizeof code) return code + (a - CB);
    err = 1; return 0; }
uint32_t mem_read32(uint32_t a){ uint8_t*p=at(a,4); uint32_t v=0; if(p) memcpy(&v,p,4); return v; }
uint32_t mem_read16(uint32_t a){ uint8_t*p=at(a,2); uint16_t v=0; if(p) memcpy(&v,p,2); return v; }
uint32_t mem_read8 (uint32_t a){ uint8_t*p=at(a,1); return p?*p:0; }
void mem_write32(uint32_t a,uint32_t v){ uint8_t*p=at(a,4); if(p) memcpy(p,&v,4); }
void mem_write16(uint32_t a,uint32_t v){ uint8_t*p=at(a,2); uint16_t w=v; if(p) memcpy(p,&w,2); }
void mem_write8 (uint32_t a,uint32_t v){ uint8_t*p=at(a,1); if(p) *p=(uint8_t)v; }
void TODO_UNIMPL(uint32_t a){ err = 2; }
"""
hp = os.path.join(d, "harness.c"); open(hp, "w").write(HARNESS)

# 3) test diferencial
reg_ids = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R4, UC_ARM_REG_R5,
           UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12]
random.seed(1234)
tested = mismatches = 0
for s, e, code in pure:
    # construir .so con solo esta funcion
    cp = os.path.join(d, f"f{s:08x}.c"); sp = cp[:-2] + ".so"
    open(cp, "w").write(L.PRELUDE.replace("uint32_t mem_read32(uint32_t); uint32_t mem_read16(uint32_t); uint32_t mem_read8(uint32_t);\nvoid mem_write32(uint32_t, uint32_t); void mem_write16(uint32_t, uint32_t); void mem_write8(uint32_t, uint32_t);\nvoid TODO_UNIMPL(uint32_t addr);", "uint32_t mem_read32(uint32_t); uint32_t mem_read16(uint32_t); uint32_t mem_read8(uint32_t);\nvoid mem_write32(uint32_t, uint32_t); void mem_write16(uint32_t, uint32_t); void mem_write8(uint32_t, uint32_t);\nvoid TODO_UNIMPL(uint32_t addr);") + code + "\n")
    if subprocess.run(["gcc", "-shared", "-fPIC", "-w", "-O1", cp, hp, "-o", sp]).returncode: continue
    lib = ctypes.CDLL(sp)
    class Cpu(ctypes.Structure): _fields_ = [("r", ctypes.c_uint32 * 16), ("N", ctypes.c_uint32), ("Z", ctypes.c_uint32), ("C", ctypes.c_uint32), ("V", ctypes.c_uint32)]
    fn = getattr(lib, f"fn_{s:08x}"); fn.argtypes = [ctypes.POINTER(Cpu)]
    lib.get_stk.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.set_code(data, len(data))
    errv = ctypes.c_int.in_dll(lib, "err")
    for _ in range(40):
        inp = [random.choice([0, 1, 0xFFFFFFFF, 0x80000000, random.getrandbits(32), random.getrandbits(8), 0x02EF8000 + random.randrange(0, 0x4000, 4), 0x02EF8000 + random.randrange(0, 0x4000)]) for _ in range(13)]
        # emulador
        mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        mu.mem_map(base, 0x120000)
        mu.mem_write(base, data)
        for rid, v in zip(reg_ids, inp): mu.reg_write(rid, v)
        mu.reg_write(UC_ARM_REG_SP, 0x02F00000); mu.reg_write(UC_ARM_REG_LR, 0x0DEAD000)
        mu.mem_map(0x02EF0000, 0x20000)
        stk0 = bytes(random.getrandbits(8) for _ in range(0x20000))
        mu.mem_write(0x02EF0000, stk0)
        ctypes.memmove(lib.get_stk(), stk0, 0x20000)
        errv.value = 0
        try:
            mu.emu_start(s, 0x0DEAD000, count=2000)
        except Exception:
            continue
        if mu.reg_read(UC_ARM_REG_PC) != 0x0DEAD000 and mu.reg_read(UC_ARM_REG_PC) & ~1 != 0x0DEAD000: continue
        # C
        c = Cpu(); 
        for i, v in enumerate(inp): c.r[i] = v
        c.r[13] = 0x02F00000; c.r[14] = 0x0DEAD000
        fn(ctypes.byref(c))
        if errv.value: continue        # acceso fuera de la memoria simulada o instruccion no traducida
        tested += 1
        exp = [mu.reg_read(r) for r in reg_ids[:12]]   # r0-r11 (r12 puede variar por ABI)
        got = [c.r[i] for i in range(12)]
        got_stk = bytes(lib.get_stk()[i] for i in range(0x20000))
        if exp != got or bytes(mu.mem_read(0x02EF0000, 0x20000)) != got_stk or c.r[13] != mu.reg_read(UC_ARM_REG_SP):
            mismatches += 1
            if mismatches <= 3:
                print(f"  DIFERENCIA en fn_{s:08x}: esperado {[hex(x) for x in exp[:4]]} obtenido {[hex(x) for x in got[:4]]}")
print(f"[diferencial] funciones puras probadas: {len(pure)}, ejecuciones comparadas: {tested}, diferencias: {mismatches}")
