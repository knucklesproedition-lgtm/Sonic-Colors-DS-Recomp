#!/usr/bin/env python3
"""
arm_lift.py - PROTOTIPO de lifter ARMv5 (modo ARM) -> C.

Uso:
  python arm_lift.py arm9.bin 0x02000000 stats            # cobertura de instrucciones
  python arm_lift.py arm9.bin 0x02000000 lift 0x02000A78  # traduce una funcion

Estado: traduce ALU (mov/add/sub/and/orr/eor/bic/cmp/tst...), flags NZCV,
ejecucion condicional, ramas b/bl/bx, ldr/str (w/b/h), push/pop y mul.
Lo demas se emite como TODO_UNIMPL(addr) para ir ampliandolo.
No cubre aun: Thumb, ldm/stm generales, coprocesador, shifter-carry exacto.
"""
import sys, struct
import capstone as cs
from capstone import arm as A

COND = {
    A.ARM_CC_EQ: "c->Z", A.ARM_CC_NE: "!c->Z", A.ARM_CC_HS: "c->C", A.ARM_CC_LO: "!c->C",
    A.ARM_CC_MI: "c->N", A.ARM_CC_PL: "!c->N", A.ARM_CC_VS: "c->V", A.ARM_CC_VC: "!c->V",
    A.ARM_CC_HI: "(c->C && !c->Z)", A.ARM_CC_LS: "(!c->C || c->Z)",
    A.ARM_CC_GE: "(c->N == c->V)", A.ARM_CC_LT: "(c->N != c->V)",
    A.ARM_CC_GT: "(!c->Z && c->N == c->V)", A.ARM_CC_LE: "(c->Z || c->N != c->V)",
}
ALIAS = {'sb': 9, 'sl': 10, 'fp': 11, 'ip': 12, 'sp': 13, 'lr': 14, 'pc': 15}

