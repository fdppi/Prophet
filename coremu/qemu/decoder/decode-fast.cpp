//
// PTLsim: Cycle Accurate x86-64 Simulator
// Decoder for simple x86 instructions
//
// Copyright 1999-2006 Matt T. Yourst <yourst@yourst.com>
//
// ----------------------------------------------------------------------
//
//  This file is part of FeS2.
//
//  FeS2 is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  FeS2 is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with FeS2.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------

#include "TraceDecoder.h"

bool TraceDecoder::decode_fast() {
  DecodedOperand rd;
  DecodedOperand ra;

  switch (op) {
  case 0x00 ... 0x0e:
  case 0x10 ... 0x3f: {
    // Arithmetic: add, or, adc, sbb, and, sub, xor, cmp
    // Low 3 bits of opcode determine the format:
    switch (bits(op, 0, 3)) {
    case 0: DECODE(eform, rd, b_mode); DECODE(gform, ra, b_mode); break;
    case 1: DECODE(eform, rd, v_mode); DECODE(gform, ra, v_mode); break;
    case 2: DECODE(gform, rd, b_mode); DECODE(eform, ra, b_mode); break;
    case 3: DECODE(gform, rd, v_mode); DECODE(eform, ra, v_mode); break;
    case 4: rd.type = OPTYPE_REG; rd.reg.reg = APR_al; DECODE(iform, ra, b_mode); break;
    case 5: DECODE(varreg_def32, rd, 0); DECODE(iform, ra, v_mode); break;
    default: throw InvalidOpcodeException(); break;
    }

    // add and sub always add carry from rc iff rc is not REG_zero
    static const byte translate_opcode[8] = {OP_add, OP_or, OP_add, OP_sub, OP_and, OP_sub, OP_xor, OP_sub};

    int subop = bits(op, 3, 3);
    int translated_opcode = translate_opcode[subop];
    int rcreg = ((subop == 2) | (subop == 3)) ? REG_cf : REG_zero;

    if (subop == 7) prefixes &= ~PFX_LOCK;
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem(translated_opcode, rd, ra, FLAGS_DEFAULT_ALU, rcreg, (subop == 7));
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x40 ... 0x4f: {
    // inc/dec in 32-bit mode only: for x86-64 this is not possible since it's the REX prefix
    ra.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    int r = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    

    this << TransOp(bit(op, 3) ? OP_sub : OP_add, r, r, REG_imm, REG_zero, sizeshift, +1, 0, SETFLAG_ZF|SETFLAG_OF); // save old rdreg
    break;
    break;
  }

  case 0x50 ... 0x5f: {
    // push (0x50..0x57) or pop (0x58..0x5f) reg (defaults to 64 bit; pushing bytes not possible)
    ra.gform_ext(*this, v_mode, bits(op, 0, 3), use64, true);
    int r = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    if (use64 && (sizeshift == 2)) sizeshift = 3; // There is no way to encode 32-bit pushes and pops in 64-bit mode:
    int size = (1 << sizeshift);
    

    if (op < 0x58) {
      // push
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), size);
    } else {
      // pop
      this << TransOp(OP_ld, r, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
      if (r != REG_rsp) {
        // Only update %rsp if the target register is not itself %rsp
        this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), size);
      }
    }
    break;
  }

  case 0x1b6 ... 0x1b7: {
    // zero extensions: movzx rd,byte / movzx rd,word
    int bytemode = (op == 0x1b6) ? b_mode : v_mode;
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, bytemode);
    int rasizeshift = bit(op, 0);
    
    signext_reg_or_mem(rd, ra, rasizeshift, true);
    break;
  }

  case 0x63: 
  case 0x1be ... 0x1bf: {
    // sign extensions: movsx movsxd
    int bytemode = (op == 0x1be) ? b_mode : v_mode;
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, bytemode);
    int rasizeshift = (op == 0x63) ? 2 : (op == 0x1be) ? 0 : (op == 0x1bf) ? 1 : 3;
    
    signext_reg_or_mem(rd, ra, rasizeshift);
    break;
  }

  case 0x68:
  case 0x6a: {
    // push immediate
    DECODE(iform64, ra, (op == 0x68) ? v_mode : b_mode);
    int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
    int size = (1 << sizeshift);
    

    int r = REG_temp0;
    immediate(r, (op == 0x68) ? 2 : 0, ra.imm.imm);

    this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

    break;
  }

  case 0x69:
  case 0x6b: {
    // multiplies with three operands including an immediate
    // 0x69: imul reg16/32/64, rm16/32/64, simm16/simm32
    // 0x6b: imul reg16/32/64, rm16/32/64, simm8
    int bytemode = (op == 0x6b) ? b_mode : v_mode;

    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);

    DecodedOperand rimm;
    DECODE(iform, rimm, bytemode);

    
    alu_reg_or_mem(OP_mull, rd, ra, SETFLAG_CF|SETFLAG_OF, REG_imm, false, false, true, rimm.imm.imm);
    break;
  }

  case 0x1af: {
    // multiplies with two operands
    // 0x69: imul reg16/32/64, rm16/32/64
    // 0x6b: imul reg16/32/64, rm16/32/64
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    int rdreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int rdshift = reginfo[rd.reg.reg].sizeshift;

    
    alu_reg_or_mem(OP_mull, rd, ra, SETFLAG_CF|SETFLAG_OF, (rdshift < 2) ? rdreg : REG_zero);
    break;
  }

  case 0x70 ... 0x7f:
  case 0x180 ... 0x18f: {
    // near conditional branches with 8-bit or 32-bit displacement:
    DECODE(iform, ra, (inrange(op, 0x180, 0x18f) ? v_mode : b_mode));
    
    if (!last_flags_update_was_atomic) 
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    int condcode = bits(op, 0, 4);
    TransOp transop(OP_br, REG_rip, cond_code_to_flag_regs[condcode].ra, cond_code_to_flag_regs[condcode].rb, REG_zero, 3, 0);
    transop.cond = condcode;
    transop.riptaken = (Waddr)rip + ra.imm.imm;
    transop.ripseq = (Waddr)rip;
    this << transop;
    end_of_block = true;
    break;
  }

  case 0x80 ... 0x83: {
    // GRP1b, GRP1s, GRP1ss:
    switch (bits(op, 0, 2)) {
    case 0: DECODE(eform, rd, b_mode); DECODE(iform, ra, b_mode); break; // GRP1b
    case 1: DECODE(eform, rd, v_mode); DECODE(iform, ra, v_mode); break; // GRP1S
    case 2: throw InvalidOpcodeException(); break;
    case 3: DECODE(eform, rd, v_mode); DECODE(iform, ra, b_mode); break; // GRP1Ss (sign ext byte)
    }
    // function in modrm.reg: add or adc sbb and sub xor cmp
    

    // add and sub always add carry from rc iff rc is not REG_zero
    static const byte translate_opcode[8] = {OP_add, OP_or, OP_add, OP_sub, OP_and, OP_sub, OP_xor, OP_sub};

    int subop = modrm.reg;
    int translated_opcode = translate_opcode[subop];
    int rcreg = ((subop == 2) | (subop == 3)) ? REG_cf : REG_zero;

    if (subop == 7) prefixes &= ~PFX_LOCK;
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem(translated_opcode, rd, ra, FLAGS_DEFAULT_ALU, rcreg, (subop == 7));
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x84 ... 0x85: {
    // test
    DECODE(eform, rd, (op & 1) ? v_mode : b_mode);
    DECODE(gform, ra, (op & 1) ? v_mode : b_mode);
    
    alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
    break;
  }

  case 0x88 ... 0x8b: {
    // moves
    int bytemode = bit(op, 0) ? v_mode : b_mode;
    switch (bit(op, 1)) {
    case 0: DECODE(eform, rd, bytemode); DECODE(gform, ra, bytemode); break;
    case 1: DECODE(gform, rd, bytemode); DECODE(eform, ra, bytemode); break;
    }
    
    move_reg_or_mem(rd, ra);
    break;
  }

  case 0x8d: {
    // lea (zero extends result: no merging)
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    
    int destreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;

    ra.mem.size = sizeshift;

    address_generate_and_load_or_store(destreg, REG_zero, ra, OP_add);
    break;
  }

  case 0x8f: {
    // pop Ev: pop to reg or memory
    DECODE(eform, rd, v_mode);
    

    prefixes &= ~PFX_LOCK;
    int sizeshift = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;
    if (use64 && (sizeshift == 2)) sizeshift = 3; // There is no way to encode 32-bit pushes and pops in 64-bit mode:
    int rdreg = (rd.type == OPTYPE_REG) ? arch_pseudo_reg_to_arch_reg[rd.reg.reg] : REG_temp7;

    this << TransOp(OP_ld, rdreg, REG_rsp, REG_imm, REG_zero, sizeshift, 0);

    //
    // Special ordering semantics: if the destination is memory
    // and in [base + index*scale + offs], the base is rsp,
    // rsp is incremented *before* calculating the store address.
    // To maintain idempotent atomic semantics, we simply add
    // 2/4/8 to the immediate in this case.
    //
    if unlikely ((rd.type == OPTYPE_MEM) && (arch_pseudo_reg_to_arch_reg[rd.mem.basereg] == REG_rsp))
      rd.mem.offset += (1 << sizeshift);

    // There is no way to encode 32-bit pushes and pops in 64-bit mode:
    if (use64 && rd.type == OPTYPE_MEM && rd.mem.size == 2) rd.mem.size = 3;

    if (rd.type == OPTYPE_MEM) {
      prefixes &= ~PFX_LOCK;
      result_store(REG_temp7, REG_temp0, rd);
      this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));
    } else {
      // Only update %rsp if the target register (if any) itself is not itself %rsp
      if (rdreg != REG_rsp) this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));
    }

    break;
  }

  case 0x90: {
    // 0x90 (xchg eax,eax) is a NOP and in x86-64 is treated as such (i.e. does not zero upper 32 bits as usual)
    
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x98: {
    // cbw cwde cdqe
    int rashift = (opsize_prefix) ? 0 : ((rex.mode64) ? 2 : 1);
    int rdshift = rashift + 1;
    
    TransOp transop(OP_maskb, REG_rax, (rdshift < 3) ? REG_rax : REG_zero, REG_rax, REG_imm, rdshift, 0, MaskControlInfo(0, (1<<rashift)*8, 0));
    transop.cond = 2; // sign extend
    this << transop;
    break;
  }

  case 0x99: {
    // cwd cdq cqo
    
    int rashift = (opsize_prefix) ? 1 : ((rex.mode64) ? 3 : 2);

    TransOp bt(OP_bt, REG_temp0, REG_rax, REG_imm, REG_zero, rashift, ((1<<rashift)*8)-1, 0, SETFLAG_CF);
    bt.nouserflags = 1; // it still generates flags, but does not rename the user flags
    this << bt;

    TransOp sel(OP_sel, REG_temp0, REG_zero, REG_imm, REG_temp0, 3, -1LL);
    sel.cond = COND_c;
    this << sel; //, endl; jld

    // move in value
    this << TransOp(OP_mov, REG_rdx, (rashift < 2) ? REG_rdx : REG_zero, REG_temp0, REG_zero, rashift);

    // zero out high bits of rax since technically both rdx and rax are modified:
    if (rashift == 2) this << TransOp(OP_mov, REG_rax, REG_zero, REG_rax, REG_zero, 2);
    break;
  }

  case 0x9e: { // sahf: %flags[7:0] = %ah
    // extract value from %ah
    
    this << TransOp(OP_maskb, REG_temp0, REG_zero, REG_rax, REG_imm, 3, 0, MaskControlInfo(0, 8, 8));
    // only low 8 bits affected (OF not included)
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_temp0, REG_zero, 3, 0, 0, SETFLAG_ZF|SETFLAG_CF);
    break;
  }

  case 0x9f: { // lahf: %ah = %flags[7:0]
    
    this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    this << TransOp(OP_maskb, REG_rax, REG_rax, REG_temp0, REG_imm, 3, 0, MaskControlInfo(56, 8, 56));
    break;
  }

  case 0xa0 ... 0xa3: {
    // mov rAX,Ov and vice versa
    prefixes &= ~PFX_LOCK;
    rd.gform_ext(*this, (op & 1) ? v_mode : b_mode, REG_rax);
    DECODE(iform64, ra, (use64 ? q_mode : addrsize_prefix ? w_mode : d_mode));
    

    ra.mem.offset = ra.imm.imm;
    ra.mem.offset = (use64) ? ra.mem.offset : lowbits(ra.mem.offset, (addrsize_prefix) ? 16 : 32);
    ra.mem.basereg = APR_zero;
    ra.mem.indexreg = APR_zero;
    ra.mem.scale = APR_zero;
    ra.mem.size = reginfo[rd.reg.reg].sizeshift;
    ra.type = OPTYPE_MEM;
    prefixes &= ~PFX_LOCK;
    if (inrange(op, 0xa2, 0xa3)) {
      result_store(REG_rax, REG_temp0, ra);
    } else {
      move_reg_or_mem(rd, ra);
    }
    break;
  }

  case 0xa8 ... 0xa9: {
    // test al|ax,imm8|immV
    rd.gform_ext(*this, (op & 1) ? v_mode : b_mode, REG_rax);
    DECODE(iform, ra, (op & 1) ? v_mode : b_mode);
    
    alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
    break;
  }

  case 0xb0 ... 0xb7: {
    // mov reg,imm8
    rd.gform_ext(*this, b_mode, bits(op, 0, 3), false, true);
    DECODE(iform, ra, b_mode);
    
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    this << TransOp(OP_mov, rdreg, rdreg, REG_imm, REG_zero, 0, ra.imm.imm);
    break;
  }

  case 0xb8 ... 0xbf: {
    // mov reg,imm16|imm32|imm64
    rd.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    DECODE(iform64, ra, v_mode);
    
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;
    this << TransOp(OP_mov, rdreg, (sizeshift >= 2) ? REG_zero : rdreg, REG_imm, REG_zero, sizeshift, ra.imm.imm);
    break;
  }

  case 0xc0 ... 0xc1: 
  case 0xd0 ... 0xd1: 
  case 0xd2 ... 0xd3: {
    /*
      rol ror rcl rcr shl shr shl sar:
      Shifts and rotates, either by an imm8, implied 1, or %cl

      The shift and rotate instructions have some of the most bizarre semantics in the
      entire x86 instruction set: they may or may not modify flags depending on the
      rotation count operand, which we may not even know until the instruction
      issues. The specific rules are as follows:

      - If the count is zero, no flags are modified
      - If the count is one, both OF and CF are modified.
      - If the count is greater than one, only the CF is modified.
      (Technically the value in OF is undefined, but on K8 and P4,
      it retains the old value, so we try to be compatible).
      - Shifts also alter the ZAPS flags while rotates do not.

      For constant counts, this is easy to determine while translating:
        
      op   rd = ra,0       op rd = ra,1              op rd = ra,N
      Becomes:             Becomes:                  Becomes
      (nop)                op rd = ra,1 [set of cf]  op rd = ra,N [set cf]
        
      For variable counts, things are more complex. Since the shift needs
      to determine its output flags at runtime based on both the shift count
      and the input flags (CF, OF, ZAPS), we need to specify the latest versions
      in program order of all the existing flags. However, this would require
      three operands to the shift uop not even counting the value and count
      operands.
        
      Therefore, we use a collcc (collect flags) uop to get all
      the most up to date flags into one result, using three operands for
      ZAPS, CF, OF. This forms a zero word with all the correct flags
      attached, which is then forwarded as the rc operand to the shift.
        
      This may add additional scheduling constraints in the case that one
      of the operands to the shift itself sets the flags, but this is
      fairly rare (generally the shift amount is read from a table and
      loads don't generate flags.
        
      Conveniently, this also lets us directly implement the 65-bit
      rcl/rcr uops in hardware with little additional complexity.
        
      Example:
        
      shl         rd,rc
        
      Becomes:
        
      collcc       t0 = zf,cf,of
      sll<size>   rd = rd,rc,t0

    */

    DECODE(eform, rd, bit(op, 0) ? v_mode : b_mode);
    if (inrange(op, 0xc0, 0xc1)) {
      // byte immediate
      DECODE(iform, ra, b_mode);
    } else if (inrange(op, 0xd0, 0xd1)) {
      ra.type = OPTYPE_IMM;
      ra.imm.imm = 1;
    } else {
      ra.type = OPTYPE_REG;
      ra.reg.reg = APR_cl;
    }

    // Mask off the appropriate number of immediate bits:
    int size = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;
    ra.imm.imm = bits(ra.imm.imm, 0, (size == 3) ? 6 : 5);
    int count = ra.imm.imm;

    bool isrot = (bit(modrm.reg, 2) == 0);

    //
    // Variable shifts/rotates set all flags except OF, possibly merging them with some
    // of the earlier flag values in program order depending on the count. Otherwise
    // the static count (0, 1, >1) determines which flags are set.
    //    
    // Variable shifts/rotates don't set the OF flag because it is the most difficult to
    // match with Simics behavior.  Therefore don't set it to prevent spurious incorrect
    // execution events in pyrite.
    //
    W32 setflags = (ra.type == OPTYPE_REG) ? (isrot ? SETFLAG_CF : (SETFLAG_ZF|SETFLAG_CF)) :
      (!count) ? 0 : // count == 0
      (count == 1) ? (isrot ? (SETFLAG_OF|SETFLAG_CF) : FLAGS_DEFAULT_ALU) : // count == 1
      (isrot ? (SETFLAG_CF) : (SETFLAG_ZF|SETFLAG_CF)); // count > 1

    static const byte translate_opcode[8] = {OP_rotl, OP_rotr, OP_rotcl, OP_rotcr, OP_shl, OP_shr, OP_shl, OP_sar};
    static const byte translate_simple_opcode[8] = {OP_nop, OP_nop, OP_nop, OP_nop, OP_shls, OP_shrs, OP_shls, OP_sars};

    bool simple = ((ra.type == OPTYPE_IMM) & (ra.imm.imm <= SIMPLE_SHIFT_LIMIT) & (translate_simple_opcode[modrm.reg] != OP_nop));
    int translated_opcode = (simple) ? translate_simple_opcode[modrm.reg] : translate_opcode[modrm.reg];

    

    // Generate the flag collect uop here:
    if (ra.type == OPTYPE_REG) {
      TransOp collcc(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      collcc.nouserflags = 1;
      this << collcc;
    }
    int rcreg = (ra.type == OPTYPE_REG) ? REG_temp5 : (translated_opcode == OP_rotcl || translated_opcode == OP_rotcr) ? REG_cf : REG_zero;

    alu_reg_or_mem(translated_opcode, rd, ra, setflags, rcreg);

    break;
  }

  case 0xc2 ... 0xc3: {
    // ret near, with and without pop count
    int addend = 0;
    if (op == 0xc2) {
      DECODE(iform, ra, w_mode);
      addend = (W16)ra.imm.imm;
    }

    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);
    int size = (1 << sizeshift);
    addend = size + addend;

    

    this << TransOp(OP_ld, REG_temp7, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
    this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, addend);
    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    TransOp jmp(OP_jmp, REG_rip, REG_temp7, REG_zero, REG_zero, 3);
    jmp.extshift = BRANCH_HINT_POP_RAS;
    this << jmp;

    end_of_block = true;

    break;
  }

  case 0xc6 ... 0xc7: {
    // move reg_or_mem,imm8|imm16|imm32|imm64 (signed imm for 32-bit to 64-bit form)
    int bytemode = bit(op, 0) ? v_mode : b_mode;
    DECODE(eform, rd, bytemode); DECODE(iform, ra, bytemode);
    
    move_reg_or_mem(rd, ra);
    break;
  }

  case 0xc8: {
    // enter imm16,imm8
    // Format: 0xc8 imm16 imm8
    DECODE(iform, rd, w_mode);
    DECODE(iform, ra, b_mode);
    int bytes = (W16)rd.imm.imm;
    int level = (byte)ra.imm.imm;
    // we only support nesting level 0
    if (level != 0) throw InvalidOpcodeException();

    

    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);

    // Exactly equivalent to:
    // push %rbp
    // mov %rbp,%rsp
    // sub %rsp,imm8

    this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_rbp, sizeshift, -(1 << sizeshift));
    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));

    this << TransOp(OP_mov, REG_rbp, REG_zero, REG_rsp, REG_zero, sizeshift);
    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, bytes);
    break;
  }

  case 0xc9: {
    // leave
    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);
    // Exactly equivalent to:
    // mov %rsp,%rbp
    // pop %rbp

    

