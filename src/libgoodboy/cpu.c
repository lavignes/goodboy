#include <goodboy/bus.h>
#include <goodboy/fatal.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>

STATIC_ASSERT(offsetof(Reg, l) == 0, "Reg.l offset must be 0");
STATIC_ASSERT(offsetof(Reg, h) == 1, "Reg.h offset must be 1");

void cpuReset(Cpu* cpu) { memset(cpu, 0, sizeof(*cpu)); }

static INLINE UInt push(Cpu* cpu, Bus* bus, U16 val) {
    busWrite(bus, --cpu->sp, val >> 8);
    busWrite(bus, --cpu->sp, val);
    return 16;
}

static INLINE U8 fetch(Cpu* cpu, Bus* bus) {
    U8 byte = busRead(bus, cpu->pc++);
    return byte;
}

static INLINE U16 rst(Cpu* cpu, Bus* bus, U16 addr) {
    push(cpu, bus, cpu->pc);
    cpu->pc = addr;
    return 16;
}

static INLINE UInt loadImm16(Cpu* cpu, Bus* bus, Reg* reg) {
    reg->l = fetch(cpu, bus);
    reg->h = fetch(cpu, bus);
    return 12;
}

static INLINE UInt storeIndirect(Bus* bus, U16 addr, U8 val) {
    busWrite(bus, addr, val);
    return 8;
}

static INLINE UInt inc16(U16* reg) {
    ++(*reg);
    return 8;
}

static INLINE UInt inc(Cpu* cpu, U8* reg) {
    U8 val = *reg;
    U8 res = val + 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= ((res ^ val) & 0x10) ? FLAG_H : 0;
    *reg = res;
    return 4;
}

static INLINE UInt dec(Cpu* cpu, U8* reg) {
    U8 val = *reg;
    U8 res = val - 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_N;
    cpu->af.l |= ((res ^ val) & 0x10) ? FLAG_H : 0;
    *reg = res;
    return 4;
}

static INLINE UInt loadImm8(Cpu* cpu, Bus* bus, U8* reg) {
    *reg = fetch(cpu, bus);
    return 8;
}

static INLINE U8 rlcVal(Cpu* cpu, U8 val) {
    U8 carry = (val & 0x80) >> 7;
    U8 res   = (val << 1) | carry;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= (val & 0x80) ? FLAG_C : 0;
    return res;
}

static INLINE UInt rlca(Cpu* cpu) {
    cpu->af.h = rlcVal(cpu, cpu->af.h);
    cpu->af.l &= ~FLAG_Z;
    return 4;
}

static INLINE U16 fetch16(Cpu* cpu, Bus* bus) {
    U16 val = (U16)fetch(cpu, bus);
    val |= ((U16)fetch(cpu, bus)) << 8;
    return val;
}

static INLINE UInt storeStack(Cpu* cpu, Bus* bus) {
    U16 addr = fetch16(cpu, bus);
    busWrite(bus, addr, cpu->sp);
    busWrite(bus, addr + 1, cpu->sp >> 8);
    return 20;
}

static INLINE UInt add16(Cpu* cpu, U16 val) {
    U16 hl  = cpu->hl.hl;
    U32 res = (U32)hl + (U32)val;
    cpu->af.l &= ~(FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((hl ^ ((U16)res) ^ val) & 0x1000) ? FLAG_H : 0;
    cpu->af.l |= (res > 0xFFFF) ? FLAG_C : 0;
    cpu->hl.hl = (U16)res;
    return 8;
}

static INLINE UInt loadIndirect(Bus* bus, U8* dst, U16 addr) {
    *dst = busRead(bus, addr);
    return 8;
}

static INLINE UInt dec16(U16* reg) {
    --(*reg);
    return 8;
}

static INLINE U8 rrcVal(Cpu* cpu, U8 val) {
    U8 carry = val & 0x01;
    U8 res   = (val >> 1) | (carry << 7);
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= carry ? FLAG_C : 0;
    return res;
}

static INLINE UInt rrca(Cpu* cpu) {
    cpu->af.h = rrcVal(cpu, cpu->af.h);
    cpu->af.l &= ~FLAG_Z;
    return 4;
}

static INLINE UInt stop(Cpu* cpu, Bus* bus) {
    cpu->stopped = TRUE;
    fetch(cpu, bus);
    return 4;
}

static INLINE U8 rlVal(Cpu* cpu, U8 val) {
    U8 carry = (cpu->af.l & FLAG_C) ? 1 : 0;
    U8 res   = (val << 1) | carry;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= (val & 0x80) ? FLAG_C : 0;
    return res;
}

static INLINE UInt rla(Cpu* cpu) {
    cpu->af.h = rlVal(cpu, cpu->af.h);
    cpu->af.l &= ~FLAG_Z;
    return 4;
}

static INLINE UInt jr(Cpu* cpu, Bus* bus) {
    I8 offset = (I8)fetch(cpu, bus);
    cpu->pc += (I16)offset;
    return 12;
}

static INLINE U8 rrVal(Cpu* cpu, U8 val) {
    U8 carry = (cpu->af.l & FLAG_C) ? 0x80 : 0;
    U8 res   = (val >> 1) | carry;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= (val & 0x01) ? FLAG_C : 0;
    return res;
}

static INLINE UInt rra(Cpu* cpu) {
    cpu->af.h = rrVal(cpu, cpu->af.h);
    cpu->af.l &= ~FLAG_Z;
    return 4;
}

static INLINE UInt jrCondition(Cpu* cpu, Bus* bus, Bool condition) {
    I8 offset = (I8)fetch(cpu, bus);
    if (condition) {
        cpu->pc += (I16)offset;
        return 12;
    }
    return 8;
}

static INLINE UInt daa(Cpu* cpu) {
    U8 val   = cpu->af.h;
    U8 flags = cpu->af.l;
    U8 res   = val;
    if (flags & FLAG_N) {
        if (flags & FLAG_H) {
            res -= 0x06;
        }
        if (flags & FLAG_C) {
            res -= 0x60;
        }
    } else {
        if ((flags & FLAG_H) || ((val & 0x0F) > 0x09)) {
            res += 0x06;
        }
        if ((flags & FLAG_C) || (val > 0x99)) {
            res += 0x60;
            flags |= FLAG_C;
        }
    }
    flags &= ~(FLAG_Z | FLAG_H);
    flags |= (res == 0) ? FLAG_Z : 0;
    cpu->af.h = res;
    cpu->af.l = flags;
    return 4;
}

static INLINE UInt cpl(Cpu* cpu) {
    cpu->af.h = ~cpu->af.h;
    cpu->af.l |= FLAG_N | FLAG_H;
    return 4;
}

static INLINE UInt loadSp(Cpu* cpu, Bus* bus) {
    cpu->sp = fetch16(cpu, bus);
    return 12;
}

static INLINE UInt incIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    U8 res = val + 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= ((res ^ val) & 0x10) ? FLAG_H : 0;
    busWrite(bus, addr, res);
    return 12;
}

static INLINE UInt decIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    U8 res = val - 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_N;
    cpu->af.l |= ((res ^ val) & 0x10) ? FLAG_H : 0;
    busWrite(bus, addr, res);
    return 12;
}

static INLINE UInt storeIndirectImm8(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = fetch(cpu, bus);
    busWrite(bus, addr, val);
    return 12;
}

static INLINE UInt scf(Cpu* cpu) {
    cpu->af.l &= ~(FLAG_N | FLAG_H);
    cpu->af.l |= FLAG_C;
    return 4;
}