class Lifter:
    def __init__(self, data, base):
        self.data, self.base = data, base
        self.md = cs.Cs(cs.CS_ARCH_ARM, cs.CS_MODE_ARM)
        self.md.detail = True

    def regnum(self, insn, reg):
        n = insn.reg_name(reg)
        return ALIAS[n] if n in ALIAS else int(n[1:])

    def R(self, insn, reg):
        n = self.regnum(insn, reg)
        return f"0x{(insn.address + 8) & 0xFFFFFFFF:x}u" if n == 15 else f"c->r[{n}]"

    def op2(self, insn, op):
        if op.type == A.ARM_OP_IMM:
            return f"0x{op.imm & 0xFFFFFFFF:x}u"
        v = self.R(insn, op.reg)
        t, s = op.shift.type, op.shift.value
        if t == A.ARM_SFT_INVALID or (t == A.ARM_SFT_LSL and s == 0): return v
        if t == A.ARM_SFT_LSL: return f"({v} << {s})"
        if t == A.ARM_SFT_LSR: return "0u" if s >= 32 else f"({v} >> {s})"
        if t == A.ARM_SFT_ASR: return f"((uint32_t)((int32_t){v} >> {min(s, 31)}))"
        if t == A.ARM_SFT_ROR: return f"((({v}) >> {s}) | (({v}) << {32 - s}))"
        if t == A.ARM_SFT_RRX: raise NotImplementedError
        # Desplazamiento por registro: cantidad = byte bajo de Rs; >=32 => 0 (lsl/lsr) o relleno de signo (asr)
        sr = f"(c->r[{self.regnum(insn, s)}] & 0xFFu)"
        if t == A.ARM_SFT_LSL_REG: return f"({sr} >= 32 ? 0u : ({v} << {sr}))"
        if t == A.ARM_SFT_LSR_REG: return f"({sr} >= 32 ? 0u : ({v} >> {sr}))"
        if t == A.ARM_SFT_ASR_REG: return f"((uint32_t)((int32_t){v} >> ({sr} >= 32 ? 31 : {sr})))"
        if t == A.ARM_SFT_ROR_REG: return f"(({sr} & 31) == 0 ? {v} : ((({v}) >> ({sr} & 31)) | (({v}) << (32 - ({sr} & 31)))))"
        raise NotImplementedError

    def flags_logic(self, res):
        return f"c->N = {res} >> 31; c->Z = ({res}) == 0;"

    def lift_insn(self, insn):
        m, ops = insn.mnemonic, insn.operands
        base = m
        cc = insn.cc
        for suf in ("eq","ne","hs","lo","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","cs","cc"):
            pass
        S = insn.update_flags
        mn = insn.id
        a = insn.address
        out = None
        try:
            id_ = insn.id
            name = insn.insn_name()
            # Escritura en PC: salto indirecto. mov pc, lr = retorno; el resto (tablas de switch) pendiente.
            if (name in ("mov","mvn","and","orr","eor","bic","add","sub","rsb","mul") and ops
                    and ops[0].type == A.ARM_OP_REG and self.regnum(insn, ops[0].reg) == 15):
                if (name == "mov" and len(ops) == 2 and ops[1].type == A.ARM_OP_REG
                        and self.regnum(insn, ops[1].reg) == 14 and ops[1].shift.type == A.ARM_SFT_INVALID):
                    out = "return;"
                    if cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID): out = f"if ({COND[cc]}) {{ {out} }}"
                    return out
                raise NotImplementedError
            if name in ("mov", "mvn") and len(ops) == 2:
                v = self.op2(insn, ops[1]); v = f"~{v}" if name == "mvn" else v
                d = self.R(insn, ops[0].reg)
                out = f"{d} = {v};"
                if S: out += " " + self.flags_logic(d)
            elif name in ("and", "orr", "eor", "bic") and len(ops) == 3:
                d = self.R(insn, ops[0].reg); n = self.R(insn, ops[1].reg); o = self.op2(insn, ops[2])
                e = {"and": f"{n} & {o}", "orr": f"{n} | {o}", "eor": f"{n} ^ {o}", "bic": f"{n} & ~{o}"}[name]
                out = f"{d} = {e};"
                if S: out += " " + self.flags_logic(d)
            elif name in ("add", "sub", "rsb") and len(ops) == 3:
                d = self.R(insn, ops[0].reg); n = self.R(insn, ops[1].reg); o = self.op2(insn, ops[2])
                x, y = (o, n) if name == "rsb" else (n, o)
                if name == "add":
                    out = f"{{ uint32_t x={x}, y={y}, r=x+y; "
                    if S: out += "c->C = r < x; c->V = (~(x^y) & (x^r)) >> 31; c->N = r>>31; c->Z = r==0; "
                else:
                    out = f"{{ uint32_t x={x}, y={y}, r=x-y; "
                    if S: out += "c->C = x >= y; c->V = ((x^y) & (x^r)) >> 31; c->N = r>>31; c->Z = r==0; "
                out += f"{d} = r; }}"
            elif name in ("cmp", "cmn", "tst", "teq") and len(ops) == 2:
                n = self.R(insn, ops[0].reg); o = self.op2(insn, ops[1])
                if name == "cmp":
                    out = f"{{ uint32_t x={n}, y={o}, r=x-y; c->C = x >= y; c->V = ((x^y) & (x^r)) >> 31; c->N = r>>31; c->Z = r==0; }}"
                elif name == "cmn":
                    out = f"{{ uint32_t x={n}, y={o}, r=x+y; c->C = r < x; c->V = (~(x^y) & (x^r)) >> 31; c->N = r>>31; c->Z = r==0; }}"
                else:
                    e = f"{n} & {o}" if name == "tst" else f"{n} ^ {o}"
                    out = f"{{ uint32_t r = {e}; {self.flags_logic('r')} }}"
            elif name == "mul" and len(ops) == 3:
                d = self.R(insn, ops[0].reg)
                out = f"{d} = {self.R(insn, ops[1].reg)} * {self.R(insn, ops[2].reg)};"
                if S: out += " " + self.flags_logic(d)
            elif name in ("b", "bl") and ops[0].type == A.ARM_OP_IMM:
                t = ops[0].imm & 0xFFFFFFFF
                out = (f"c->r[14] = 0x{a+4:x}u; fn_{t:08x}(c);" if name == "bl" else f"goto L_{t:08x};")
            elif name == "bx" and self.regnum(insn, ops[0].reg) == 14:
                out = "return;"
            elif name in ("ldr", "ldrb", "ldrh", "str", "strb", "strh") and ops[1].type == A.ARM_OP_MEM:
                mem = ops[1].mem
                if mem.base == 0 or self.regnum(insn, mem.base) == 15 and mem.index != 0: raise NotImplementedError
                basen = self.regnum(insn, mem.base)
                bv = f"0x{(a+8)&0xFFFFFFFF:x}u" if basen == 15 else f"c->r[{basen}]"
                if mem.index:
                    off = f"c->r[{self.regnum(insn, mem.index)}]"
                    if mem.lshift: off = f"({off} << {mem.lshift})"
                else:
                    off = f"0x{abs(mem.disp):x}u"; 
                sign = "-" if (ops[1].subtracted or (not mem.index and mem.disp < 0)) else "+"
                post = getattr(insn, "post_index", False)
                addr = bv if post else f"({bv} {sign} {off})"
                rt = self.R(insn, ops[0].reg)
                sz = {"ldr": "32", "str": "32", "ldrb": "8", "strb": "8", "ldrh": "16", "strh": "16"}[name]
                core = (f"{rt} = mem_read{sz}({addr});" if name.startswith("ldr") else f"mem_write{sz}({addr}, {rt});")
                wb = ""
                if post: wb = f" c->r[{basen}] {sign}= {off};"
                elif insn.writeback: wb = f" c->r[{basen}] = {addr};"
                out = core + wb
                if name.startswith("ldr") and self.regnum(insn, ops[0].reg) == 15: raise NotImplementedError
            elif name in ("push", "pop"):
                regs = [self.regnum(insn, o.reg) for o in ops]
                if name == "push":
                    out = f"c->r[13] -= {4*len(regs)}; " + " ".join(f"mem_write32(c->r[13] + {4*i}, c->r[{r}]);" for i, r in enumerate(regs))
                else:
                    if 15 in regs:
                        regs = [r for r in regs if r != 15]
                        out = " ".join(f"c->r[{r}] = mem_read32(c->r[13] + {4*i});" for i, r in enumerate(regs)) + f" c->r[13] += {4*(len(regs)+1)}; return;"
                    else:
                        out = " ".join(f"c->r[{r}] = mem_read32(c->r[13] + {4*i});" for i, r in enumerate(regs)) + f" c->r[13] += {4*len(regs)};"
        except NotImplementedError:
            out = None
        if out is None:
            return None
        if cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID):
            out = f"if ({COND[cc]}) {{ {out} }}"
        return out

    def disasm(self, start, end):
        off = start - self.base
        return list(self.md.disasm(self.data[off:end - self.base], start))

    def lift_function(self, start, end):
        insns = self.disasm(start, end)
        targets = {i.operands[0].imm & 0xFFFFFFFF for i in insns
                   if i.insn_name() == "b" and i.operands and i.operands[0].type == A.ARM_OP_IMM}
        lines = [f"void fn_{start:08x}(Cpu* c) {{"]
        ok = tot = 0
        for i in insns:
            if i.address in targets: lines.append(f"L_{i.address:08x}:;")
            c = self.lift_insn(i)
            tot += 1
            if c is None:
                lines.append(f"    TODO_UNIMPL(0x{i.address:08x}); /* {i.mnemonic} {i.op_str} */")
            else:
                ok += 1
                lines.append(f"    {c}  /* {i.mnemonic} {i.op_str} */")
        lines.append("}")
        return "\n".join(lines), ok, tot