#if 0
    // Make idempotent by checking new rsp (aka rbp) alignment first:
    this << TransOp(OP_ld, REG_temp0, REG_rbp, REG_imm, REG_zero, sizeshift, 0);
#endif

    this << TransOp(OP_mov, REG_rsp, REG_zero, REG_rbp, REG_zero, sizeshift);
    this << TransOp(OP_ld, REG_rbp, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
    this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));

    break;
  }

  case 0xe8:
  case 0xe9:
  case 0xeb: {
    bool iscall = (op == 0xe8);
    // CALL or JMP rel16/rel32/rel64
    // near conditional branches with 8-bit displacement:
    DECODE(iform, ra, (op == 0xeb) ? b_mode : v_mode);
    

    //bb.rip_taken = (Waddr)rip + (W64s)ra.imm.imm; jld
    //bb.rip_not_taken = bb.rip_taken; jld

    int sizeshift = (use64) ? 3 : 2;

    if (iscall) {
      immediate(REG_temp0, 3, (Waddr)rip);
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp0, sizeshift, -(1 << sizeshift));
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, (1 << sizeshift));
    }

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    TransOp transop(OP_bru, REG_rip, REG_zero, REG_zero, REG_zero, 3);
    transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
    transop.riptaken = (Waddr)((W64s)rip + ra.imm.imm);
    transop.ripseq = (iscall) ? (Waddr)rip : (Waddr)((W64s)rip + ra.imm.imm);
    this << transop;
    //bb.rip_taken = (Waddr)rip + (W64s)ra.imm.imm; jld
    //bb.rip_not_taken = bb.rip_taken; jld

    end_of_block = true;
    break;
  }

  case 0xf5: {
    // cmc
    // TransOp(int opcode, int rd, int ra, int rb, int rc, int size, W64s rbimm = 0, W64s rcimm = 0, W32 setflags = 0)
    
    this << TransOp(OP_xorcc, REG_temp0, REG_cf, REG_imm, REG_zero, 3, FLAG_CF, 0, SETFLAG_CF);
    break;
  }

    //
    // NOTE: Some forms of this are handled by the complex decoder:
    //
  case 0xf6 ... 0xf7: {
    // COMPLEX: handle mul and div in the complex decoder
    if (modrm.reg >= 4) return false;

    // GRP3b and GRP3S
    DECODE(eform, rd, (op & 1) ? v_mode : b_mode);
    

    switch (modrm.reg) {
    case 0: // test
      DECODE(iform, ra, (op & 1) ? v_mode : b_mode);
      
      alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
      break;
    case 1: // (invalid)
      throw UnimplementedOpcodeException();
      break;
    case 2: { // not
      // As an exception to the rule, NOT does not generate any flags. Go figure.
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem(OP_nor, rd, rd, 0, REG_zero);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }
      break;
    }
    case 3: { // neg r1 => sub r1 = 0, r1
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem(OP_sub, rd, rd, FLAGS_DEFAULT_ALU, REG_zero, false, true);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }
      break;
    }
    default:
      abort();
    }
    break;
  }

  case 0xf8: { // clc
    
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_imm, REG_zero, 3, 0, 0, SETFLAG_CF);
    break;
  }
  case 0xf9: { // stc
    
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_imm, REG_zero, 3, FLAG_CF, 0, SETFLAG_CF);
    break;
  }

  case 0xfe: {
    // Group 4: inc/dec Eb in register or memory
    // Increments are unusual in that they do NOT update CF.
    DECODE(eform, rd, b_mode);
    
    ra.type = OPTYPE_IMM;
    ra.imm.imm = +1;

    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem((bit(modrm.reg, 0)) ? OP_sub : OP_add, rd, ra, SETFLAG_ZF|SETFLAG_OF, REG_zero);
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0xff: {
    switch (modrm.reg) {
    case 0:
    case 1: {
      // inc/dec Ev in register or memory
      // Increments are unusual in that they do NOT update CF.
      DECODE(eform, rd, v_mode);
      
      ra.type = OPTYPE_IMM;
      ra.imm.imm = +1;

      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem((bit(modrm.reg, 0)) ? OP_sub : OP_add, rd, ra, SETFLAG_ZF|SETFLAG_OF, REG_zero);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

      break;
    }
    case 2:
    case 4: {
      bool iscall = (modrm.reg == 2);
      // call near Ev
      DECODE(eform, ra, v_mode);
      
      if (!last_flags_update_was_atomic)
        this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

      int sizeshift = (use64) ? 3 : 2;
      if (ra.type == OPTYPE_REG) {
        int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
        int rashift = reginfo[ra.reg.reg].sizeshift;
        // there is no way to encode a 32-bit jump address in x86-64 mode:
        if (use64 && (rashift == 2)) rashift = 3;
        if (iscall) {
          immediate(REG_temp6, 3, (Waddr)rip);
          this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp6, sizeshift, -(1 << sizeshift));
          this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, 1 << sizeshift);
        }
        // We do not know the taken or not-taken directions yet so just leave them as zero:
        TransOp transop(OP_jmp, REG_rip, rareg, REG_zero, REG_zero, rashift);
        transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
        transop.ripseq = (iscall) ? (Waddr)rip : transop.ripseq;
        this << transop;
      } else if (ra.type == OPTYPE_MEM) {
        // there is no way to encode a 32-bit jump address in x86-64 mode:
        if (use64 && (ra.mem.size == 2)) ra.mem.size = 3;
        prefixes &= ~PFX_LOCK;
        operand_load(REG_temp0, ra);
        if (iscall) {
          immediate(REG_temp6, 3, (Waddr)rip);
          this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp6, sizeshift, -(1 << sizeshift));
          this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, 1 << sizeshift);
        }
        // We do not know the taken or not-taken directions yet so just leave them as zero:
        TransOp transop(OP_jmp, REG_rip, REG_temp0, REG_zero, REG_zero, ra.mem.size);
        transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
        transop.ripseq = (iscall) ? (Waddr)rip : transop.ripseq;
        this << transop;
      }

      end_of_block = true;
      break;
    }
    case 6: {
      // push Ev: push reg or memory
      DECODE(eform, ra, v_mode);
      

      // There is no way to encode 32-bit pushes and pops in 64-bit mode:
      if (use64 && ra.type == OPTYPE_MEM && ra.mem.size == 2) ra.mem.size = 3;

      int rareg;

      if (ra.type == OPTYPE_REG) {
        rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      } else {
        rareg = REG_temp7;
        prefixes &= ~PFX_LOCK;
        operand_load(rareg, ra);
      }

      int sizeshift = (ra.type == OPTYPE_REG) ? reginfo[ra.reg.reg].sizeshift : ra.mem.size;
      if (use64 && sizeshift == 2) sizeshift = 3; // There is no way to encode 32-bit pushes and pops in 64-bit mode:
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, rareg, sizeshift, -(1 << sizeshift));
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), (1 << sizeshift));

      break;
    }
    default:
      throw UnimplementedOpcodeException();
      break;
    }
    break;
  }

  case 0x140 ... 0x14f: {
    // cmov: conditional moves
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    

    int srcreg;
    int destreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;

    if (ra.type == OPTYPE_REG) {
      srcreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    } else {
      assert(ra.type == OPTYPE_MEM);
      prefixes &= ~PFX_LOCK;
      operand_load(REG_temp7, ra);
      srcreg = REG_temp7;
    }

    int condcode = bits(op, 0, 4);
    const CondCodeToFlagRegs& cctfr = cond_code_to_flag_regs[condcode];

    int condreg;
    if (cctfr.req2) {
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      condreg = REG_temp0;
    } else {
      condreg = (cctfr.ra != REG_zero) ? cctfr.ra : cctfr.rb;
    }
    assert(condreg != REG_zero);

    TransOp transop(OP_sel, destreg, destreg, srcreg, condreg, sizeshift);
    transop.cond = condcode;
    this << transop;
    break;
  }

  case 0x190 ... 0x19f: {
    // conditional sets
    DECODE(eform, rd, v_mode);
    

    int r;

    if (rd.type == OPTYPE_REG) {
      r = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    } else {
      assert(rd.type == OPTYPE_MEM);
      r = REG_temp7;
    }

    int condcode = bits(op, 0, 4);
    const CondCodeToFlagRegs& cctfr = cond_code_to_flag_regs[condcode];

    int condreg;
    if (cctfr.req2) {
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      condreg = REG_temp0;
    } else {
      condreg = (cctfr.ra != REG_zero) ? cctfr.ra : cctfr.rb;
    }
    assert(condreg != REG_zero);

    TransOp transop(OP_set, r, (rd.type == OPTYPE_MEM) ? REG_zero : r, REG_imm, condreg, 0, +1);
    transop.cond = condcode;
    this << transop;

    if (rd.type == OPTYPE_MEM) {
      rd.mem.size = 0;
      prefixes &= ~PFX_LOCK;
      result_store(r, REG_temp0, rd);
    }
    break;
  }

  case 0x1a3: // bt ra,rb     101 00 011
  case 0x1ab: // bts ra,rb    101 01 011
  case 0x1b3: // btr ra,rb    101 10 011
  case 0x1bb: { // btc ra,rb  101 11 011
    // COMPLEX: Let complex decoder handle memory forms
    if (modrm.mod != 3) return false;

    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
    

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[bits(op, 3, 2)];

    assert(rd.type == OPTYPE_REG);
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    
    // bt has no output - just flags:
    this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, rareg, REG_zero, 3, 0, 0, SETFLAG_CF);
    break;
  }

  case 0x1ba: { // bt|btc|btr|bts ra,imm
    // COMPLEX: Let complex decoder handle memory forms
    if (modrm.mod != 3) return false;

    DECODE(eform, rd, v_mode);
    DECODE(iform, ra, b_mode);
    if (modrm.reg < 4) throw UnimplementedOpcodeException();
    

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[lowbits(modrm.reg, 2)];

    assert(rd.type == OPTYPE_REG);
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    // bt has no output - just flags:
    this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, REG_imm, REG_zero, 3, ra.imm.imm, 0, SETFLAG_CF);
    break;
  }

  case 0x118: {
    // prefetchN [eform]
    DECODE(eform, ra, b_mode);
    

    static const byte x86_prefetch_to_pt2x_cachelevel[8] = {2, 1, 2, 3};
    int level = x86_prefetch_to_pt2x_cachelevel[modrm.reg];
    prefixes &= ~PFX_LOCK;
#if 0
    operand_load(REG_temp0, ra, OP_ld_pre, level);
#endif
    break;
  }

  case 0x10d: {
    // prefetchw [eform] (NOTE: this is an AMD-only insn from K6 onwards)
    DECODE(eform, ra, b_mode);
    

    int level = 2;
    prefixes &= ~PFX_LOCK;
#if 0
    operand_load(REG_temp0, ra, OP_ld_pre, level);
#endif
    break;
  }

  case 0x1bc: 
  case 0x1bd: {
    // bsf/bsr:
    DECODE(gform, rd, v_mode); DECODE(eform, ra, v_mode);
    
    alu_reg_or_mem((op == 0x1bc) ? OP_ctz: OP_clz, rd, ra, FLAGS_DEFAULT_ALU, REG_zero);
    break;
  }

  case 0x1c8 ... 0x1cf: {
    // bswap
    rd.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;
    this << TransOp(OP_bswap, rdreg, (sizeshift >= 2) ? REG_zero : rdreg, rdreg, REG_zero, sizeshift);
    break;
  }

#if 1
  case 0x119 ... 0x11f: {
    // Special form of NOP with ModRM form (used for padding)
    DECODE(eform, ra, v_mode);
    //EndOfDecode();
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }
#endif

  default: {
    // Let the slow decoder handle it or mark it invalid
    return false;
  }
  }

  return true;
}