static INLINE UInt ccf(Cpu* cpu) {
    cpu->af.l &= ~(FLAG_N | FLAG_H);
    cpu->af.l ^= FLAG_C;
    return 4;
}

static INLINE UInt copy(U8* dst, U8* src) {
    *dst = *src;
    return 4;
}

static INLINE UInt halt(Cpu* cpu) {
    cpu->halted = TRUE;
    return 4;
}

static INLINE U8 addVal(Cpu* cpu, U8 val, U8 carry) {
    U16 res = (U16)cpu->af.h + (U16)val + (U16)carry;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((U8)res == 0) ? FLAG_Z : 0;
    cpu->af.l |= ((cpu->af.h ^ val ^ (U8)res) & 0x10) ? FLAG_H : 0;
    cpu->af.l |= (res > 0xFF) ? FLAG_C : 0;
    return (U8)res;
}

static INLINE UInt add(Cpu* cpu, U8 val) {
    cpu->af.h = addVal(cpu, val, 0);
    return 4;
}

static INLINE UInt addIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    cpu->af.h = addVal(cpu, val, 0);
    return 8;
}

static INLINE UInt adc(Cpu* cpu, U8 val) {
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = addVal(cpu, val, carry);
    return 4;
}

static INLINE UInt adcIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = addVal(cpu, val, carry);
    return 8;
}

static INLINE U8 subVal(Cpu* cpu, U8 val, U8 carry) {
    U16 res = (U16)cpu->af.h - (U16)val - (U16)carry;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((U8)res == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_N;
    cpu->af.l |= ((cpu->af.h ^ val ^ (U8)res) & 0x10) ? FLAG_H : 0;
    cpu->af.l |= (res > 0xFF) ? FLAG_C : 0;
    return (U8)res;
}

static INLINE UInt sub(Cpu* cpu, U8 val) {
    cpu->af.h = subVal(cpu, val, 0);
    return 4;
}

static INLINE UInt subIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    cpu->af.h = subVal(cpu, val, 0);
    return 8;
}

static INLINE UInt sbc(Cpu* cpu, U8 val) {
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = subVal(cpu, val, carry);
    return 4;
}

static INLINE UInt sbcIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = subVal(cpu, val, carry);
    return 8;
}

static INLINE U8 andVal(Cpu* cpu, U8 val) {
    U8 res = cpu->af.h & val;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_H;
    return res;
}

static INLINE UInt and_(Cpu* cpu, U8 val) {
    cpu->af.h = andVal(cpu, val);
    return 4;
}

static INLINE UInt andIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    cpu->af.h = andVal(cpu, val);
    return 8;
}

static INLINE U8 xorVal(Cpu* cpu, U8 val) {
    U8 res = cpu->af.h ^ val;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    return res;
}

static INLINE UInt xor_(Cpu* cpu, U8 val) {
    cpu->af.h = xorVal(cpu, val);
    return 4;
}

static INLINE UInt xorIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    cpu->af.h = xorVal(cpu, val);
    return 8;
}

static INLINE U8 orVal(Cpu* cpu, U8 val) {
    U8 res = cpu->af.h | val;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    return res;
}

static INLINE UInt or_(Cpu* cpu, U8 val) {
    cpu->af.h = orVal(cpu, val);
    return 4;
}

static INLINE UInt orIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val    = busRead(bus, addr);
    cpu->af.h = orVal(cpu, val);
    return 8;
}

static INLINE void cpVal(Cpu* cpu, U8 val) {
    U16 res = (U16)cpu->af.h - (U16)val;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((U8)res == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_N;
    cpu->af.l |= ((cpu->af.h ^ val ^ (U8)res) & 0x10) ? FLAG_H : 0;
    cpu->af.l |= (res > 0xFF) ? FLAG_C : 0;
}

static INLINE UInt cp(Cpu* cpu, U8 val) {
    cpVal(cpu, val);
    return 4;
}

static INLINE UInt cpIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    cpVal(cpu, val);
    return 8;
}

static INLINE UInt ret(Cpu* cpu, Bus* bus) {
    U8 lo   = busRead(bus, cpu->sp++);
    U8 hi   = busRead(bus, cpu->sp++);
    cpu->pc = (((U16)hi) << 8) | (U16)lo;
    return 16;
}

static INLINE UInt retCondition(Cpu* cpu, Bus* bus, Bool condition) {
    if (condition) {
        return 4 + ret(cpu, bus);
    }
    return 8;
}

static INLINE UInt pop(Cpu* cpu, Bus* bus, Reg* reg) {
    reg->l = busRead(bus, cpu->sp++);
    reg->h = busRead(bus, cpu->sp++);
    return 12;
}

static INLINE UInt jmp(Cpu* cpu, Bus* bus) {
    cpu->pc = fetch16(cpu, bus);
    return 16;
}

static INLINE UInt jmpCondition(Cpu* cpu, Bus* bus, Bool condition) {
    U16 addr = fetch16(cpu, bus);
    if (condition) {
        cpu->pc = addr;
        return 16;
    }
    return 12;
}

static INLINE UInt call(Cpu* cpu, Bus* bus) {
    U16 addr = fetch16(cpu, bus);
    push(cpu, bus, cpu->pc);
    cpu->pc = addr;
    return 24;
}

static INLINE UInt callCondition(Cpu* cpu, Bus* bus, Bool condition) {
    U16 addr = fetch16(cpu, bus);
    if (condition) {
        push(cpu, bus, cpu->pc);
        cpu->pc = addr;
        return 24;
    }
    return 12;
}

static INLINE UInt adcImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = addVal(cpu, val, carry);
    return 8;
}

static INLINE UInt subImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    cpu->af.h = subVal(cpu, val, 0);
    return 8;
}

static INLINE UInt reti(Cpu* cpu, Bus* bus) {
    cpu->ime = TRUE;
    return ret(cpu, bus);
}

static INLINE UInt sbcImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    U8 carry  = (cpu->af.l & FLAG_C) ? 1 : 0;
    cpu->af.h = subVal(cpu, val, carry);
    return 8;
}

static INLINE UInt storeIndirectHighImm8(Cpu* cpu, Bus* bus) {
    U16 offset = (U16)fetch(cpu, bus);
    busWrite(bus, 0xFF00 | offset, cpu->af.h);
    return 12;
}

static INLINE UInt storeIndirectHighC(Cpu* cpu, Bus* bus) {
    busWrite(bus, 0xFF00 | (U16)cpu->bc.l, cpu->af.h);
    return 8;
}

static INLINE UInt andImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    cpu->af.h = andVal(cpu, val);
    return 8;
}

static INLINE UInt addSpImm8(Cpu* cpu, Bus* bus) {
    I8  offset = (I8)fetch(cpu, bus);
    U16 sp     = cpu->sp;
    U32 res    = (U32)((I32)sp + (I32)offset);
    // the flags are set based on the low byte only
    U8  lo     = (U8)sp;
    U16 lores  = ((U16)lo) + (U16)((U8)offset);
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((lo ^ ((U8)lores) ^ ((U8)offset)) & 0x10) ? FLAG_H : 0;
    cpu->af.l |= (lores > 0xFF) ? FLAG_C : 0;
    cpu->sp = res;
    return 16;
}

static INLINE UInt jmpHl(Cpu* cpu) {
    cpu->pc = cpu->hl.hl;
    return 4;
}

static INLINE UInt storeIndirectImm16Addr(Cpu* cpu, Bus* bus) {
    U16 addr = fetch16(cpu, bus);
    busWrite(bus, addr, cpu->af.h);
    return 16;
}