PRELUDE = """#include <stdint.h>
typedef struct { uint32_t r[16]; uint32_t N, Z, C, V; } Cpu;
uint32_t mem_read32(uint32_t); uint32_t mem_read16(uint32_t); uint32_t mem_read8(uint32_t);
void mem_write32(uint32_t, uint32_t); void mem_write16(uint32_t, uint32_t); void mem_write8(uint32_t, uint32_t);
void TODO_UNIMPL(uint32_t addr);
"""

def find_functions(data, base, limit):
    starts = []
    for off in range(0, min(len(data), limit) - 4, 4):
        w = struct.unpack_from('<I', data, off)[0]
        if (w & 0xFFFF4000) == 0xE92D4000:   # push {..., lr}
            starts.append(base + off)
    return starts

def main():
    if len(sys.argv) < 4: print(__doc__); sys.exit(1)
    data = open(sys.argv[1], 'rb').read(); base = int(sys.argv[2], 0); cmd = sys.argv[3]
    L = Lifter(data, base)
    if cmd == "stats":
        end_lim = 0x800 + 0xB0000
        fns = find_functions(data, base, end_lim)
        ok = tot = full = 0
        for s, e in zip(fns, fns[1:] + [base + end_lim]):
            _, o, t = L.lift_function(s, e)
            ok += o; tot += t; full += (o == t)
        print(f"funciones detectadas: {len(fns)}")
        print(f"instrucciones traducidas: {ok}/{tot} = {100*ok/tot:.1f}%")
        print(f"funciones 100% traducidas: {full}/{len(fns)} = {100*full/len(fns):.1f}%")
    elif cmd == "lift":
        s = int(sys.argv[4], 0)
        fns = find_functions(data, base, len(data)); nxt = [f for f in fns if f > s]
        e = nxt[0] if nxt else base + len(data)
        code, ok, tot = L.lift_function(s, e)
        print(PRELUDE); print(code); print(f"/* {ok}/{tot} instrucciones traducidas */", file=sys.stderr)

if __name__ == "__main__":
    main()
