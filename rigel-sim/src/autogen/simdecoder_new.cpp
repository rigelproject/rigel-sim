#include "autogen/autogen_isa_sim.h"
#include "sim.h"

#include "instr.h"
uint32_t StaticDecodeInfo::optype() const {
  return (raw_instr_bits & 0xF0000000) >> 28;
}

uint32_t StaticDecodeInfo::opcode() const {
  switch(optype()) {
    // OPTYPE 0x0
    case 0x0:
      return (raw_instr_bits >> 0) & 0xff; break;
    // OPTYPE 0x1
    case 0x1:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x2
    case 0x2:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x3
    case 0x3:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x4
    case 0x4:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x5
    case 0x5:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x6
    case 0x6:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x7
    case 0x7:
      return (raw_instr_bits >> 26) & 0x3; break;
    // OPTYPE 0x8
    case 0x8:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0x9
    case 0x9:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0xa
    case 0xa:
      return (raw_instr_bits >> 16) & 0x3; break;
    // OPTYPE 0xb
    case 0xb:
      return (raw_instr_bits >> 0) & 0xff; break;
    // OPTYPE 0xc
    case 0xc:
      return (raw_instr_bits >> 16) & 0x3; break;
default: //error 
  break;
  }
	assert(0 && "Unrecognized opcode!");
	return 0;
}