static INLINE UInt xorImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    cpu->af.h = xorVal(cpu, val);
    return 8;
}

static INLINE UInt loadIndirectHighImm8(Cpu* cpu, Bus* bus) {
    U16 offset = (U16)fetch(cpu, bus);
    cpu->af.h  = busRead(bus, 0xFF00 | offset);
    return 12;
}

static INLINE UInt popAF(Cpu* cpu, Bus* bus) {
    cpu->af.l = busRead(bus, cpu->sp++) & 0xF0;
    cpu->af.h = busRead(bus, cpu->sp++);
    return 12;
}

static INLINE UInt loadIndirectHighC(Cpu* cpu, Bus* bus) {
    cpu->af.h = busRead(bus, 0xFF00 | (U16)cpu->bc.l);
    return 8;
}

static INLINE UInt di(Cpu* cpu) {
    cpu->ime = FALSE;
    return 4;
}

static INLINE UInt orImm8(Cpu* cpu, Bus* bus) {
    U8 val    = fetch(cpu, bus);
    cpu->af.h = orVal(cpu, val);
    return 8;
}

static INLINE UInt loadSpImm8(Cpu* cpu, Bus* bus) {
    I8  offset = (I8)fetch(cpu, bus);
    U16 sp     = cpu->sp;
    U32 res    = (U32)((I32)sp + (I32)offset);
    cpu->hl.hl = (U16)res;
    // the flags are set based on the low byte only
    U8  lo     = (U8)sp;
    U16 lores  = ((U16)lo) + (U16)((U8)offset);
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= ((lo ^ ((U8)lores) ^ ((U8)offset)) & 0x10) ? FLAG_H : 0;
    cpu->af.l |= (lores > 0xFF) ? FLAG_C : 0;
    return 12;
}

static INLINE UInt loadSpHl(Cpu* cpu) {
    cpu->sp = cpu->hl.hl;
    return 8;
}

static INLINE UInt loadIndirectImm16(Cpu* cpu, Bus* bus) {
    U16 addr  = fetch16(cpu, bus);
    cpu->af.h = busRead(bus, addr);
    return 16;
}

static INLINE UInt ei(Cpu* cpu) {
    cpu->ime = TRUE;
    return 4;
}

static INLINE UInt cpImm8(Cpu* cpu, Bus* bus) {
    U8 val = fetch(cpu, bus);
    cpVal(cpu, val);
    return 8;
}

static INLINE UInt rlc(Cpu* cpu, U8* reg) {
    *reg = rlcVal(cpu, *reg);
    return 8;
}

static INLINE UInt rlcIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = rlcVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE UInt rrc(Cpu* cpu, U8* reg) {
    *reg = rrcVal(cpu, *reg);
    return 8;
}

static INLINE UInt rrcIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = rrcVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE UInt rl(Cpu* cpu, U8* reg) {
    *reg = rlVal(cpu, *reg);
    return 8;
}

static INLINE UInt rlIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = rlVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE UInt rr(Cpu* cpu, U8* reg) {
    *reg = rrVal(cpu, *reg);
    return 8;
}

static INLINE UInt rrIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = rrVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE U8 slaVal(Cpu* cpu, U8 val) {
    U8 carry = (val & 0x80) >> 7;
    U8 res   = val << 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= carry ? FLAG_C : 0;
    return res;
}

static INLINE UInt sla(Cpu* cpu, U8* reg) {
    *reg = slaVal(cpu, *reg);
    return 8;
}

static INLINE UInt slaIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = slaVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE U8 sraVal(Cpu* cpu, U8 val) {
    U8 carry = val & 0x01;
    U8 res   = ((I8)val) >> 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= carry ? FLAG_C : 0;
    return res;
}

static INLINE UInt sra(Cpu* cpu, U8* reg) {
    *reg = sraVal(cpu, *reg);
    return 8;
}

static INLINE UInt sraIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = sraVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE U8 swapVal(Cpu* cpu, U8 val) {
    U8 res = (val << 4) | (val >> 4);
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    return res;
}

static INLINE UInt swap(Cpu* cpu, U8* reg) {
    *reg = swapVal(cpu, *reg);
    return 8;
}

static INLINE UInt swapIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = swapVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE U8 srlVal(Cpu* cpu, U8 val) {
    U8 carry = val & 0x01;
    U8 res   = val >> 1;
    cpu->af.l &= ~(FLAG_Z | FLAG_N | FLAG_H | FLAG_C);
    cpu->af.l |= (res == 0) ? FLAG_Z : 0;
    cpu->af.l |= carry ? FLAG_C : 0;
    return res;
}

static INLINE UInt srl(Cpu* cpu, U8* reg) {
    *reg = srlVal(cpu, *reg);
    return 8;
}

