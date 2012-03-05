#ifndef __DECODE_SWITCH_H__
#define __DECODE_SWITCH_H__
#include "autogen_isa_sim.h"
#include "instr.h"
#include "sim.h"
int sim_decode( InstrSlot instr ) {
using namespace simconst; 
  uint32_t optype, instr_bits;
  instr_bits = instr->get_raw_instr(); 
  optype = (instr_bits & 0xF0000000) >> 28; 
  switch (optype)                       
  {                                     
    /**************/                     
     case 0x0:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 0) & 0xff ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // add
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADD);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // sub
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_SUB);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // addu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADDU);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // subu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_SUBU);

           goto decode_done;      
           break;                 
                                  
         case 0x4:  // addc
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADDC);

           goto decode_done;      
           break;                 
                                  
         case 0x5:  // addg
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADDG);

           goto decode_done;      
           break;                 
                                  
         case 0x6:  // addcu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADDCU);

           goto decode_done;      
           break;                 
                                  
         case 0x7:  // addgu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ADDGU);

           goto decode_done;      
           break;                 
                                  
         case 0x8:  // and
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_AND);

           goto decode_done;      
           break;                 
                                  
         case 0x9:  // or
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_OR);

           goto decode_done;      
           break;                 
                                  
         case 0xa:  // xor
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_XOR);

           goto decode_done;      
           break;                 
                                  
         case 0xb:  // nor
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_NOR);

           goto decode_done;      
           break;                 
                                  
         case 0xc:  // sll
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_SLL);

           goto decode_done;      
           break;                 
                                  
         case 0xd:  // srl
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_SRL);

           goto decode_done;      
           break;                 
                                  
         case 0xe:  // sra
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_SRA);

           goto decode_done;      
           break;                 
                                  
         case 0xf:  // slli
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
                IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SLLI);

           goto decode_done;      
           break;                 
                                  
         case 0x10:  // srli
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
                IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SRLI);

           goto decode_done;      
           break;                 
                                  
         case 0x11:  // srai
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
                IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SRAI);

           goto decode_done;      
           break;                 
                                  
         case 0x12:  // ceq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CEQ);

           goto decode_done;      
           break;                 
                                  
         case 0x13:  // clt
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLT);

           goto decode_done;      
           break;                 
                                  
         case 0x14:  // cle
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLE);

           goto decode_done;      
           break;                 
                                  
         case 0x15:  // cltu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLTU);

           goto decode_done;      
           break;                 
                                  
         case 0x16:  // cleu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLEU);

           goto decode_done;      
           break;                 
                                  
         case 0x17:  // ceqf
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CEQF);

           goto decode_done;      
           break;                 
                                  
         case 0x18:  // cltf
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLTF);

           goto decode_done;      
           break;                 
                                  
         case 0x19:  // cltef
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CLTEF);

           goto decode_done;      
           break;                 
                                  
         case 0x1a:  // cmov.eq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CMEQ);

           goto decode_done;      
           break;                 
                                  
         case 0x1b:  // cmov.neq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_CMNE);

           goto decode_done;      
           break;                 
                                  
         case 0x1c:  // mfsr
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_MFSR);

           goto decode_done;      
           break;                 
                                  
         case 0x1d:  // mtsr
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_MTSR);

           goto decode_done;      
           break;                 
                                  
         case 0x1e:  // jmpr
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_JMPR);

           goto decode_done;      
           break;                 
                                  
         case 0x1f:  // jalr
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_JALR);

           goto decode_done;      
           break;                 
                                  
         case 0x20:  // ldl
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_LDL);

           goto decode_done;      
           break;                 
                                  
         case 0x21:  // stc
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_STC);

           goto decode_done;      
           break;                 
                                  
         case 0x22:  // atom.cas
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMCAS);

           goto decode_done;      
           break;                 
                                  
         case 0x23:  // atom.addu
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMADDU);

           goto decode_done;      
           break;                 
                                  
         case 0x24:  // atom.xor
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMXOR);

           goto decode_done;      
           break;                 
                                  
         case 0x25:  // atom.or
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMOR);

           goto decode_done;      
           break;                 
                                  
         case 0x26:  // atom.and
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMAND);

           goto decode_done;      
           break;                 
                                  
         case 0x27:  // atom.max
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMMAX);

           goto decode_done;      
           break;                 
                                  
         case 0x28:  // atom.min
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_ATOMMIN);

           goto decode_done;      
           break;                 
                                  
         case 0x29:  // tbloffset
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_TBLOFFSET);

           goto decode_done;      
           break;                 
                                  
         case 0x2a:  // nop
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_NOP);

           goto decode_done;      
           break;                 
                                  
         case 0x2b:  // brk
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_BRK);

           goto decode_done;      
           break;                 
                                  
         case 0x2c:  // hlt
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_HLT);

           goto decode_done;      
           break;                 
                                  
         case 0x2d:  // sync
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_SYNC);

           goto decode_done;      
           break;                 
                                  
         case 0x2e:  // undef
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_UNDEF);

           goto decode_done;      
           break;                 
                                  
         case 0x2f:  // cc.wb
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_CC_WB);

           goto decode_done;      
           break;                 
                                  
         case 0x30:  // cc.inv
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_CC_INV);

           goto decode_done;      
           break;                 
                                  
         case 0x31:  // cc.flush
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_CC_FLUSH);

           goto decode_done;      
           break;                 
                                  
         case 0x32:  // ic.inv
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_IC_INV);

           goto decode_done;      
           break;                 
                                  
         case 0x33:  // mb
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_MB);

           goto decode_done;      
           break;                 
                                  
         case 0x34:  // rfe
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_RFE);

           goto decode_done;      
           break;                 
                                  
         case 0x35:  // abort
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_ABORT);

           goto decode_done;      
           break;                 
                                  
         case 0x36:  // syscall
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_SYSCALL);

           goto decode_done;      
           break;                 
                                  
         case 0x37:  // printreg
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_PRINTREG);

           goto decode_done;      
           break;                 
                                  
         case 0x38:  // timer.start
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_START_TIMER);

           goto decode_done;      
           break;                 
                                  
         case 0x39:  // timer.stop
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_STOP_TIMER);

           goto decode_done;      
           break;                 
                                  
         case 0x3a:  // flush.bcast
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_FLUSH_BCAST);

           goto decode_done;      
           break;                 
                                  
         case 0x3b:  // line.wb
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_LINE_WB);

           goto decode_done;      
           break;                 
                                  
         case 0x3c:  // line.inv
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_LINE_INV);

           goto decode_done;      
           break;                 
                                  
         case 0x3d:  // line.flush
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_LINE_FLUSH);

           goto decode_done;      
           break;                 
                                  
         case 0x3e:  // tq.enq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_TQ_ENQUEUE);

           goto decode_done;      
           break;                 
                                  
         case 0x3f:  // tq.deq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_TQ_DEQUEUE);

           goto decode_done;      
           break;                 
                                  
         case 0x40:  // tq.loop
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_TQ_LOOP);

           goto decode_done;      
           break;                 
                                  
         case 0x41:  // tq.init
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_TQ_INIT);

           goto decode_done;      
           break;                 
                                  
         case 0x42:  // tq.end
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_TQ_END);

           goto decode_done;      
           break;                 
                                  
         case 0x43:  // fadd
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_FADD);

           goto decode_done;      
           break;                 
                                  
         case 0x44:  // fsub
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_FSUB);

           goto decode_done;      
           break;                 
                                  
         case 0x45:  // fmul
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_FMUL);

           goto decode_done;      
           break;                 
                                  
         case 0x46:  // frcp
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_FRCP);

           goto decode_done;      
           break;                 
                                  
         case 0x47:  // frsq
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_FRSQ);

           goto decode_done;      
           break;                 
                                  
         case 0x48:  // fabs
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_FABS);

           goto decode_done;      
           break;                 
                                  
         case 0x49:  // fmrs
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_FMRS);

           goto decode_done;      
           break;                 
                                  
         case 0x4a:  // fmov
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_FMOV);

           goto decode_done;      
           break;                 
                                  
         case 0x4b:  // fr2a
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(NULL_REG,
               instr->get_reg_t(), 
               NULL_REG,
               NULL_REG,
               instr->get_acc_d());
            instr->update_type(I_FR2A);

           goto decode_done;      
           break;                 
                                  
         case 0x4c:  // f2i
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_F2I);

           goto decode_done;      
           break;                 
                                  
         case 0x4d:  // i2f
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_I2F);

           goto decode_done;      
           break;                 
                                  
         case 0x4e:  // mul
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_MUL);

           goto decode_done;      
           break;                 
                                  
         case 0x4f:  // mul32
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_MUL32);

           goto decode_done;      
           break;                 
                                  
         case 0x50:  // mul16
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_MUL16);

           goto decode_done;      
           break;                 
                                  
         case 0x51:  // mul16.c
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_MUL16C);

           goto decode_done;      
           break;                 
                                  
         case 0x52:  // mul16.g
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_MUL16G);

           goto decode_done;      
           break;                 
                                  
         case 0x53:  // clz
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_CLZ);

           goto decode_done;      
           break;                 
                                  
         case 0x54:  // zext8
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_ZEXTB);

           goto decode_done;      
           break;                 
                                  
         case 0x55:  // sext8
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_SEXTB);

           goto decode_done;      
           break;                 
                                  
         case 0x56:  // zext16
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_ZEXTS);

           goto decode_done;      
           break;                 
                                  
         case 0x57:  // sext16
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               instr->get_reg_d());
            instr->update_type(I_SEXTS);

           goto decode_done;      
           break;                 
                                  
         case 0x58:  // vadd
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_VADD);

           goto decode_done;      
           break;                 
                                  
         case 0x59:  // vsub
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_VSUB);

           goto decode_done;      
           break;                 
                                  
         case 0x5a:  // vfadd
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_VFADD);

           goto decode_done;      
           break;                 
                                  
         case 0x5b:  // vfsub
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_VFSUB);

           goto decode_done;      
           break;                 
                                  
         case 0x5c:  // vfmul
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
               instr->get_reg_s(),
               instr->get_reg_d());
            instr->update_type(I_VFMUL);

           goto decode_done;      
           break;                 
                                  
         case 0x5d:  // sleep
           /* RigelSim-specific stuff for type 0 */
            instr->set_regs(instr->get_reg_t(), 
                IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SLEEP);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x1:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // addi
           /* RigelSim-specific stuff for type 1 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_ADDI);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // subi
           /* RigelSim-specific stuff for type 1 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SUBI);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // addiu
           /* RigelSim-specific stuff for type 1 */
            instr->set_regs(instr->get_reg_t(), 
               SIMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_ADDIU);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // subiu
           /* RigelSim-specific stuff for type 1 */
            instr->set_regs(instr->get_reg_t(), 
               SIMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_SUBIU);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x2:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // andi
           /* RigelSim-specific stuff for type 2 */
            instr->set_regs(instr->get_reg_t(), 
               SIMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_ANDI);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // ori
           /* RigelSim-specific stuff for type 2 */
            instr->set_regs(instr->get_reg_t(), 
               SIMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_ORI);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // xori
           /* RigelSim-specific stuff for type 2 */
            instr->set_regs(instr->get_reg_t(), 
               SIMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_XORI);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // beq
           /* RigelSim-specific stuff for type 2 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_BEQ);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x3:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // mvui
           /* RigelSim-specific stuff for type 3 */
            instr->set_regs(instr->get_reg_d(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_MVUI);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // jal
           /* RigelSim-specific stuff for type 3 */
            instr->set_regs(SIMM16_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_JAL);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // pref.l
           /* RigelSim-specific stuff for type 3 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_PREF_L);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // pref.nga
           /* RigelSim-specific stuff for type 3 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_PREF_NGA);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x4:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // bne
           /* RigelSim-specific stuff for type 4 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_BNE);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // pref.b.gc
           /* RigelSim-specific stuff for type 4 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_PREF_B_GC);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // pref.b.cc
           /* RigelSim-specific stuff for type 4 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_PREF_B_CC);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // pldw
           /* RigelSim-specific stuff for type 4 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_PAUSE_LDW);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x5:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // be
           /* RigelSim-specific stuff for type 5 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BE);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // bnz
           /* RigelSim-specific stuff for type 5 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BN);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // blt
           /* RigelSim-specific stuff for type 5 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BLT);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // bgt
           /* RigelSim-specific stuff for type 5 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BGT);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x6:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // ble
           /* RigelSim-specific stuff for type 6 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BLE);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // bge
           /* RigelSim-specific stuff for type 6 */
            instr->set_regs(instr->get_reg_t(), 
               NULL_REG,
               SIMM16_REG);
            instr->update_type(I_BGE);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // jmp
           /* RigelSim-specific stuff for type 6 */
            instr->set_regs(SIMM16_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_JMP);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x7:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 26) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // lj
           /* RigelSim-specific stuff for type 7 */
            instr->set_regs(SIMM16_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_LJ);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // ljl
           /* RigelSim-specific stuff for type 7 */
            instr->set_regs(SIMM16_REG, 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_LJL);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x8:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // bcast.i
           /* RigelSim-specific stuff for type 8 */
            instr->set_regs(instr->get_reg_d(), 
               NULL_REG,
               NULL_REG);
            instr->update_type(I_BCAST_INV);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // strtof
           /* RigelSim-specific stuff for type 8 */
            instr->set_regs(instr->get_reg_d(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_STRTOF);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // event
           /* RigelSim-specific stuff for type 8 */
            instr->set_regs(instr->get_reg_d(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_EVENT);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // prio
           /* RigelSim-specific stuff for type 8 */
            instr->set_regs(instr->get_reg_d(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_PRIO);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0x9:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // bcast.u
           /* RigelSim-specific stuff for type 9 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_BCAST_UPDATE);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // ldw
           /* RigelSim-specific stuff for type 9 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_LDW);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // stw
           /* RigelSim-specific stuff for type 9 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_STW);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // g.ldw
           /* RigelSim-specific stuff for type 9 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_GLDW);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0xa:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // g.stw
           /* RigelSim-specific stuff for type 10 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_GSTW);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // atom.dec
           /* RigelSim-specific stuff for type 10 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               instr->get_reg_d());
            instr->update_type(I_ATOMDEC);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // atom.inc
           /* RigelSim-specific stuff for type 10 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               instr->get_reg_d());
            instr->update_type(I_ATOMINC);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // atom.xchg
           /* RigelSim-specific stuff for type 10 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               instr->get_reg_d());
            instr->update_type(I_ATOMXCHG);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0xb:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 0) & 0xff ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // fmadd
           /* RigelSim-specific stuff for type 11 */
            instr->set_regs(instr->get_acc_s(),
               instr->get_reg_t(), 
               instr->get_reg_s(),
               NULL_REG,
               instr->get_acc_d());
            instr->update_type(I_FMADD);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // fmsub
           /* RigelSim-specific stuff for type 11 */
            instr->set_regs(instr->get_acc_s(),
               instr->get_reg_t(), 
               instr->get_reg_s(),
               NULL_REG,
               instr->get_acc_d());
            instr->update_type(I_FMSUB);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // fa2r
           /* RigelSim-specific stuff for type 11 */
            instr->set_regs(instr->get_acc_s(),
               NULL_REG,
               NULL_REG,
               instr->get_reg_d(),
               NULL_REG);
            instr->update_type(I_FA2R);

           goto decode_done;      
           break;                 
                                  
         default: { goto unknown_opcode;  } 
       }                                 
     }                                   
                                        
    /**************/                     
     case 0xc:                         
    /**************/                     
     {                                   
       uint32_t opcode = (instr_bits >> 16) & 0x3 ; 
       switch (opcode)                   
       {                                 
         case 0x0:  // vaddi
           /* RigelSim-specific stuff for type 12 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_VADDI);

           goto decode_done;      
           break;                 
                                  
         case 0x1:  // vsubi
           /* RigelSim-specific stuff for type 12 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_VSUBI);

           goto decode_done;      
           break;                 
                                  
         case 0x2:  // vldw
           /* RigelSim-specific stuff for type 12 */
            instr->set_regs(instr->get_reg_t(), 
               IMM16_REG,
               instr->get_reg_d());
            instr->update_type(I_VLDW);

           goto decode_done;      
           break;                 
                                  
         case 0x3:  // vstw
           /* RigelSim-specific stuff for type 12 */
            instr->set_regs(instr->get_reg_d(), 
               instr->get_reg_t(),
               SIMM16_REG);
            instr->update_type(I_VSTW);

           goto decode_done;      
           break;                 
                                  
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

#endif
