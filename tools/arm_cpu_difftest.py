#!/usr/bin/env python3
"""Test diferencial del interprete ARM9 contra Unicorn (modelo ARM946E-S).
Uso: python arm_cpu_difftest.py libarmtest.so [n_arm] [n_thumb] [semilla]"""
import sys, random, ctypes, collections, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UcError
from unicorn.arm_const import *
from unicorn import arm_const

lib = ctypes.CDLL(sys.argv[1])
lib.t_new.restype = ctypes.c_void_p
for f in ("t_free", "t_load", "t_set", "t_run", "t_get", "t_data"): getattr(lib, f).argtypes = None
lib.t_data.restype = ctypes.POINTER(ctypes.c_uint8)
N_ARM = int(sys.argv[2]) if len(sys.argv) > 2 else 60000
N_THUMB = int(sys.argv[3]) if len(sys.argv) > 3 else 40000
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 1)

CODE, DATA = 0x02000000, 0x02EF0000
H = lib.t_new(1)
mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
mu.ctl_set_cpu_model(arm_const.UC_CPU_ARM_946)
mu.mem_map(CODE, 0x1000); mu.mem_map(DATA, 0x4000)
REGS = [UC_ARM_REG_R0 + i for i in range(13)] + [UC_ARM_REG_SP, UC_ARM_REG_LR]

def rand_reg():
    k = random.random()
    if k < 0.35: return DATA + 0x2000 + random.randrange(-0x400, 0x400)
    if k < 0.45: return DATA + 0x2000 + random.randrange(-0x400, 0x400) & ~3
    return random.choice([0, 1, 2, 3, 0xFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFF, 0x10000, random.getrandbits(32), random.getrandbits(32), random.getrandbits(5), random.getrandbits(8)])

def run_case(word, thumb, n=1, pair=None):
    regs = [rand_reg() for _ in range(15)]
    cpsr = (random.getrandbits(5) << 27) | 0x1F | (0x20 if thumb else 0)    # NZCVQ aleatorios, modo SYS
    data = bytes(random.getrandbits(8) for _ in range(0x4000))
    code = struct.pack('<HH', word & 0xFFFF, pair & 0xFFFF) if thumb and pair is not None else (struct.pack('<H', word) if thumb else struct.pack('<I', word))
    code = code + b'\0' * 8
    pc = CODE + 0x100
    # --- Unicorn ---
    mu.mem_write(CODE, bytes(0x1000)); mu.mem_write(CODE + 0x100, code); mu.mem_write(DATA, data)
    mu.ctl_remove_cache(CODE + 0x100, CODE + 0x120)         # Unicorn cachea el codigo traducido
    mu.reg_write(UC_ARM_REG_CPSR, cpsr)                # primero el modo (cambia el banco), luego los registros
    for rid, v in zip(REGS, regs): mu.reg_write(rid, v)
    try:
        mu.emu_start(pc | (1 if thumb else 0), CODE + 0xF00, count=(1 if pair is not None else n))
    except UcError:
        return None
    u_regs = [mu.reg_read(r) for r in REGS] + [mu.reg_read(UC_ARM_REG_PC)]
    u_cpsr = mu.reg_read(UC_ARM_REG_CPSR)
    u_data = bytes(mu.mem_read(DATA, 0x4000))
    # --- mi interprete ---
    full = (ctypes.c_uint32 * 16)(*(regs + [pc | (1 if thumb else 0)]))
    buf = (ctypes.c_uint8 * 0x1000)(); 
    cb = (ctypes.c_uint8 * len(code)).from_buffer_copy(code)
    # el codigo va en CODE+0x100: cargamos un bloque de 0x1000 con el codigo desplazado
    blk = bytes(0x100) + code
    cbuf = (ctypes.c_uint8 * len(blk)).from_buffer_copy(blk)
    dbuf = (ctypes.c_uint8 * 0x4000).from_buffer_copy(data)
    lib.t_load(ctypes.c_void_p(H), cbuf, len(blk), dbuf)
    lib.t_set(ctypes.c_void_p(H), full, ctypes.c_uint32(cpsr))
    err = lib.t_run(ctypes.c_void_p(H), n)
    if err: return "mi_error_de_bus"
    if lib.t_unaligned(ctypes.c_void_p(H)): return "desalineado"
    out = (ctypes.c_uint32 * 16)(); oc = ctypes.c_uint32()
    lib.t_get(ctypes.c_void_p(H), out, ctypes.byref(oc))
    m_regs = list(out); m_cpsr = oc.value
    m_data = bytes(lib.t_data(ctypes.c_void_p(H))[:0x4000])
    diffs = []
    names = [f"r{i}" for i in range(13)] + ["sp", "lr", "pc"]
    for i in range(16):
        a, b = m_regs[i], u_regs[i]
        if i == 15 and not (u_cpsr & 0x20): a, b = a & ~3, b & ~3
        if a != b: diffs.append((names[i], m_regs[i], u_regs[i]))
    if (m_cpsr ^ u_cpsr) & 0xF80000FF: diffs.append(("cpsr", m_cpsr, u_cpsr))
    if m_data != u_data: diffs.append(("memoria", 0, 0))
    return diffs