static INLINE UInt srlIndirect(Cpu* cpu, Bus* bus, U16 addr) {
    U8 val = busRead(bus, addr);
    val    = srlVal(cpu, val);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE void bitVal(Cpu* cpu, U8 val, U8 bit) {
    cpu->af.l &= ~(FLAG_Z | FLAG_N);
    cpu->af.l |= ((val & (1 << bit)) == 0) ? FLAG_Z : 0;
    cpu->af.l |= FLAG_H;
}

static INLINE UInt bit(Cpu* cpu, U8* reg, U8 bit) {
    bitVal(cpu, *reg, bit);
    return 8;
}

static INLINE UInt bitIndirect(Cpu* cpu, Bus* bus, U16 addr, U8 bit) {
    U8 val = busRead(bus, addr);
    bitVal(cpu, val, bit);
    return 16;
}

static INLINE U8 resVal(U8 val, U8 bit) { return val & ~(1 << bit); }

static INLINE UInt res(U8* reg, U8 bit) {
    *reg = resVal(*reg, bit);
    return 8;
}

static INLINE UInt resIndirect(Bus* bus, U16 addr, U8 bit) {
    U8 val = busRead(bus, addr);
    val    = resVal(val, bit);
    busWrite(bus, addr, val);
    return 16;
}

static INLINE U8 setVal(U8 val, U8 bit) { return val | (1 << bit); }

static INLINE UInt set(U8* reg, U8 bit) {
    *reg = setVal(*reg, bit);
    return 8;
}

static INLINE UInt setIndirect(Bus* bus, U16 addr, U8 bit) {
    U8 val = busRead(bus, addr);
    val    = setVal(val, bit);
    busWrite(bus, addr, val);
    return 16;
}

static UInt cbPrefix(Cpu* cpu, Bus* bus);

UInt cpuTick(Cpu* cpu, Bus* bus) {
    U8 imask = bus->ieflags & bus->iflags;
    if (cpu->halted) {
        if (imask == 0) {
            return 4;
        }
        cpu->halted = FALSE;
    }
    if (cpu->ime) {
        if (imask != 0) {
            if (imask & INT_VBLANK) {
                rst(cpu, bus, 0x0040);
                bus->iflags &= ~INT_VBLANK;
            } else if (imask & INT_LCDSTAT) {
                rst(cpu, bus, 0x0048);
                bus->iflags &= ~INT_LCDSTAT;
            } else if (imask & INT_TIMER) {
                rst(cpu, bus, 0x0050);
                bus->iflags &= ~INT_TIMER;
            } else if (imask & INT_SERIAL) {
                rst(cpu, bus, 0x0058);
                bus->iflags &= ~INT_SERIAL;
            } else if (imask & INT_JOYPAD) {
                rst(cpu, bus, 0x0060);
                bus->iflags &= ~INT_JOYPAD;
            }
            cpu->ime = FALSE;
            return 20;
        }
    }
    switch (fetch(cpu, bus)) {
    case 0x00:
        return 4;
    case 0x01:
        return loadImm16(cpu, bus, &cpu->bc);
    case 0x02:
        return storeIndirect(bus, cpu->bc.hl, cpu->af.h);
    case 0x03:
        return inc16(&cpu->bc.hl);
    case 0x04:
        return inc(cpu, &cpu->bc.h);
    case 0x05:
        return dec(cpu, &cpu->bc.h);
    case 0x06:
        return loadImm8(cpu, bus, &cpu->bc.h);
    case 0x07:
        return rlca(cpu);
    case 0x08:
        return storeStack(cpu, bus);
    case 0x09:
        return add16(cpu, cpu->bc.hl);
    case 0x0A:
        return loadIndirect(bus, &cpu->af.h, cpu->bc.hl);
    case 0x0B:
        return dec16(&cpu->bc.hl);
    case 0x0C:
        return inc(cpu, &cpu->bc.l);
    case 0x0D:
        return dec(cpu, &cpu->bc.l);
    case 0x0E:
        return loadImm8(cpu, bus, &cpu->bc.l);
    case 0x0F:
        return rrca(cpu);

    case 0x10:
        return stop(cpu, bus);
    case 0x11:
        return loadImm16(cpu, bus, &cpu->de);
    case 0x12:
        return storeIndirect(bus, cpu->de.hl, cpu->af.h);
    case 0x13:
        return inc16(&cpu->de.hl);
    case 0x14:
        return inc(cpu, &cpu->de.h);
    case 0x15:
        return dec(cpu, &cpu->de.h);
    case 0x16:
        return loadImm8(cpu, bus, &cpu->de.h);
    case 0x17:
        return rla(cpu);
    case 0x18:
        return jr(cpu, bus);
    case 0x19:
        return add16(cpu, cpu->de.hl);
    case 0x1A:
        return loadIndirect(bus, &cpu->af.h, cpu->de.hl);
    case 0x1B:
        return dec16(&cpu->de.hl);
    case 0x1C:
        return inc(cpu, &cpu->de.l);
    case 0x1D:
        return dec(cpu, &cpu->de.l);
    case 0x1E:
        return loadImm8(cpu, bus, &cpu->de.l);
    case 0x1F:
        return rra(cpu);

    case 0x20:
        return jrCondition(cpu, bus, !(cpu->af.l & FLAG_Z));
    case 0x21:
        return loadImm16(cpu, bus, &cpu->hl);
    case 0x22:
        return storeIndirect(bus, cpu->hl.hl++, cpu->af.h);
    case 0x23:
        return inc16(&cpu->hl.hl);
    case 0x24:
        return inc(cpu, &cpu->hl.h);
    case 0x25:
        return dec(cpu, &cpu->hl.h);
    case 0x26:
        return loadImm8(cpu, bus, &cpu->hl.h);
    case 0x27:
        return daa(cpu);
    case 0x28:
        return jrCondition(cpu, bus, cpu->af.l & FLAG_Z);
    case 0x29:
        return add16(cpu, cpu->hl.hl);
    case 0x2A:
        return loadIndirect(bus, &cpu->af.h, cpu->hl.hl++);
    case 0x2B:
        return dec16(&cpu->hl.hl);
    case 0x2C:
        return inc(cpu, &cpu->hl.l);
    case 0x2D:
        return dec(cpu, &cpu->hl.l);
    case 0x2E:
        return loadImm8(cpu, bus, &cpu->hl.l);
    case 0x2F:
        return cpl(cpu);

    case 0x30:
        return jrCondition(cpu, bus, !(cpu->af.l & FLAG_C));
    case 0x31:
        return loadSp(cpu, bus);
    case 0x32:
        return storeIndirect(bus, cpu->hl.hl--, cpu->af.h);
    case 0x33:
        return inc16(&cpu->sp);
    case 0x34:
        return incIndirect(cpu, bus, cpu->hl.hl);
    case 0x35:
        return decIndirect(cpu, bus, cpu->hl.hl);
    case 0x36:
        return storeIndirectImm8(cpu, bus, cpu->hl.hl);
    case 0x37:
        return scf(cpu);
    case 0x38:
        return jrCondition(cpu, bus, cpu->af.l & FLAG_C);
    case 0x39:
        return add16(cpu, cpu->sp);
    case 0x3A:
        return loadIndirect(bus, &cpu->af.h, cpu->hl.hl--);
    case 0x3B:
        return dec16(&cpu->sp);
    case 0x3C:
        return inc(cpu, &cpu->af.h);
    case 0x3D:
        return dec(cpu, &cpu->af.h);
    case 0x3E:
        return loadImm8(cpu, bus, &cpu->af.h);
    case 0x3F:
        return ccf(cpu);

    case 0x40:
        return copy(&cpu->bc.h, &cpu->bc.h);
    case 0x41:
        return copy(&cpu->bc.h, &cpu->bc.l);
    case 0x42:
        return copy(&cpu->bc.h, &cpu->de.h);
    case 0x43:
        return copy(&cpu->bc.h, &cpu->de.l);
    case 0x44:
        return copy(&cpu->bc.h, &cpu->hl.h);
    case 0x45:
        return copy(&cpu->bc.h, &cpu->hl.l);
    case 0x46:
        return loadIndirect(bus, &cpu->bc.h, cpu->hl.hl);
    case 0x47:
        return copy(&cpu->bc.h, &cpu->af.h);
    case 0x48:
        return copy(&cpu->bc.l, &cpu->bc.h);
    case 0x49:
        return copy(&cpu->bc.l, &cpu->bc.l);
    case 0x4A:
        return copy(&cpu->bc.l, &cpu->de.h);
    case 0x4B:
        return copy(&cpu->bc.l, &cpu->de.l);
    case 0x4C:
        return copy(&cpu->bc.l, &cpu->hl.h);
    case 0x4D:
        return copy(&cpu->bc.l, &cpu->hl.l);
    case 0x4E:
        return loadIndirect(bus, &cpu->bc.l, cpu->hl.hl);
    case 0x4F:
        return copy(&cpu->bc.l, &cpu->af.h);

    case 0x50:
        return copy(&cpu->de.h, &cpu->bc.h);
    case 0x51:
        return copy(&cpu->de.h, &cpu->bc.l);
    case 0x52:
        return copy(&cpu->de.h, &cpu->de.h);
    case 0x53:
        return copy(&cpu->de.h, &cpu->de.l);
    case 0x54:
        return copy(&cpu->de.h, &cpu->hl.h);
    case 0x55:
        return copy(&cpu->de.h, &cpu->hl.l);
    case 0x56:
        return loadIndirect(bus, &cpu->de.h, cpu->hl.hl);
    case 0x57:
        return copy(&cpu->de.h, &cpu->af.h);
    case 0x58:
        return copy(&cpu->de.l, &cpu->bc.h);
    case 0x59:
        return copy(&cpu->de.l, &cpu->bc.l);
    case 0x5A:
        return copy(&cpu->de.l, &cpu->de.h);
    case 0x5B:
        return copy(&cpu->de.l, &cpu->de.l);
    case 0x5C:
        return copy(&cpu->de.l, &cpu->hl.h);
    case 0x5D:
        return copy(&cpu->de.l, &cpu->hl.l);
    case 0x5E:
        return loadIndirect(bus, &cpu->de.l, cpu->hl.hl);
    case 0x5F:
        return copy(&cpu->de.l, &cpu->af.h);

    case 0x60:
        return copy(&cpu->hl.h, &cpu->bc.h);
    case 0x61:
        return copy(&cpu->hl.h, &cpu->bc.l);
    case 0x62:
        return copy(&cpu->hl.h, &cpu->de.h);
    case 0x63:
        return copy(&cpu->hl.h, &cpu->de.l);
    case 0x64:
        return copy(&cpu->hl.h, &cpu->hl.h);
    case 0x65:
        return copy(&cpu->hl.h, &cpu->hl.l);
    case 0x66:
        return loadIndirect(bus, &cpu->hl.h, cpu->hl.hl);
    case 0x67:
        return copy(&cpu->hl.h, &cpu->af.h);
    case 0x68:
        return copy(&cpu->hl.l, &cpu->bc.h);
    case 0x69:
        return copy(&cpu->hl.l, &cpu->bc.l);
    case 0x6A:
        return copy(&cpu->hl.l, &cpu->de.h);
    case 0x6B:
        return copy(&cpu->hl.l, &cpu->de.l);
    case 0x6C:
        return copy(&cpu->hl.l, &cpu->hl.h);
    case 0x6D:
        return copy(&cpu->hl.l, &cpu->hl.l);
    case 0x6E:
        return loadIndirect(bus, &cpu->hl.l, cpu->hl.hl);
    case 0x6F:
        return copy(&cpu->hl.l, &cpu->af.h);

    case 0x70:
        return storeIndirect(bus, cpu->hl.hl, cpu->bc.h);
    case 0x71:
        return storeIndirect(bus, cpu->hl.hl, cpu->bc.l);
    case 0x72:
        return storeIndirect(bus, cpu->hl.hl, cpu->de.h);
    case 0x73:
        return storeIndirect(bus, cpu->hl.hl, cpu->de.l);
    case 0x74:
        return storeIndirect(bus, cpu->hl.hl, cpu->hl.h);
    case 0x75:
        return storeIndirect(bus, cpu->hl.hl, cpu->hl.l);
    case 0x76:
        return halt(cpu);
    case 0x77:
        return storeIndirect(bus, cpu->hl.hl, cpu->af.h);
    case 0x78:
        return copy(&cpu->af.h, &cpu->bc.h);
    case 0x79:
        return copy(&cpu->af.h, &cpu->bc.l);
    case 0x7A:
        return copy(&cpu->af.h, &cpu->de.h);
    case 0x7B:
        return copy(&cpu->af.h, &cpu->de.l);
    case 0x7C:
        return copy(&cpu->af.h, &cpu->hl.h);
    case 0x7D:
        return copy(&cpu->af.h, &cpu->hl.l);
    case 0x7E:
        return loadIndirect(bus, &cpu->af.h, cpu->hl.hl);
    case 0x7F:
        return copy(&cpu->af.h, &cpu->af.h);

    case 0x80:
        return add(cpu, cpu->bc.h);
    case 0x81:
        return add(cpu, cpu->bc.l);
    case 0x82:
        return add(cpu, cpu->de.h);
    case 0x83:
        return add(cpu, cpu->de.l);
    case 0x84:
        return add(cpu, cpu->hl.h);
    case 0x85:
        return add(cpu, cpu->hl.l);
    case 0x86:
        return addIndirect(cpu, bus, cpu->hl.hl);
    case 0x87:
        return add(cpu, cpu->af.h);
    case 0x88:
        return adc(cpu, cpu->bc.h);
    case 0x89:
        return adc(cpu, cpu->bc.l);
    case 0x8A:
        return adc(cpu, cpu->de.h);
    case 0x8B:
        return adc(cpu, cpu->de.l);
    case 0x8C:
        return adc(cpu, cpu->hl.h);
    case 0x8D:
        return adc(cpu, cpu->hl.l);
    case 0x8E:
        return adcIndirect(cpu, bus, cpu->hl.hl);
    case 0x8F:
        return adc(cpu, cpu->af.h);

    case 0x90:
        return sub(cpu, cpu->bc.h);
    case 0x91:
        return sub(cpu, cpu->bc.l);
    case 0x92:
        return sub(cpu, cpu->de.h);
    case 0x93:
        return sub(cpu, cpu->de.l);
    case 0x94:
        return sub(cpu, cpu->hl.h);
    case 0x95:
        return sub(cpu, cpu->hl.l);
    case 0x96:
        return subIndirect(cpu, bus, cpu->hl.hl);
    case 0x97:
        return sub(cpu, cpu->af.h);
    case 0x98:
        return sbc(cpu, cpu->bc.h);
    case 0x99:
        return sbc(cpu, cpu->bc.l);
    case 0x9A:
        return sbc(cpu, cpu->de.h);
    case 0x9B:
        return sbc(cpu, cpu->de.l);
    case 0x9C:
        return sbc(cpu, cpu->hl.h);
    case 0x9D:
        return sbc(cpu, cpu->hl.l);
    case 0x9E:
        return sbcIndirect(cpu, bus, cpu->hl.hl);
    case 0x9F:
        return sbc(cpu, cpu->af.h);

    case 0xA0:
        return and_(cpu, cpu->bc.h);
    case 0xA1:
        return and_(cpu, cpu->bc.l);
    case 0xA2:
        return and_(cpu, cpu->de.h);
    case 0xA3:
        return and_(cpu, cpu->de.l);
    case 0xA4:
        return and_(cpu, cpu->hl.h);
    case 0xA5:
        return and_(cpu, cpu->hl.l);
    case 0xA6:
        return andIndirect(cpu, bus, cpu->hl.hl);
    case 0xA7:
        return and_(cpu, cpu->af.h);
    case 0xA8:
        return xor_(cpu, cpu->bc.h);
    case 0xA9:
        return xor_(cpu, cpu->bc.l);
    case 0xAA:
        return xor_(cpu, cpu->de.h);
    case 0xAB:
        return xor_(cpu, cpu->de.l);
    case 0xAC:
        return xor_(cpu, cpu->hl.h);
    case 0xAD:
        return xor_(cpu, cpu->hl.l);
    case 0xAE:
        return xorIndirect(cpu, bus, cpu->hl.hl);
    case 0xAF:
        return xor_(cpu, cpu->af.h);

    case 0xB0:
        return or_(cpu, cpu->bc.h);
    case 0xB1:
        return or_(cpu, cpu->bc.l);
    case 0xB2:
        return or_(cpu, cpu->de.h);
    case 0xB3:
        return or_(cpu, cpu->de.l);
    case 0xB4:
        return or_(cpu, cpu->hl.h);
    case 0xB5:
        return or_(cpu, cpu->hl.l);
    case 0xB6:
        return orIndirect(cpu, bus, cpu->hl.hl);
    case 0xB7:
        return or_(cpu, cpu->af.h);
    case 0xB8:
        return cp(cpu, cpu->bc.h);
    case 0xB9:
        return cp(cpu, cpu->bc.l);
    case 0xBA:
        return cp(cpu, cpu->de.h);
    case 0xBB:
        return cp(cpu, cpu->de.l);
    case 0xBC:
        return cp(cpu, cpu->hl.h);
    case 0xBD:
        return cp(cpu, cpu->hl.l);
    case 0xBE:
        return cpIndirect(cpu, bus, cpu->hl.hl);
    case 0xBF:
        return cp(cpu, cpu->af.h);

    case 0xC0:
        return retCondition(cpu, bus, !(cpu->af.l & FLAG_Z));
    case 0xC1:
        return pop(cpu, bus, &cpu->bc);
    case 0xC2:
        return jmpCondition(cpu, bus, !(cpu->af.l & FLAG_Z));
    case 0xC3:
        return jmp(cpu, bus);
    case 0xC4:
        return callCondition(cpu, bus, !(cpu->af.l & FLAG_Z));
    case 0xC5:
        return push(cpu, bus, cpu->bc.hl);
    case 0xC6:
        return add(cpu, fetch(cpu, bus));
    case 0xC7:
        return rst(cpu, bus, 0x0000);
    case 0xC8:
        return retCondition(cpu, bus, cpu->af.l & FLAG_Z);
    case 0xC9:
        return ret(cpu, bus);
    case 0xCA:
        return jmpCondition(cpu, bus, cpu->af.l & FLAG_Z);
    case 0xCB:
        return cbPrefix(cpu, bus);
    case 0xCC:
        return callCondition(cpu, bus, cpu->af.l & FLAG_Z);
    case 0xCD:
        return call(cpu, bus);
    case 0xCE:
        return adcImm8(cpu, bus);
    case 0xCF:
        return rst(cpu, bus, 0x0008);

    case 0xD0:
        return retCondition(cpu, bus, !(cpu->af.l & FLAG_C));
    case 0xD1:
        return pop(cpu, bus, &cpu->de);
    case 0xD2:
        return jmpCondition(cpu, bus, !(cpu->af.l & FLAG_C));
    case 0xD3:
        return 4;
    case 0xD4:
        return callCondition(cpu, bus, !(cpu->af.l & FLAG_C));
    case 0xD5:
        return push(cpu, bus, cpu->de.hl);
    case 0xD6:
        return subImm8(cpu, bus);
    case 0xD7:
        return rst(cpu, bus, 0x0010);
    case 0xD8:
        return retCondition(cpu, bus, cpu->af.l & FLAG_C);
    case 0xD9:
        return reti(cpu, bus);
    case 0xDA:
        return jmpCondition(cpu, bus, cpu->af.l & FLAG_C);
    case 0xDB:
        return 4;
    case 0xDC:
        return callCondition(cpu, bus, cpu->af.l & FLAG_C);
    case 0xDD:
        return 4;
    case 0xDE:
        return sbcImm8(cpu, bus);
    case 0xDF:
        return rst(cpu, bus, 0x0018);

    case 0xE0:
        return storeIndirectHighImm8(cpu, bus);
    case 0xE1:
        return pop(cpu, bus, &cpu->hl);
    case 0xE2:
        return storeIndirectHighC(cpu, bus);
    case 0xE3:
        return 4;
    case 0xE4:
        return 4;
    case 0xE5:
        return push(cpu, bus, cpu->hl.hl);
    case 0xE6:
        return andImm8(cpu, bus);
    case 0xE7:
        return rst(cpu, bus, 0x0020);
    case 0xE8:
        return addSpImm8(cpu, bus);
    case 0xE9:
        return jmpHl(cpu);
    case 0xEA:
        return storeIndirectImm16Addr(cpu, bus);
    case 0xEB:
        return 4;
    case 0xEC:
        return 4;
    case 0xED:
        return 4;
    case 0xEE:
        return xorImm8(cpu, bus);
    case 0xEF:
        return rst(cpu, bus, 0x0028);

    case 0xF0:
        return loadIndirectHighImm8(cpu, bus);
    case 0xF1:
        return popAF(cpu, bus);
    case 0xF2:
        return loadIndirectHighC(cpu, bus);
    case 0xF3:
        return di(cpu);
    case 0xF4:
        return 4;
    case 0xF5:
        return push(cpu, bus, cpu->af.hl);
    case 0xF6:
        return orImm8(cpu, bus);
    case 0xF7:
        return rst(cpu, bus, 0x0030);
    case 0xF8:
        return loadSpImm8(cpu, bus);
    case 0xF9:
        return loadSpHl(cpu);
    case 0xFA:
        return loadIndirectImm16(cpu, bus);
    case 0xFB:
        return ei(cpu);
    case 0xFC:
        return 4;
    case 0xFD:
        return 4;
    case 0xFE:
        return cpImm8(cpu, bus);
    case 0xFF:
        return rst(cpu, bus, 0x0038);
    }
    UNREACHABLE();
}

static UInt cbPrefix(Cpu* cpu, Bus* bus) {
    switch (fetch(cpu, bus)) {
    case 0x00:
        return rlc(cpu, &cpu->bc.h);
    case 0x01:
        return rlc(cpu, &cpu->bc.l);
    case 0x02:
        return rlc(cpu, &cpu->de.h);
    case 0x03:
        return rlc(cpu, &cpu->de.l);
    case 0x04:
        return rlc(cpu, &cpu->hl.h);
    case 0x05:
        return rlc(cpu, &cpu->hl.l);
    case 0x06:
        return rlcIndirect(cpu, bus, cpu->hl.hl);
    case 0x07:
        return rlc(cpu, &cpu->af.h);
    case 0x08:
        return rrc(cpu, &cpu->bc.h);
    case 0x09:
        return rrc(cpu, &cpu->bc.l);
    case 0x0A:
        return rrc(cpu, &cpu->de.h);
    case 0x0B:
        return rrc(cpu, &cpu->de.l);
    case 0x0C:
        return rrc(cpu, &cpu->hl.h);
    case 0x0D:
        return rrc(cpu, &cpu->hl.l);
    case 0x0E:
        return rrcIndirect(cpu, bus, cpu->hl.hl);
    case 0x0F:
        return rrc(cpu, &cpu->af.h);

    case 0x10:
        return rl(cpu, &cpu->bc.h);
    case 0x11:
        return rl(cpu, &cpu->bc.l);
    case 0x12:
        return rl(cpu, &cpu->de.h);
    case 0x13:
        return rl(cpu, &cpu->de.l);
    case 0x14:
        return rl(cpu, &cpu->hl.h);
    case 0x15:
        return rl(cpu, &cpu->hl.l);
    case 0x16:
        return rlIndirect(cpu, bus, cpu->hl.hl);
    case 0x17:
        return rl(cpu, &cpu->af.h);
    case 0x18:
        return rr(cpu, &cpu->bc.h);
    case 0x19:
        return rr(cpu, &cpu->bc.l);
    case 0x1A:
        return rr(cpu, &cpu->de.h);
    case 0x1B:
        return rr(cpu, &cpu->de.l);
    case 0x1C:
        return rr(cpu, &cpu->hl.h);
    case 0x1D:
        return rr(cpu, &cpu->hl.l);
    case 0x1E:
        return rrIndirect(cpu, bus, cpu->hl.hl);
    case 0x1F:
        return rr(cpu, &cpu->af.h);

    case 0x20:
        return sla(cpu, &cpu->bc.h);
    case 0x21:
        return sla(cpu, &cpu->bc.l);
    case 0x22:
        return sla(cpu, &cpu->de.h);
    case 0x23:
        return sla(cpu, &cpu->de.l);
    case 0x24:
        return sla(cpu, &cpu->hl.h);
    case 0x25:
        return sla(cpu, &cpu->hl.l);
    case 0x26:
        return slaIndirect(cpu, bus, cpu->hl.hl);
    case 0x27:
        return sla(cpu, &cpu->af.h);
    case 0x28:
        return sra(cpu, &cpu->bc.h);
    case 0x29:
        return sra(cpu, &cpu->bc.l);
    case 0x2A:
        return sra(cpu, &cpu->de.h);
    case 0x2B:
        return sra(cpu, &cpu->de.l);
    case 0x2C:
        return sra(cpu, &cpu->hl.h);
    case 0x2D:
        return sra(cpu, &cpu->hl.l);
    case 0x2E:
        return sraIndirect(cpu, bus, cpu->hl.hl);
    case 0x2F:
        return sra(cpu, &cpu->af.h);

    case 0x30:
        return swap(cpu, &cpu->bc.h);
    case 0x31:
        return swap(cpu, &cpu->bc.l);
    case 0x32:
        return swap(cpu, &cpu->de.h);
    case 0x33:
        return swap(cpu, &cpu->de.l);
    case 0x34:
        return swap(cpu, &cpu->hl.h);
    case 0x35:
        return swap(cpu, &cpu->hl.l);
    case 0x36:
        return swapIndirect(cpu, bus, cpu->hl.hl);
    case 0x37:
        return swap(cpu, &cpu->af.h);
    case 0x38:
        return srl(cpu, &cpu->bc.h);
    case 0x39:
        return srl(cpu, &cpu->bc.l);
    case 0x3A:
        return srl(cpu, &cpu->de.h);
    case 0x3B:
        return srl(cpu, &cpu->de.l);
    case 0x3C:
        return srl(cpu, &cpu->hl.h);
    case 0x3D:
        return srl(cpu, &cpu->hl.l);
    case 0x3E:
        return srlIndirect(cpu, bus, cpu->hl.hl);
    case 0x3F:
        return srl(cpu, &cpu->af.h);

    case 0x40:
        return bit(cpu, &cpu->bc.h, 0);
    case 0x41:
        return bit(cpu, &cpu->bc.l, 0);
    case 0x42:
        return bit(cpu, &cpu->de.h, 0);
    case 0x43:
        return bit(cpu, &cpu->de.l, 0);
    case 0x44:
        return bit(cpu, &cpu->hl.h, 0);
    case 0x45:
        return bit(cpu, &cpu->hl.l, 0);
    case 0x46:
        return bitIndirect(cpu, bus, cpu->hl.hl, 0);
    case 0x47:
        return bit(cpu, &cpu->af.h, 0);
    case 0x48:
        return bit(cpu, &cpu->bc.h, 1);
    case 0x49:
        return bit(cpu, &cpu->bc.l, 1);
    case 0x4A:
        return bit(cpu, &cpu->de.h, 1);
    case 0x4B:
        return bit(cpu, &cpu->de.l, 1);
    case 0x4C:
        return bit(cpu, &cpu->hl.h, 1);
    case 0x4D:
        return bit(cpu, &cpu->hl.l, 1);
    case 0x4E:
        return bitIndirect(cpu, bus, cpu->hl.hl, 1);
    case 0x4F:
        return bit(cpu, &cpu->af.h, 1);

    case 0x50:
        return bit(cpu, &cpu->bc.h, 2);
    case 0x51:
        return bit(cpu, &cpu->bc.l, 2);
    case 0x52:
        return bit(cpu, &cpu->de.h, 2);
    case 0x53:
        return bit(cpu, &cpu->de.l, 2);
    case 0x54:
        return bit(cpu, &cpu->hl.h, 2);
    case 0x55:
        return bit(cpu, &cpu->hl.l, 2);
    case 0x56:
        return bitIndirect(cpu, bus, cpu->hl.hl, 2);
    case 0x57:
        return bit(cpu, &cpu->af.h, 2);
    case 0x58:
        return bit(cpu, &cpu->bc.h, 3);
    case 0x59:
        return bit(cpu, &cpu->bc.l, 3);
    case 0x5A:
        return bit(cpu, &cpu->de.h, 3);
    case 0x5B:
        return bit(cpu, &cpu->de.l, 3);
    case 0x5C:
        return bit(cpu, &cpu->hl.h, 3);
    case 0x5D:
        return bit(cpu, &cpu->hl.l, 3);
    case 0x5E:
        return bitIndirect(cpu, bus, cpu->hl.hl, 3);
    case 0x5F:
        return bit(cpu, &cpu->af.h, 3);

    case 0x60:
        return bit(cpu, &cpu->bc.h, 4);
    case 0x61:
        return bit(cpu, &cpu->bc.l, 4);
    case 0x62:
        return bit(cpu, &cpu->de.h, 4);
    case 0x63:
        return bit(cpu, &cpu->de.l, 4);
    case 0x64:
        return bit(cpu, &cpu->hl.h, 4);
    case 0x65:
        return bit(cpu, &cpu->hl.l, 4);
    case 0x66:
        return bitIndirect(cpu, bus, cpu->hl.hl, 4);
    case 0x67:
        return bit(cpu, &cpu->af.h, 4);
    case 0x68:
        return bit(cpu, &cpu->bc.h, 5);
    case 0x69:
        return bit(cpu, &cpu->bc.l, 5);
    case 0x6A:
        return bit(cpu, &cpu->de.h, 5);
    case 0x6B:
        return bit(cpu, &cpu->de.l, 5);
    case 0x6C:
        return bit(cpu, &cpu->hl.h, 5);
    case 0x6D:
        return bit(cpu, &cpu->hl.l, 5);
    case 0x6E:
        return bitIndirect(cpu, bus, cpu->hl.hl, 5);
    case 0x6F:
        return bit(cpu, &cpu->af.h, 5);

    case 0x70:
        return bit(cpu, &cpu->bc.h, 6);
    case 0x71:
        return bit(cpu, &cpu->bc.l, 6);
    case 0x72:
        return bit(cpu, &cpu->de.h, 6);
    case 0x73:
        return bit(cpu, &cpu->de.l, 6);
    case 0x74:
        return bit(cpu, &cpu->hl.h, 6);
    case 0x75:
        return bit(cpu, &cpu->hl.l, 6);
    case 0x76:
        return bitIndirect(cpu, bus, cpu->hl.hl, 6);
    case 0x77:
        return bit(cpu, &cpu->af.h, 6);
    case 0x78:
        return bit(cpu, &cpu->bc.h, 7);
    case 0x79:
        return bit(cpu, &cpu->bc.l, 7);
    case 0x7A:
        return bit(cpu, &cpu->de.h, 7);
    case 0x7B:
        return bit(cpu, &cpu->de.l, 7);
    case 0x7C:
        return bit(cpu, &cpu->hl.h, 7);
    case 0x7D:
        return bit(cpu, &cpu->hl.l, 7);
    case 0x7E:
        return bitIndirect(cpu, bus, cpu->hl.hl, 7);
    case 0x7F:
        return bit(cpu, &cpu->af.h, 7);

    case 0x80:
        return res(&cpu->bc.h, 0);
    case 0x81:
        return res(&cpu->bc.l, 0);
    case 0x82:
        return res(&cpu->de.h, 0);
    case 0x83:
        return res(&cpu->de.l, 0);
    case 0x84:
        return res(&cpu->hl.h, 0);
    case 0x85:
        return res(&cpu->hl.l, 0);
    case 0x86:
        return resIndirect(bus, cpu->hl.hl, 0);
    case 0x87:
        return res(&cpu->af.h, 0);
    case 0x88:
        return res(&cpu->bc.h, 1);
    case 0x89:
        return res(&cpu->bc.l, 1);
    case 0x8A:
        return res(&cpu->de.h, 1);
    case 0x8B:
        return res(&cpu->de.l, 1);
    case 0x8C:
        return res(&cpu->hl.h, 1);
    case 0x8D:
        return res(&cpu->hl.l, 1);
    case 0x8E:
        return resIndirect(bus, cpu->hl.hl, 1);
    case 0x8F:
        return res(&cpu->af.h, 1);

    case 0x90:
        return res(&cpu->bc.h, 2);
    case 0x91:
        return res(&cpu->bc.l, 2);
    case 0x92:
        return res(&cpu->de.h, 2);
    case 0x93:
        return res(&cpu->de.l, 2);
    case 0x94:
        return res(&cpu->hl.h, 2);
    case 0x95:
        return res(&cpu->hl.l, 2);
    case 0x96:
        return resIndirect(bus, cpu->hl.hl, 2);
    case 0x97:
        return res(&cpu->af.h, 2);
    case 0x98:
        return res(&cpu->bc.h, 3);
    case 0x99:
        return res(&cpu->bc.l, 3);
    case 0x9A:
        return res(&cpu->de.h, 3);
    case 0x9B:
        return res(&cpu->de.l, 3);
    case 0x9C:
        return res(&cpu->hl.h, 3);
    case 0x9D:
        return res(&cpu->hl.l, 3);
    case 0x9E:
        return resIndirect(bus, cpu->hl.hl, 3);
    case 0x9F:
        return res(&cpu->af.h, 3);

    case 0xA0:
        return res(&cpu->bc.h, 4);
    case 0xA1:
        return res(&cpu->bc.l, 4);
    case 0xA2:
        return res(&cpu->de.h, 4);
    case 0xA3:
        return res(&cpu->de.l, 4);
    case 0xA4:
        return res(&cpu->hl.h, 4);
    case 0xA5:
        return res(&cpu->hl.l, 4);
    case 0xA6:
        return resIndirect(bus, cpu->hl.hl, 4);
    case 0xA7:
        return res(&cpu->af.h, 4);
    case 0xA8:
        return res(&cpu->bc.h, 5);
    case 0xA9:
        return res(&cpu->bc.l, 5);
    case 0xAA:
        return res(&cpu->de.h, 5);
    case 0xAB:
        return res(&cpu->de.l, 5);
    case 0xAC:
        return res(&cpu->hl.h, 5);
    case 0xAD:
        return res(&cpu->hl.l, 5);
    case 0xAE:
        return resIndirect(bus, cpu->hl.hl, 5);
    case 0xAF:
        return res(&cpu->af.h, 5);

    case 0xB0:
        return res(&cpu->bc.h, 6);
    case 0xB1:
        return res(&cpu->bc.l, 6);
    case 0xB2:
        return res(&cpu->de.h, 6);
    case 0xB3:
        return res(&cpu->de.l, 6);
    case 0xB4:
        return res(&cpu->hl.h, 6);
    case 0xB5:
        return res(&cpu->hl.l, 6);
    case 0xB6:
        return resIndirect(bus, cpu->hl.hl, 6);
    case 0xB7:
        return res(&cpu->af.h, 6);
    case 0xB8:
        return res(&cpu->bc.h, 7);
    case 0xB9:
        return res(&cpu->bc.l, 7);
    case 0xBA:
        return res(&cpu->de.h, 7);
    case 0xBB:
        return res(&cpu->de.l, 7);
    case 0xBC:
        return res(&cpu->hl.h, 7);
    case 0xBD:
        return res(&cpu->hl.l, 7);
    case 0xBE:
        return resIndirect(bus, cpu->hl.hl, 7);
    case 0xBF:
        return res(&cpu->af.h, 7);

    case 0xC0:
        return set(&cpu->bc.h, 0);
    case 0xC1:
        return set(&cpu->bc.l, 0);
    case 0xC2:
        return set(&cpu->de.h, 0);
    case 0xC3:
        return set(&cpu->de.l, 0);
    case 0xC4:
        return set(&cpu->hl.h, 0);
    case 0xC5:
        return set(&cpu->hl.l, 0);
    case 0xC6:
        return setIndirect(bus, cpu->hl.hl, 0);
    case 0xC7:
        return set(&cpu->af.h, 0);
    case 0xC8:
        return set(&cpu->bc.h, 1);
    case 0xC9:
        return set(&cpu->bc.l, 1);
    case 0xCA:
        return set(&cpu->de.h, 1);
    case 0xCB:
        return set(&cpu->de.l, 1);
    case 0xCC:
        return set(&cpu->hl.h, 1);
    case 0xCD:
        return set(&cpu->hl.l, 1);
    case 0xCE:
        return setIndirect(bus, cpu->hl.hl, 1);
    case 0xCF:
        return set(&cpu->af.h, 1);

    case 0xD0:
        return set(&cpu->bc.h, 2);
    case 0xD1:
        return set(&cpu->bc.l, 2);
    case 0xD2:
        return set(&cpu->de.h, 2);
    case 0xD3:
        return set(&cpu->de.l, 2);
    case 0xD4:
        return set(&cpu->hl.h, 2);
    case 0xD5:
        return set(&cpu->hl.l, 2);
    case 0xD6:
        return setIndirect(bus, cpu->hl.hl, 2);
    case 0xD7:
        return set(&cpu->af.h, 2);
    case 0xD8:
        return set(&cpu->bc.h, 3);
    case 0xD9:
        return set(&cpu->bc.l, 3);
    case 0xDA:
        return set(&cpu->de.h, 3);
    case 0xDB:
        return set(&cpu->de.l, 3);
    case 0xDC:
        return set(&cpu->hl.h, 3);
    case 0xDD:
        return set(&cpu->hl.l, 3);
    case 0xDE:
        return setIndirect(bus, cpu->hl.hl, 3);
    case 0xDF:
        return set(&cpu->af.h, 3);

    case 0xE0:
        return set(&cpu->bc.h, 4);
    case 0xE1:
        return set(&cpu->bc.l, 4);
    case 0xE2:
        return set(&cpu->de.h, 4);
    case 0xE3:
        return set(&cpu->de.l, 4);
    case 0xE4:
        return set(&cpu->hl.h, 4);
    case 0xE5:
        return set(&cpu->hl.l, 4);
    case 0xE6:
        return setIndirect(bus, cpu->hl.hl, 4);
    case 0xE7:
        return set(&cpu->af.h, 4);
    case 0xE8:
        return set(&cpu->bc.h, 5);
    case 0xE9:
        return set(&cpu->bc.l, 5);
    case 0xEA:
        return set(&cpu->de.h, 5);
    case 0xEB:
        return set(&cpu->de.l, 5);
    case 0xEC:
        return set(&cpu->hl.h, 5);
    case 0xED:
        return set(&cpu->hl.l, 5);
    case 0xEE:
        return setIndirect(bus, cpu->hl.hl, 5);
    case 0xEF:
        return set(&cpu->af.h, 5);

    case 0xF0:
        return set(&cpu->bc.h, 6);
    case 0xF1:
        return set(&cpu->bc.l, 6);
    case 0xF2:
        return set(&cpu->de.h, 6);
    case 0xF3:
        return set(&cpu->de.l, 6);
    case 0xF4:
        return set(&cpu->hl.h, 6);
    case 0xF5:
        return set(&cpu->hl.l, 6);
    case 0xF6:
        return setIndirect(bus, cpu->hl.hl, 6);
    case 0xF7:
        return set(&cpu->af.h, 6);
    case 0xF8:
        return set(&cpu->bc.h, 7);
    case 0xF9:
        return set(&cpu->bc.l, 7);
    case 0xFA:
        return set(&cpu->de.h, 7);
    case 0xFB:
        return set(&cpu->de.l, 7);
    case 0xFC:
        return set(&cpu->hl.h, 7);
    case 0xFD:
        return set(&cpu->hl.l, 7);
    case 0xFE:
        return setIndirect(bus, cpu->hl.hl, 7);
    case 0xFF:
        return set(&cpu->af.h, 7);
    }
    UNREACHABLE();
}