int StaticDecodeInfo::sim_decode_type() {
using namespace simconst; 
  switch (optype()) {
    // OPTYPE 0x0
     case 0x0:
     {
       switch (opcode()) {
         case 0x0:  // add
            setType(I_ADD); goto decode_done; break;
         case 0x1:  // sub
            setType(I_SUB); goto decode_done; break;
         case 0x2:  // addu
            setType(I_ADDU); goto decode_done; break;
         case 0x3:  // subu
            setType(I_SUBU); goto decode_done; break;
         case 0x4:  // addc
            setType(I_ADDC); goto decode_done; break;
         case 0x5:  // addg
            setType(I_ADDG); goto decode_done; break;
         case 0x6:  // addcu
            setType(I_ADDCU); goto decode_done; break;
         case 0x7:  // addgu
            setType(I_ADDGU); goto decode_done; break;
         case 0x8:  // and
            setType(I_AND); goto decode_done; break;
         case 0x9:  // or
            setType(I_OR); goto decode_done; break;
         case 0xa:  // xor
            setType(I_XOR); goto decode_done; break;
         case 0xb:  // nor
            setType(I_NOR); goto decode_done; break;
         case 0xc:  // sll
            setType(I_SLL); goto decode_done; break;
         case 0xd:  // srl
            setType(I_SRL); goto decode_done; break;
         case 0xe:  // sra
            setType(I_SRA); goto decode_done; break;
         case 0xf:  // slli
            setType(I_SLLI); goto decode_done; break;
         case 0x10:  // srli
            setType(I_SRLI); goto decode_done; break;
         case 0x11:  // srai
            setType(I_SRAI); goto decode_done; break;
         case 0x12:  // ceq
            setType(I_CEQ); goto decode_done; break;
         case 0x13:  // clt
            setType(I_CLT); goto decode_done; break;
         case 0x14:  // cle
            setType(I_CLE); goto decode_done; break;
         case 0x15:  // cltu
            setType(I_CLTU); goto decode_done; break;
         case 0x16:  // cleu
            setType(I_CLEU); goto decode_done; break;
         case 0x17:  // ceqf
            setType(I_CEQF); goto decode_done; break;
         case 0x18:  // cltf
            setType(I_CLTF); goto decode_done; break;
         case 0x19:  // cltef
            setType(I_CLTEF); goto decode_done; break;
         case 0x1a:  // cmov.eq
            setType(I_CMEQ); goto decode_done; break;
         case 0x1b:  // cmov.neq
            setType(I_CMNE); goto decode_done; break;
         case 0x1c:  // mfsr
            setType(I_MFSR); goto decode_done; break;
         case 0x1d:  // mtsr
            setType(I_MTSR); goto decode_done; break;
         case 0x1e:  // jmpr
            setType(I_JMPR); goto decode_done; break;
         case 0x1f:  // jalr
            setType(I_JALR); goto decode_done; break;
         case 0x20:  // ldl
            setType(I_LDL); goto decode_done; break;
         case 0x21:  // stc
            setType(I_STC); goto decode_done; break;
         case 0x22:  // atom.cas
            setType(I_ATOMCAS); goto decode_done; break;
         case 0x23:  // atom.addu
            setType(I_ATOMADDU); goto decode_done; break;
         case 0x24:  // atom.xor
            setType(I_ATOMXOR); goto decode_done; break;
         case 0x25:  // atom.or
            setType(I_ATOMOR); goto decode_done; break;
         case 0x26:  // atom.and
            setType(I_ATOMAND); goto decode_done; break;
         case 0x27:  // atom.max
            setType(I_ATOMMAX); goto decode_done; break;
         case 0x28:  // atom.min
            setType(I_ATOMMIN); goto decode_done; break;
         case 0x29:  // tbloffset
            setType(I_TBLOFFSET); goto decode_done; break;
         case 0x2a:  // nop
            setType(I_NOP); goto decode_done; break;
         case 0x2b:  // brk
            setType(I_BRK); goto decode_done; break;
         case 0x2c:  // hlt
            setType(I_HLT); goto decode_done; break;
         case 0x2d:  // sync
            setType(I_SYNC); goto decode_done; break;
         case 0x2e:  // undef
            setType(I_UNDEF); goto decode_done; break;
         case 0x2f:  // cc.wb
            setType(I_CC_WB); goto decode_done; break;
         case 0x30:  // cc.inv
            setType(I_CC_INV); goto decode_done; break;
         case 0x31:  // cc.flush
            setType(I_CC_FLUSH); goto decode_done; break;
         case 0x32:  // ic.inv
            setType(I_IC_INV); goto decode_done; break;
         case 0x33:  // mb
            setType(I_MB); goto decode_done; break;
         case 0x34:  // rfe
            setType(I_RFE); goto decode_done; break;
         case 0x35:  // abort
            setType(I_ABORT); goto decode_done; break;
         case 0x36:  // syscall
            setType(I_SYSCALL); goto decode_done; break;
         case 0x37:  // printreg
            setType(I_PRINTREG); goto decode_done; break;
         case 0x38:  // timer.start
            setType(I_START_TIMER); goto decode_done; break;
         case 0x39:  // timer.stop
            setType(I_STOP_TIMER); goto decode_done; break;
         case 0x3a:  // flush.bcast
            setType(I_FLUSH_BCAST); goto decode_done; break;
         case 0x3b:  // line.wb
            setType(I_LINE_WB); goto decode_done; break;
         case 0x3c:  // line.inv
            setType(I_LINE_INV); goto decode_done; break;
         case 0x3d:  // line.flush
            setType(I_LINE_FLUSH); goto decode_done; break;
         case 0x3e:  // tq.enq
            setType(I_TQ_ENQUEUE); goto decode_done; break;
         case 0x3f:  // tq.deq
            setType(I_TQ_DEQUEUE); goto decode_done; break;
         case 0x40:  // tq.loop
            setType(I_TQ_LOOP); goto decode_done; break;
         case 0x41:  // tq.init
            setType(I_TQ_INIT); goto decode_done; break;
         case 0x42:  // tq.end
            setType(I_TQ_END); goto decode_done; break;
         case 0x43:  // fadd
            setType(I_FADD); goto decode_done; break;
         case 0x44:  // fsub
            setType(I_FSUB); goto decode_done; break;
         case 0x45:  // fmul
            setType(I_FMUL); goto decode_done; break;
         case 0x46:  // frcp
            setType(I_FRCP); goto decode_done; break;
         case 0x47:  // frsq
            setType(I_FRSQ); goto decode_done; break;
         case 0x48:  // fabs
            setType(I_FABS); goto decode_done; break;
         case 0x49:  // fmrs
            setType(I_FMRS); goto decode_done; break;
         case 0x4a:  // fmov
            setType(I_FMOV); goto decode_done; break;
         case 0x4b:  // fr2a
            setType(I_FR2A); goto decode_done; break;
         case 0x4c:  // f2i
            setType(I_F2I); goto decode_done; break;
         case 0x4d:  // i2f
            setType(I_I2F); goto decode_done; break;
         case 0x4e:  // mul
            setType(I_MUL); goto decode_done; break;
         case 0x4f:  // mul32
            setType(I_MUL32); goto decode_done; break;
         case 0x50:  // mul16
            setType(I_MUL16); goto decode_done; break;
         case 0x51:  // mul16.c
            setType(I_MUL16C); goto decode_done; break;
         case 0x52:  // mul16.g
            setType(I_MUL16G); goto decode_done; break;
         case 0x53:  // clz
            setType(I_CLZ); goto decode_done; break;
         case 0x54:  // zext8
            setType(I_ZEXTB); goto decode_done; break;
         case 0x55:  // sext8
            setType(I_SEXTB); goto decode_done; break;
         case 0x56:  // zext16
            setType(I_ZEXTS); goto decode_done; break;
         case 0x57:  // sext16
            setType(I_SEXTS); goto decode_done; break;
         case 0x58:  // vadd
            setType(I_VADD); goto decode_done; break;
         case 0x59:  // vsub
            setType(I_VSUB); goto decode_done; break;
         case 0x5a:  // vfadd
            setType(I_VFADD); goto decode_done; break;
         case 0x5b:  // vfsub
            setType(I_VFSUB); goto decode_done; break;
         case 0x5c:  // vfmul
            setType(I_VFMUL); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x1
     case 0x1:
     {
       switch (opcode()) {
         case 0x0:  // addi
            setType(I_ADDI); goto decode_done; break;
         case 0x1:  // subi
            setType(I_SUBI); goto decode_done; break;
         case 0x2:  // addiu
            setType(I_ADDIU); goto decode_done; break;
         case 0x3:  // subiu
            setType(I_SUBIU); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x2
     case 0x2:
     {
       switch (opcode()) {
         case 0x0:  // andi
            setType(I_ANDI); goto decode_done; break;
         case 0x1:  // ori
            setType(I_ORI); goto decode_done; break;
         case 0x2:  // xori
            setType(I_XORI); goto decode_done; break;
         case 0x3:  // beq
            setType(I_BEQ); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x3
     case 0x3:
     {
       switch (opcode()) {
         case 0x0:  // mvui
            setType(I_MVUI); goto decode_done; break;
         case 0x1:  // jal
            setType(I_JAL); goto decode_done; break;
         case 0x2:  // pref.l
            setType(I_PREF_L); goto decode_done; break;
         case 0x3:  // pref.nga
            setType(I_PREF_NGA); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x4
     case 0x4:
     {
       switch (opcode()) {
         case 0x0:  // bne
            setType(I_BNE); goto decode_done; break;
         case 0x1:  // pref.b.gc
            setType(I_PREF_B_GC); goto decode_done; break;
         case 0x2:  // pref.b.cc
            setType(I_PREF_B_CC); goto decode_done; break;
         case 0x3:  // pldw
            setType(I_PAUSE_LDW); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x5
     case 0x5:
     {
       switch (opcode()) {
         case 0x0:  // be
            setType(I_BE); goto decode_done; break;
         case 0x1:  // bnz
            setType(I_BN); goto decode_done; break;
         case 0x2:  // blt
            setType(I_BLT); goto decode_done; break;
         case 0x3:  // bgt
            setType(I_BGT); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x6
     case 0x6:
     {
       switch (opcode()) {
         case 0x0:  // ble
            setType(I_BLE); goto decode_done; break;
         case 0x1:  // bge
            setType(I_BGE); goto decode_done; break;
         case 0x2:  // jmp
            setType(I_JMP); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x7
     case 0x7:
     {
       switch (opcode()) {
         case 0x0:  // lj
            setType(I_LJ); goto decode_done; break;
         case 0x1:  // ljl
            setType(I_LJL); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x8
     case 0x8:
     {
       switch (opcode()) {
         case 0x0:  // bcast.i
            setType(I_BCAST_INV); goto decode_done; break;
         case 0x1:  // strtof
            setType(I_STRTOF); goto decode_done; break;
         case 0x2:  // event
            setType(I_EVENT); goto decode_done; break;
         case 0x3:  // prio
            setType(I_PRIO); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0x9
     case 0x9:
     {
       switch (opcode()) {
         case 0x0:  // bcast.u
            setType(I_BCAST_UPDATE); goto decode_done; break;
         case 0x1:  // ldw
            setType(I_LDW); goto decode_done; break;
         case 0x2:  // stw
            setType(I_STW); goto decode_done; break;
         case 0x3:  // g.ldw
            setType(I_GLDW); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0xa
     case 0xa:
     {
       switch (opcode()) {
         case 0x0:  // g.stw
            setType(I_GSTW); goto decode_done; break;
         case 0x1:  // atom.dec
            setType(I_ATOMDEC); goto decode_done; break;
         case 0x2:  // atom.inc
            setType(I_ATOMINC); goto decode_done; break;
         case 0x3:  // atom.xchg
            setType(I_ATOMXCHG); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0xb
     case 0xb:
     {
       switch (opcode()) {
         case 0x0:  // fmadd
            setType(I_FMADD); goto decode_done; break;
         case 0x1:  // fmsub
            setType(I_FMSUB); goto decode_done; break;
         case 0x2:  // fa2r
            setType(I_FA2R); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

    // OPTYPE 0xc
     case 0xc:
     {
       switch (opcode()) {
         case 0x0:  // vaddi
            setType(I_VADDI); goto decode_done; break;
         case 0x1:  // vsubi
            setType(I_VSUBI); goto decode_done; break;
         case 0x2:  // vldw
            setType(I_VLDW); goto decode_done; break;
         case 0x3:  // vstw
            setType(I_VSTW); goto decode_done; break;
         default: { goto unknown_opcode;  }
       }
     }

     default: { goto unknown_opcode;  }
   }
unknown_opcode:
  return 0;
decode_done:
  return 1;
} 


uint32_t StaticDecodeInfo::StaticDecode() {
switch(type()) {
case I_ADD:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_SUB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_ADDU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_SUBU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_ADDI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_SUBI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_ADDIU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_SUBIU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_ADDC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000207;
  break;
case I_ADDG:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x0000020b;
  break;
case I_ADDCU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000205;
  break;
case I_ADDGU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000209;
  break;
case I_AND:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_OR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_XOR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_NOR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_ANDI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_ORI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_XORI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_SLL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_SRL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_SRA:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_SLLI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 1;
  decode_flags = 0x00000201;
  break;
case I_SRLI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 1;
  decode_flags = 0x00000201;
  break;
case I_SRAI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 1;
  decode_flags = 0x00000203;
  break;
case I_CEQ:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004203;
  break;
case I_CLT:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004203;
  break;
case I_CLE:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004203;
  break;
case I_CLTU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004201;
  break;
case I_CLEU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004201;
  break;
case I_CEQF:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004201;
  break;
case I_CLTF:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004201;
  break;
case I_CLTEF:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00004201;
  break;
case I_CMEQ:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_CMNE:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_MFSR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000243;
  break;
case I_MTSR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_MVUI:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_BEQ:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00008203;
  break;
case I_BNE:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00008203;
  break;
case I_BE:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_BN:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_BLT:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_BGT:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_BLE:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_BGE:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_LJ:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 1;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_LJL:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 1;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000a03;
  break;
case I_JMP:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_JAL:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000a03;
  break;
case I_JMPR:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_JALR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000a03;
  break;
case I_LDL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000401;
  break;
case I_STC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00001001;
  break;
case I_PREF_L:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PREF_B_GC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PREF_B_CC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PREF_NGA:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PAUSE_LDW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_BCAST_INV:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_BCAST_UPDATE:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_LDW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_STW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00028003;
  break;
case I_GLDW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_GSTW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00028001;
  break;
case I_ATOMDEC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00040001;
  break;
case I_ATOMINC:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00040001;
  break;
case I_ATOMXCHG:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00058001;
  break;
case I_ATOMCAS:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ATOMADDU:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00040001;
  break;
case I_ATOMXOR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ATOMOR:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ATOMAND:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ATOMMAX:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ATOMMIN:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_TBLOFFSET:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_NOP:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_BRK:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_HLT:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00002001;
  break;
case I_SYNC:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_UNDEF:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_CC_WB:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_CC_INV:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_CC_FLUSH:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_IC_INV:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_MB:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_RFE:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ABORT:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_SYSCALL:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PRINTREG:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_START_TIMER:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_STOP_TIMER:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_STRTOF:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_FLUSH_BCAST:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_LINE_WB:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_LINE_INV:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_LINE_FLUSH:
  has_dreg   = 0;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_TQ_ENQUEUE:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_TQ_DEQUEUE:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_TQ_LOOP:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_TQ_INIT:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_TQ_END:
  has_dreg   = 0;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_FADD:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_FSUB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_FMUL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_FMADD:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000181;
  break;
case I_FMSUB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_FRCP:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_FRSQ:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_FABS:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_FMRS:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_FMOV:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_FA2R:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000101;
  break;
case I_FR2A:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000081;
  break;
case I_F2I:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_I2F:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000001;
  break;
case I_MUL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_MUL32:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_MUL16:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000003;
  break;
case I_MUL16C:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_MUL16G:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_CLZ:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_ZEXTB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_SEXTB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_ZEXTS:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000201;
  break;
case I_SEXTS:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000203;
  break;
case I_VADD:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VSUB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VADDI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VSUBI:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VFADD:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VFSUB:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VFMUL:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 1;
  has_imm26   = 0;
  has_imm16   = 0;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VLDW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_VSTW:
  has_dreg   = 1;
  has_sreg_t = 1;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_EVENT:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
case I_PRIO:
  has_dreg   = 1;
  has_sreg_t = 0;
  has_sreg_s = 0;
  has_imm26   = 0;
  has_imm16   = 1;
  has_imm5    = 0;
  decode_flags = 0x00000000;
  break;
default:
 assert(0 && "unknown type!");
 return 0; 
}
  return 1;
}