def classify_arm(w):
    t = (w >> 25) & 7
    if t < 2:
        if (w & 0x90) == 0x90 and t == 0: return "mul/halfword/swp"
        if (w & 0x01900000) == 0x01000000: return "misc(bx,clz,qadd,smul,mrs,msr)"
        return "dataproc"
    return {2: "ldr/str imm", 3: "ldr/str reg", 4: "ldm/stm", 5: "branch", 6: "copro", 7: "swi/mcr"}[t]

def gen_arm():
    w = random.getrandbits(32)
    if random.random() < 0.8: w = (w & 0x0FFFFFFF) | 0xE0000000          # casi siempre condicion AL
    elif random.random() < 0.5: w = (w & 0x0FFFFFFF) | (random.randrange(15) << 28)
    t = random.random()
    # sesgo a clases utiles
    if t < 0.3: w = (w & ~(7 << 25)) | (random.choice([0, 1]) << 25)
    elif t < 0.5: w = (w & ~(7 << 25)) | (random.choice([2, 3]) << 25)
    elif t < 0.65: w = (w & ~(7 << 25)) | (4 << 25)
    return w

def skip_arm(w):
    if (w >> 28) == 0xF and ((w >> 25) & 7) != 5: return True
    cls = classify_arm(w)
    if cls in ("copro", "swi/mcr"): return True
    rd, rn = (w >> 12) & 15, (w >> 16) & 15
    t = (w >> 25) & 7
    if (t == 1 or (t == 0 and (w & 0x90) != 0x90)) and (w & 0x01900000) != 0x01000000 and rd == 15 and (w & (1 << 20)): return True   # S con Rd=PC en SYS: indefinido
    if t == 4 and (w & (1 << 22)): return True                                           # LDM/STM con S (banco de usuario): indefinido en modo SYS
    if t == 0 and (w & 0x90) == 0x90 and not (w & (1 << 20)) and ((w >> 5) & 3) in (2, 3) and rd >= 14: return True   # LDRD/STRD con Rd=LR
    if cls in ("misc(bx,clz,qadd,smul,mrs,msr)", "mul/halfword/swp") and (((w >> 16) & 15) == 15 or rd == 15) and not (w & 0x0FFFFFD0) in (0x012FFF10,): 
        if (w & 0x0FFFFFF0) not in (0x012FFF10, 0x012FFF30): return True    # destino/fuente PC en mul/dsp/mrs: dependiente de implementacion
    # stores con PC o shifts por registro con PC: valor de PC almacenado/leido depende de la implementacion
    if t in (2, 3) and not (w & (1 << 20)) and rd == 15: return True
    if t == 4 and not (w & (1 << 20)) and (w & 0x8000): return True
    if t == 0 and (w & 0x90) == 0x10 and ((w & 15) == 15 or rn == 15): return True       # registro-shift con PC
    if t == 0 and (w & 0x90) == 0x90 and (w & 0x60) and not (w & (1 << 20)) and rd == 15: return True
    if t == 4:                                                                           # base en la lista (comportamiento distinto por modelo)
        if (w & (1 << 21)) and (w & (1 << rn)): return True
        if rn == 15: return True
    if t in (2, 3, 0) and rn == 15 and ((w & (1 << 21)) or not (w & (1 << 24))): return True   # writeback con PC base
    # LDR/STR con Rn==Rd y writeback: UNPREDICTABLE
    if t in (2, 3) and rn == rd and (w & (1 << 21) or not (w & (1 << 24))): return True
    # LDRH/LDRSB etc con writeback y Rn==Rd
    if t == 0 and (w & 0x90) == 0x90 and rn == rd and ((w & (1 << 21)) or not (w & (1 << 24))): return True
    # accesos de media palabra/doble con direccion desalineada: depende de modelo
    return False

stats = collections.Counter(); bad = collections.defaultdict(list); tested = collections.Counter()
for _ in range(N_ARM):
    w = gen_arm()
    if skip_arm(w): continue
    r = run_case(w, False)
    cls = classify_arm(w)
    if r is None or isinstance(r, str): stats["descartado:" + ("unicorn" if r is None else r)] += 1; continue
    tested[cls] += 1
    if r: stats["DIFERENCIA"] += 1; bad[cls].append((w, r))
    else: stats["ok"] += 1

print("== ARM ==", dict(stats))
print("   casos comparados por clase:", dict(tested))
for cls, lst in bad.items():
    print(f"   [{cls}] {len(lst)} diferencias; ejemplos:")
    for w, r in lst[:4]:
        print(f"      {w:08x}: {[(n, hex(a), hex(b)) for n, a, b in r][:3]}")

# ---- Thumb ----
def gen_thumb():
    k = random.random()
    h = random.getrandbits(16)
    if k < 0.6: h = (h & 0x07FF) | (random.randrange(0, 0x18) << 11)        # formatos 0-0x17 (ALU, ldr/str, push/pop...)
    elif k < 0.75: h = (h & 0x07FF) | (random.choice([0x18, 0x19]) << 11)   # ldmia/stmia
    elif k < 0.85: h = 0xD000 | (h & 0x0FFF)                                  # bcond
    elif k < 0.92: h = 0xE000 | (h & 0x07FF)                                  # b
    return h

stats2 = collections.Counter(); bad2 = collections.defaultdict(list); t2 = collections.Counter()
for _ in range(N_THUMB):
    h = gen_thumb()
    top = h >> 11
    if h >> 8 == 0xDF or (h >> 8) in (0xB0 - 0xB0,) and False: continue
    if (h & 0xFF00) == 0xDF00 or (h & 0xFF00) == 0xBE00 or (h & 0xF800) == 0xF800 or (h & 0xF800) == 0xF000 or (h & 0xF800) == 0xE800: continue
    if (h & 0xF000) == 0xD000 and (h & 0x0F00) == 0x0E00: continue
    if (h & 0xF000) == 0xB000 and not ((h & 0xFF00) == 0xB000 or (h & 0xF600) == 0xB400): continue   # el resto (CBZ, IT, hints...) no existe en ARMv5
    if top == 8 and (h & 0x0400) and (((h >> 8) & 3) in (0, 1, 2)) and ((h & 7) | ((h >> 4) & 8)) == 15 and False: continue
    if top == 8 and (h & 0x0400) and ((h >> 8) & 3) == 3 and (h & 7): continue             # BX con bits SBZ
    if top in (24, 25) and (h & 0xFF) == 0: continue                                        # lista vacia (depende del modelo)
    if top in (24, 25) and (h & (1 << (8 + 0))) and False: continue
    if top in (24, 25):
        rn = (h >> 8) & 7
        if (h >> 11) == 25 and (h & (1 << rn)): continue
    r = run_case(h, True)
    cls = f"thumb fmt {top:02d}"
    if r is None or isinstance(r, str): stats2["descartado"] += 1; continue
    t2[top] += 1
    if r: stats2["DIFERENCIA"] += 1; bad2[top].append((h, r))
    else: stats2["ok"] += 1
print("== Thumb ==", dict(stats2))
for top, lst in bad2.items():
    print(f"   [formato {top}] {len(lst)} diferencias; ejemplos:")
    for h, r in lst[:4]: print(f"      {h:04x}: {[(n, hex(a), hex(b)) for n, a, b in r][:3]}")

# ---- Thumb: pares BL / BLX (dos halfwords) ----
stats3 = collections.Counter(); bad3 = []
for _ in range(8000):
    hi = random.choice([0xF000, 0xF7FF])                       # desplazamientos pequenos: el destino debe caer en memoria mapeada
    lo = random.choice([0xF800, 0xE800]) | random.getrandbits(10)
    if (lo & 0xF800) == 0xE800: lo &= ~1
    r = run_case(hi, True, n=2, pair=lo)
    if r is None or isinstance(r, str): stats3["descartado"] += 1; continue
    if r: stats3["DIFERENCIA"] += 1; bad3.append((hi, lo, r))
    else: stats3["ok"] += 1
print("== Thumb BL/BLX ==", dict(stats3))
for hi, lo, r in bad3[:4]: print(f"      {hi:04x} {lo:04x}: {[(n, hex(a), hex(b)) for n, a, b in r][:3]}")
